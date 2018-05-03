/*
 * Copyright (c) 2015-2018 Reed A. Cartwright
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

#pragma once
#ifndef DNG_PEELING_H
#define DNG_PEELING_H

#include <vector>
#include <utility>

#include <boost/range/algorithm/copy.hpp>
#include <boost/range/algorithm/fill.hpp>
#include <boost/range/algorithm/fill_n.hpp>

#include <dng/matrix.h>
#include <dng/genotyper.h>
#include <dng/detail/unit_test.h>

#include <boost/range/adaptor/sliced.hpp>

namespace dng {
namespace peel {

struct workspace_t {
    GenotypeArrayVector upper; // Holds P(~Descendent_Data & G=g)
    GenotypeArrayVector lower; // Holds P( Descendent_Data | G=g)
    ParentArrayVector super; // Holds P(~Descendent_Data & G=g) for parent nodes

    bool dirty_lower = false;
    double ln_scale = 0.0;
    size_t matrix_index = 0;

    // Temporary data used by some peeling ops
    TemporaryMatrix temp_buffer;

    // Information about the pedigree
    std::size_t num_nodes = 0;
    using node_range_t = std::pair<std::size_t, std::size_t>;
    node_range_t founder_nodes,
                 germline_nodes,
                 somatic_nodes,
                 library_nodes;

    std::vector<int> ploidies;

    template<typename Rng>
    static auto make_slice(Rng& rng, node_range_t n) -> boost::sliced_range<Rng> {
        return boost::adaptors::slice(rng, n.first, n.second);
    }

    template<typename Rng>
    auto make_founder_slice(Rng& rng) -> boost::sliced_range<Rng> {
        return make_slice(rng, founder_nodes);
    }

    template<typename Rng>
    auto make_germline_slice(Rng& rng) -> boost::sliced_range<Rng> {
        return make_slice(rng, germline_nodes);
    }

    template<typename Rng>
    auto make_somatic_slice(Rng& rng) -> boost::sliced_range<Rng> {
        return make_slice(rng, somatic_nodes);
    }

    template<typename Rng>
    auto make_library_slice(Rng& rng) -> boost::sliced_range<Rng> {
        return make_slice(rng, library_nodes);
    }

    // Resize the workspace to fit a pedigree with sz nodes
    void Resize(std::size_t sz) {
        num_nodes = sz;
        upper.resize(num_nodes);
        super.resize(num_nodes);
        lower.resize(num_nodes);
        for(auto && a: lower) {
            a.setOnes(10);
        }
        temp_buffer.resize(100, 1);
        dirty_lower = false;
    }

    // Cleanup after a backwards peeling algorithm
    void Cleanup() {
        for(auto && a : lower) {
            a.setOnes();
        }
        dirty_lower = false;
    }

    // Cleanup after a backwards peeling algorithm
    // Do not update libraries since they might be set in another operation
    void CleanupFast() {
        for(std::size_t n = 0; n < somatic_nodes.second; ++n) {
            lower[n].setOnes();
        }
        dirty_lower = false;
    }

    // Set the prior probability of the founders given the reference
    void SetGermline(const GenotypeArray &prior) {
        assert(founder_nodes.first <= founder_nodes.second);
        assert(founder_nodes.second <= germline_nodes.second);

        // Set the Upper of the Founder Nodes
        for(auto i = founder_nodes.first; i < founder_nodes.second; ++i) {
            upper[i] = prior;
        }
        // Set the lowers of any germline node
        for(auto i = founder_nodes.first; i < germline_nodes.second; ++i) {
            lower[i].setOnes(prior.size());
        }
    }

    void SetGermline(const GenotypeArray &diploid_prior, const GenotypeArray &haploid_prior) {
        assert(founder_nodes.first <= founder_nodes.second);
        assert(founder_nodes.second <= germline_nodes.second);
        
        // Set the Upper of the Founder Nodes
        for(auto i = founder_nodes.first; i < founder_nodes.second; ++i) {
            assert(ploidies[i] == 2 || ploidies[i] == 1);
            if(ploidies[i] == 2) {
                upper[i] = diploid_prior;
            } else {
                upper[i] = haploid_prior;
            }
        }
        // Set the lowers of any germline node
        for(auto i = founder_nodes.first; i < germline_nodes.second; ++i) {
            assert(ploidies[i] == 2 || ploidies[i] == 1);
             if(ploidies[i] == 2) {
                lower[i].setOnes(diploid_prior.size());
            } else {
                lower[i].setOnes(haploid_prior.size());  
            }           
        }
    }

    template<typename G, typename D, typename ...A>
    void CalculateGenotypeLikelihoods(const G& gt, const D& d, A&&... args) {
        ln_scale = 0.0;
        size_t u = 0;
        for(auto pos = library_nodes.first; pos < library_nodes.second; ++pos) {
            ln_scale += gt(d[u++], std::forward<A>(args)..., ploidies[pos], &lower[pos]);
        }
    }

    void ExpGenotypeLikelihoods() {
        for(auto pos = library_nodes.first; pos < library_nodes.second; ++pos) {
            lower[pos] = lower[pos].exp();
        }
    }

    void LogGenotypeLikelihoods() {
        for(auto pos = library_nodes.first; pos < library_nodes.second; ++pos) {
            lower[pos] = lower[pos].log();
        }
    }

    // Set the genotype likelihoods into the lower values of the library_nodes.
    // Scales the genotype likelihoods as needed.
    // Input: log-likelihood values
    template<typename D>
    void SetGenotypeLikelihoods(const D& d) {
        ln_scale = 0.0;
        size_t u = 0;
        for(auto pos = library_nodes.first; pos < library_nodes.second; ++pos,++u) {
            lower[pos].resize(d[u].size());
            boost::copy(d[u], lower[pos].data());
            double temp = lower[pos].maxCoeff();
            lower[pos] = (lower[pos]-temp).exp();
            ln_scale += temp;
        }
    }    

    // Copy genotype likelihoods into the lower values of the library_nodes
    template<typename D>
    void CopyGenotypeLikelihoods(const D& d) {
        ln_scale = 0.0;
        size_t u = 0;
        for(auto pos = library_nodes.first; pos < library_nodes.second; ++pos,++u) {
            lower[pos].resize(d[u].size());
            boost::copy(d[u], lower[pos].data());
        }
    }

    // Set all genotype likelihoods to 1
    void ClearGenotypeLikelihoods(size_t num_of_obs_alleles) {
        ln_scale = 0.0;
        size_t hap_sz = num_of_obs_alleles;
        size_t gt_sz = hap_sz*(hap_sz+1)/2;
        for(auto pos = library_nodes.first; pos < library_nodes.second; ++pos) {
            assert(ploidies[pos] == 1 || ploidies[pos] == 2);
            lower[pos].setOnes( (ploidies[pos] == 2) ? gt_sz : hap_sz );
        }
    }   
};

typedef std::vector<std::size_t> family_members_t;

// Basic peeling operations
void up(workspace_t &work, const family_members_t &family,
        const TransitionMatrixVector &mat);
void down(workspace_t &work, const family_members_t &family,
          const TransitionMatrixVector &mat);
void to_father(workspace_t &work, const family_members_t &family,
               const TransitionMatrixVector &mat);
void to_mother(workspace_t &work, const family_members_t &family,
               const TransitionMatrixVector &mat);
void to_child(workspace_t &work, const family_members_t &family,
              const TransitionMatrixVector &mat);

// Fast versions that can be used on "dirty" workspaces
void up_fast(workspace_t &work, const family_members_t &family,
             const TransitionMatrixVector &mat);
void down_fast(workspace_t &work, const family_members_t &family,
               const TransitionMatrixVector &mat);
void to_father_fast(workspace_t &work, const family_members_t &family,
                    const TransitionMatrixVector &mat);
void to_mother_fast(workspace_t &work, const family_members_t &family,
                    const TransitionMatrixVector &mat);
void to_child_fast(workspace_t &work, const family_members_t &family,
                   const TransitionMatrixVector &mat);

// Reverse versions that can be used for backwards algorithms
void up_reverse(workspace_t &work, const family_members_t &family,
                const TransitionMatrixVector &mat);
void down_reverse(workspace_t &work, const family_members_t &family,
                  const TransitionMatrixVector &mat);
void to_father_reverse(workspace_t &work, const family_members_t &family,
                       const TransitionMatrixVector &mat);
void to_mother_reverse(workspace_t &work, const family_members_t &family,
                       const TransitionMatrixVector &mat);
void to_child_reverse(workspace_t &work, const family_members_t &family,
                      const TransitionMatrixVector &mat);

typedef decltype(&down) function_t;

struct info_t {
    bool writes_lower;
    int writes_to;
};

enum struct Op { 
    UP=0, DOWN, TOFATHER, TOMOTHER, TOCHILD,
    UPFAST, DOWNFAST, TOFATHERFAST, TOMOTHERFAST,
    TOCHILDFAST,
    NUM // Total number of possible forward operations
};

constexpr info_t info[(int)Op::NUM] = {
    /* Up           */ {true,  0},
    /* Down         */ {false, 1},
    /* ToFather     */ {true,  0},
    /* ToMother     */ {true,  1},
    /* ToChild      */ {false, 2},
    /* UpFast       */ {true,  0},
    /* DownFast     */ {false, 1},
    /* ToFatherFast */ {true,  0},
    /* ToMotherFast */ {true,  1},
    /* ToChildFast  */ {false, 2}
};

// TODO: Write test case to check that peeling ops are in the right order.
constexpr function_t functions[(int)Op::NUM] = {
    &up, &down, &to_father, &to_mother, &to_child,
    &up_fast, &down_fast, &to_father_fast, &to_mother_fast,
    &to_child_fast
};

constexpr function_t reverse_functions[(int)Op::NUM] = {
    &up_reverse, &down_reverse, &to_father_reverse,
    &to_mother_reverse, &to_child_reverse,
    &up_reverse, &down_reverse, &to_father_reverse,
    &to_mother_reverse, &to_child_reverse
};

} // namespace dng::peel
} // namespace dng

#endif // DNG_PEELING_H
