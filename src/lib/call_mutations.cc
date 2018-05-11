/*
 * Copyright (c) 2016 Steven H. Wu
 * Copyright (c) 2016-2018 Reed A. Cartwright
 * Authors:  Steven H. Wu <stevenwu@asu.edu>
 *           Reed A. Cartwright <reed@cartwrig.ht>
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

#include <dng/call_mutations.h>

#include <boost/range/algorithm/transform.hpp>
#include <functional>
#include <iterator>

using namespace dng;

// helper function to simplify the code below
// Not optimized by any means, but reduces code mistakes
template<typename V>
V container_subtract(const V& a, const V& b) {
    V output;
    boost::transform(a, b, std::back_inserter(output),
        std::minus<typename V::value_type>());
    return output;
}

CallMutations::CallMutations(const RelationshipGraph &graph, params_t params)
        : Probability(graph, params) {

    // Create Special Transition Matrices
    zero_mutation_matrices_ = CreateMutationMatrices(0);
    one_mutation_matrices_ = CreateMutationMatrices(1);
    mean_mutation_matrices_ = CreateMutationMatrices(mutation::mean_t{});

    for(size_t j=0; j < oneplus_mutation_matrices_.size(); ++j) {
        oneplus_mutation_matrices_[j] = container_subtract(transition_matrices_[j],
            zero_mutation_matrices_[j]);
    }

    // Calculate P(one mutation) assuming no data and 2 obs alleles
    work_.matrix_index = 1;
    work_.ClearGenotypeLikelihoods(2);
    work_.SetGermline(DiploidPrior(2), HaploidPrior(2));
    one_mutation_prior_ = CalculateDNP().prob();

    // Calculate prior non-ref allele freq
    alt_freq_prior_ = HaploidPrior(2)(1);
}

Probability::logdiff_t CallMutations::CalculateMUTQ() {
    const int matrix_index = work_.matrix_index;
    // Now peel numerator
    double numerator = graph_.PeelForwards(work_, zero_mutation_matrices_[matrix_index]);
    // Calculate log P(Data ; model)
    double denominator = graph_.PeelForwards(work_, transition_matrices_[matrix_index]);
    
    return {numerator, denominator};
}

Probability::logdiff_t CallMutations::CalculateDNP() {
    const int matrix_index = work_.matrix_index;

    double ln_all = graph_.PeelForwards(work_, transition_matrices_[matrix_index]);

    double ln_zero = graph_.PeelForwards(work_, zero_mutation_matrices_[matrix_index]);
    graph_.PeelBackwards(work_, zero_mutation_matrices_[matrix_index]);
    double total = 0.0;
    for (size_t i = work_.founder_nodes.second; i < work_.num_nodes; ++i) {
        work_.temp_buffer = (work_.super[i].matrix() *
                                work_.lower[i].matrix().transpose()).array() *
                                one_mutation_matrices_[matrix_index][i].array();
        total += work_.temp_buffer.sum();
    }
    // total = P(1 mutation & D)/P(0 mutations & D) due to backwards algorithm
    ln_zero = ln_zero + log(total);
    return {ln_zero, ln_all};
}

// Returns true if a mutation was found and the record was modified
bool CallMutations::CalculateMutationStats(genotype::Mode mode, stats_t *stats) {
    const int matrix_index = work_.matrix_index;

    // We can't find mutations or variants if we have
    // no variation in the data
    if(matrix_index == 0 && quality_threshold() > 0.0) {
        return false;
    }

    auto mono = CalculateMONO(mode);
    double quality = mono.phred_score();
    if(quality < min_quality_) {
        return false;
    }
    double ln_nomut = PeelNoMutations();

    decltype(mono) nomut{ln_nomut, mono.right};
    double mutq = nomut.phred_score();

    if(!all_variants_ && mutq < min_quality_) {
        return false;
    }
    if(stats == nullptr) {
        return true;
    }

    stats->mutq = mutq;
    stats->lld = (mono.right + work_.ln_scale)/M_LN10;
    stats->quality = quality;
    stats->ln_mono = mono.left;
    stats->ln_zero = nomut.left;
    stats->ln_all  = nomut.right;

    // Since we just called PeelNoMutations(), can call this and
    // skip peeling forward again
    CalculateSingleMutationStats(false, stats);

    stats->denovo = (mutq >= min_quality_ && min_quality_ > 0);

    graph_.PeelForwards(work_, transition_matrices_[matrix_index]);
    graph_.PeelBackwards(work_, transition_matrices_[matrix_index]);

    // Genotype Likelihoods for Libraries
    size_t num_libraries = work_.library_nodes.second-work_.library_nodes.first;
    stats->genotype_likelihoods.resize(num_libraries);
    for (size_t u = 0; u < num_libraries; ++u) {
        size_t pos = work_.library_nodes.first + u;
        stats->genotype_likelihoods[u] = work_.lower[pos];
    }

    // Posterior probabilities and best genotypes for all nodes
    stats->posterior_probabilities.resize(work_.num_nodes);
    stats->best_genotypes.resize(work_.num_nodes);
    stats->genotype_qualities.resize(work_.num_nodes);

    for (size_t i = 0; i < work_.num_nodes; ++i) {
        stats->posterior_probabilities[i] = (work_.upper[i] * work_.lower[i]);
        stats->posterior_probabilities[i] /= stats->posterior_probabilities[i].sum();

        size_t pos;
        double d = stats->posterior_probabilities[i].maxCoeff(&pos);
        stats->best_genotypes[i] = pos;
        stats->genotype_qualities[i] = dng::utility::lphred1m<int>(d, 255);
    }

    // Expected Number of Mutations
    stats->mutx = 0.0;
    for(size_t i = work_.founder_nodes.second; i < work_.num_nodes; ++i) {
        stats->mutx += (work_.super[i] * (mean_mutation_matrices_[matrix_index][i] *
                                      work_.lower[i].matrix()).array()).sum();
    }

    // Probability of at least 1 mutation at a node, given that there is at least 1 mutation in the graph
    stats->node_mutp.resize(work_.num_nodes);
    for(size_t i = work_.founder_nodes.first; i < work_.founder_nodes.second; ++i) {
        stats->node_mutp[i] = 0.0;
    }

    double mutp = utility::unphred1m(mutq);
    for (size_t i = work_.founder_nodes.second; i < work_.num_nodes; ++i) {
        double temp = (work_.super[i] * (oneplus_mutation_matrices_[matrix_index][i] *
                                          work_.lower[i].matrix()).array()).sum();
        stats->node_mutp[i] = temp/mutp;
    }

    // Probability of Exactly One Mutation
    
    return true;
}

// NOTE: if stats is not nullptr, this assumes that CalculateMUP has been run on it
bool CallMutations::CalculateSingleMutationStats(bool peel_forward, stats_t *stats) {
    // Probability of Exactly One Mutation
    assert(stats != nullptr);
    // We will peel again, but this time with the zero matrix
    const int matrix_index = work_.matrix_index;

    double ln_zero;
    if(peel_forward) {
        ln_zero = graph_.PeelForwards(work_, zero_mutation_matrices_[matrix_index]);
    } else {
        ln_zero = stats->ln_zero;
    }
    graph_.PeelBackwards(work_, zero_mutation_matrices_[matrix_index]);

    double total = 0.0, max_coeff = -1.0;
    size_t dn_row = 0, dn_col = 0, dn_location = 0;

    stats->node_dnp.resize(work_.num_nodes);
    for(size_t i = work_.founder_nodes.first; i < work_.founder_nodes.second; ++i) {
        stats->node_dnp[i] = 0.0;
    }
    for (size_t i = work_.founder_nodes.second; i < work_.num_nodes; ++i) {
        work_.temp_buffer = (work_.super[i].matrix() *
                                work_.lower[i].matrix().transpose()).array() *
                                one_mutation_matrices_[matrix_index][i].array();
        std::size_t row, col;
        double temp = work_.temp_buffer.maxCoeff(&row, &col);
        if (temp > max_coeff) {
            max_coeff = temp;
            dn_row = row;
            dn_col = col;
            dn_location = i;
        }
        temp = work_.temp_buffer.sum();
        stats->node_dnp[i] = temp;
        total += temp;
    }
    for (size_t i = work_.founder_nodes.second; i < work_.num_nodes; ++i) {
        stats->node_dnp[i] = stats->node_dnp[i]/total;
    }
    // total = P(1 mutation & D)/P(0 mutations & D) due to backwards algorithm
    // thus P(1 mutation | D) = total*P(0 mutations & D)/P(D)

    stats->dnp = total*exp(stats->ln_zero - stats->ln_all);

    // NOTE: if site doesn't have a known reference, this will be biased
    // stats->lld1 = log10(total) + (stats->ln_zero+work_.ln_scale)/M_LN10 - log10_one_mutation_[matrix_index];

    stats->dnq = dng::utility::lphred1m<int>(max_coeff/total, 255);
    stats->dnl = dn_location;
    stats->dnt_row = dn_row;
    stats->dnt_col = dn_col;

    stats->dnp_min = one_mutation_prior_;
    stats->af_min = alt_freq_prior_;

    return (stats->dnp > one_mutation_prior_);
}
