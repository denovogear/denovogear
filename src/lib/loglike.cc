/*
 * Copyright (c) 2016 Reed A. Cartwright
 * Authors:  Reed A. Cartwright <reed@cartwrig.ht>
 *
 * This file is part of DeNovoGear.
 *
 * DeNovoGear is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <iosfwd>

#include <iostream>
#include <iomanip>
#include <ctime>
#include <sstream>
#include <string>

#include <boost/range/iterator_range.hpp>
#include <boost/range/algorithm/replace.hpp>
#include <boost/range/algorithm/max_element.hpp>

#include <boost/algorithm/string.hpp>

#include <dng/task/loglike.h>
#include <dng/pedigree.h>
#include <dng/fileio.h>
#include <dng/pileup.h>
#include <dng/read_group.h>
#include <dng/likelihood.h>
#include <dng/seq.h>
#include <dng/utility.h>
#include <dng/hts/bcf.h>
#include <dng/hts/extra.h>
#include <dng/vcfpileup.h>
#include <dng/mutation.h>
#include <dng/stats.h>
#include <dng/io/utility.h>
#include <dng/io/fasta.h>

#include <htslib/faidx.h>
#include <htslib/khash.h>

#include "version.h"

using namespace std;
using namespace dng;
using namespace dng::task;

// Helper function for writing the vcf header information
void cout_add_header_text(task::LogLike::argument_type &arg) {
    using namespace std;
    string line{"##DeNovoGearCommandLine=<ID=dng-loglike,Version="
                PACKAGE_VERSION ","};
    line += utility::vcf_timestamp();
    line += ",CommandLineOptions=\"";

#define XM(lname, sname, desc, type, def) \
    line += utility::vcf_command_line_text(XS(lname),arg.XV(lname)) + ' ';
#   include <dng/task/loglike.xmh>
#undef XM
    for(auto && a : arg.input) {
        line += a + ' ';
    }

    line.pop_back();
    line += "\">";

    std::cout << line << "\n";
 }

class LogProbability {
public:
    struct params_t {
        double theta;
        std::array<double, 4> nuc_freq;
        double ref_weight;

        dng::genotype::DirichletMultinomialMixture::params_t params_a;
        dng::genotype::DirichletMultinomialMixture::params_t params_b;
    };

    struct stats_t {
        double log_data;
        double log_scale;        
    };

    LogProbability(const Pedigree &pedigree, params_t params);

    stats_t operator()(const std::vector<depth_t> &depths, int ref_index);

protected:
    const dng::Pedigree &pedigree_;

    params_t params_;

    dng::peel::workspace_t work_;


    dng::TransitionVector full_transition_matrices_;

    // Model genotype likelihoods as a mixture of two dirichlet multinomials
    // TODO: control these with parameters
    dng::genotype::DirichletMultinomialMixture genotype_likelihood_;

    dng::GenotypeArray genotype_prior_[5]; // Holds P(G | theta)
};

// Sub-tasks
int process_bam(LogLike::argument_type &arg);
int process_ad(LogLike::argument_type &arg);

// The main loop for dng-loglike application
// argument_type arg holds the processed command line arguments
int task::LogLike::operator()(task::LogLike::argument_type &arg) {
    using utility::FileCat;
    using utility::FileCatSet;
    // if input is empty default to stdin.
    if(arg.input.empty()) {
        arg.input.emplace_back("-");
    }

    // Check that all input formats are of same category
    auto it = arg.input.begin();
    FileCat mode = utility::input_category(*it, FileCat::Sequence|FileCat::Pileup, FileCat::Sequence);
    for(++it; it != arg.input.end(); ++it) {
        if(utility::input_category(*it, FileCat::Sequence|FileCat::Pileup, FileCat::Sequence) != mode) {
            throw runtime_error("Argument error: mixing sam/bam/cram and tad/ad input files is not supported.");
        }
    }
    // Execute sub tasks based on input type
    if(mode == FileCat::Pileup) {
        return process_ad(arg);
    }
    return process_bam(arg);
}


int process_bam(LogLike::argument_type &arg) {
    using namespace hts::bcf;

    // Parse pedigree from file
    io::Pedigree ped = io::parse_pedigree(arg.ped);

    // Open Reference
    io::Fasta reference{arg.fasta.c_str()};

    // Parse Nucleotide Frequencies
    std::array<double, 4> freqs = utility::parse_nuc_freqs(arg.nuc_freqs);

    // quality thresholds
    int min_qual = arg.min_basequal;

    // Print header to output
    cout_add_header_text(arg);

    // replace arg.region with the contents of a file if needed
    io::at_slurp(arg.region);

    // Open input files
    dng::ReadGroups rgs;
    vector<hts::bam::File> bamdata;
    for(auto && str : arg.input) {
        bamdata.emplace_back(str.c_str(), "r", arg.fasta.c_str(), arg.min_mapqual, arg.header.c_str());
        if(!bamdata.back().is_open()) {
            throw std::runtime_error("unable to open input file '" + str + "'.");
        }
        // add regions
        bamdata.back().regions(regions::bam_parse(arg.region,bamdata.back()));
        // Add each genotype/sample column
        rgs.ParseHeaderText(bamdata, arg.rgtag);
    }

    // Construct peeling algorithm from parameters and pedigree information
    dng::Pedigree pedigree;
    if(!pedigree.Construct(ped, rgs, arg.mu, arg.mu_somatic, arg.mu_library)) {
        throw std::runtime_error("Unable to construct peeler for pedigree; "
                                 "possible non-zero-loop pedigree.");
    }
    if(arg.gamma.size() < 2) {
        throw std::runtime_error("Unable to construct genotype-likelihood model; "
                                 "Gamma needs to be specified at least twice to change model from default.");
    }

    for(auto && line : pedigree.BCFHeaderLines()) {
        std:cout << line << "\n";
    }

    LogProbability calculate (pedigree,
        { arg.theta, freqs, arg.ref_weight, arg.gamma[0], arg.gamma[1] } );

    // Pileup data
    std::vector<depth_t> read_depths(rgs.libraries().size());

    const size_t num_nodes = pedigree.num_nodes();
    const size_t library_start = pedigree.library_nodes().first;

    const int min_basequal = arg.min_basequal;
    auto filter_read = [min_basequal](
    dng::BamPileup::data_type::value_type::const_reference r) -> bool {
        return (r.is_missing
        || r.qual.first[r.pos] < min_basequal
        || seq::base_index(r.aln.seq_at(r.pos)) >= 4);
    };

    // Treat sequence_data and variant data separately
    dng::stats::ExactSum sum_data;
    dng::stats::ExactSum sum_scale;
    const bam_hdr_t *h = bamdata[0].header();
    dng::BamPileup mpileup{rgs.groups(), arg.min_qlen};
    mpileup(bamdata, [&](const dng::BamPileup::data_type & data, location_t loc) {
        // Calculate target position and fetch sequence name
        int contig = utility::location_to_contig(loc);
        int position = utility::location_to_position(loc);

        // Calculate reference base
        assert(0 <= contig && contig < h->n_targets);
        char ref_base = reference.FetchBase(h->target_name[contig],position);
        size_t ref_index = seq::char_index(ref_base);

        // reset all depth counters
        read_depths.assign(read_depths.size(), {});

        // pileup on read counts
        for(std::size_t u = 0; u < data.size(); ++u) {
            for(auto && r : data[u]) {
                if(filter_read(r)) {
                    continue;
                }
                std::size_t base = seq::base_index(r.aln.seq_at(r.pos));
                assert(read_depths[rgs.library_from_id(u)].counts[ base ] < 65535);

                read_depths[rgs.library_from_id(u)].counts[ base ] += 1;
            }
        }
        auto loglike = calculate(read_depths, ref_index);
        sum_data += loglike.log_data;
        sum_scale += loglike.log_scale;
    });
    std::cout << "log_hidden\tlog_observed\n";
    std::cout << setprecision(16) << sum_data.result() << "\t" << sum_scale.result() << "\n";

    return EXIT_SUCCESS;
}

int process_ad(LogLike::argument_type &arg) {
    // Parse pedigree from file
    io::Pedigree ped = io::parse_pedigree(arg.ped);

    // Open Reference
    io::Fasta reference{arg.fasta.c_str()};

    // Parse Nucleotide Frequencies
    std::array<double, 4> freqs = utility::parse_nuc_freqs(arg.nuc_freqs);

    // quality thresholds
    int min_qual = arg.min_basequal;

    // Print header to output
    cout_add_header_text(arg);

    // replace arg.region with the contents of a file if needed
    io::at_slurp(arg.region);

    // Open input files
    if(arg.input.size() != 1) {
        throw std::runtime_error("Argument Error: can only process one ad/tad file at a time.");
    }
    
    io::Ad input{arg.input[0], std::ios_base::in};
    if(!input) {
        throw std::runtime_error("Argument Error: unable to open input file '" + arg.input[0] + "'.");
    }
    if(input.ReadHeader() == 0) {
        throw std::runtime_error("Argument Error: unable to read header from '" + input.path() + "'.");
    }
    // Construct ReadGroups
    dng::ReadGroups rgs;
    rgs.ParseLibraries(input.libraries());

    // Construct peeling algorithm from parameters and pedigree information
    dng::Pedigree pedigree;
    if(!pedigree.Construct(ped, rgs, arg.mu, arg.mu_somatic, arg.mu_library)) {
        throw std::runtime_error("Unable to construct peeler for pedigree; "
                                 "possible non-zero-loop pedigree.");
    }
    if(arg.gamma.size() < 2) {
        throw std::runtime_error("Unable to construct genotype-likelihood model; "
                                 "Gamma needs to be specified at least twice to change model from default.");
    }

    for(auto && line : pedigree.BCFHeaderLines()) {
        std:cout << line << "\n";
    }

    pileup::AlleleDepths line;
    line.data().reserve(4*input.libraries().size());
    while(input.Read(&line)) {
        
    }    


    return EXIT_SUCCESS;
}

LogProbability::LogProbability(const Pedigree &pedigree,
                             params_t params) :
    pedigree_(pedigree),
    params_(params), genotype_likelihood_{params.params_a, params.params_b},
    work_(pedigree.CreateWorkspace()) {

    using namespace dng;

    // Use a parent-independent mutation model, which produces a
    // beta-binomial
    genotype_prior_[0] = population_prior(params_.theta, params_.nuc_freq, {params_.ref_weight, 0, 0, 0});
    genotype_prior_[1] = population_prior(params_.theta, params_.nuc_freq, {0, params_.ref_weight, 0, 0});
    genotype_prior_[2] = population_prior(params_.theta, params_.nuc_freq, {0, 0, params_.ref_weight, 0});
    genotype_prior_[3] = population_prior(params_.theta, params_.nuc_freq, {0, 0, 0, params_.ref_weight});
    genotype_prior_[4] = population_prior(params_.theta, params_.nuc_freq, {0, 0, 0, 0});

    // Calculate mutation expectation matrices
    full_transition_matrices_.assign(work_.num_nodes, {});
 
    for(size_t child = 0; child < work_.num_nodes; ++child) {
        auto trans = pedigree.transitions()[child];
        if(trans.type == Pedigree::TransitionType::Germline) {
            auto dad = f81::matrix(trans.length1, params_.nuc_freq);
            auto mom = f81::matrix(trans.length2, params_.nuc_freq);

            full_transition_matrices_[child] = meiosis_diploid_matrix(dad, mom);
        } else if(trans.type == Pedigree::TransitionType::Somatic ||
                  trans.type == Pedigree::TransitionType::Library) {
            auto orig = f81::matrix(trans.length1, params_.nuc_freq);

            full_transition_matrices_[child] = mitosis_diploid_matrix(orig);
        } else {
            full_transition_matrices_[child] = {};
        }
    }
}

// returns 'log10 P(Data ; model)-log10 scale' and log10 scaling.
LogProbability::stats_t LogProbability::operator()(const std::vector<depth_t> &depths,
                               int ref_index) {
    using namespace std;

    // calculate genotype likelihoods and store in the lower library vector
    double scale = 0.0, stemp;
    for(std::size_t u = 0; u < depths.size(); ++u) {
        std::tie(work_.lower[work_.library_nodes.first + u], stemp) =
            genotype_likelihood_(depths[u], ref_index);
        scale += stemp;
    }

    // Set the prior probability of the founders given the reference
    work_.SetFounders(genotype_prior_[ref_index]);

    // Calculate log P(Data ; model)
    double logdata = pedigree_.PeelForwards(work_, full_transition_matrices_);

    return {logdata/M_LN10, scale/M_LN10};
}
