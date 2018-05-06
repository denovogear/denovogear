/*
 * Copyright (c) 2014-2017 Reed A. Cartwright
 * Copyright (c) 2016 Steven H. Wu
 * Authors:  Reed A. Cartwright <reed@cartwrig.ht>
 *           Steven H. Wu <stevenwu@asu.edu>
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
#include <dng/relationship_graph.h>

#include <queue>

#include <boost/graph/biconnected_components.hpp>
#include <boost/graph/connected_components.hpp>
#include <boost/range/algorithm/find.hpp>
#include <boost/range/adaptor/reversed.hpp>

using namespace dng;
using namespace dng::detail;
using namespace dng::detail::graph;

namespace {
vertex_t parse_pedigree_table(Graph &pedigree_graph, const dng::Pedigree &pedigree,
        bool normalize_somatic_trees);
void add_libraries_to_graph(Graph &pedigree_graph, const libraries_t &libs);
void update_edge_lengths(Graph &pedigree_graph,
        double mu_meiotic, double mu_somatic, double mu_library);
void simplify_pedigree(Graph &pedigree_graph);

void prune_pedigree(Graph &pedigree_graph, InheritanceModel model);

void prefix_vertex_labels(Graph &pedigree_graph);
} // namespace


constexpr vertex_t DUMMY_INDEX{0};
constexpr vertex_t NULL_INDEX{DUMMY_INDEX-1};

const std::pair<std::string, InheritanceModel> inheritance_keys[] = {
    {"", InheritanceModel::Unknown},
    {"AUTOSOMAL", InheritanceModel::Autosomal},
    {"MATERNAL", InheritanceModel::Maternal},
    {"PATERNAL", InheritanceModel::Paternal},
    {"X-LINKED", InheritanceModel::XLinked},
    {"Y-LINKED", InheritanceModel::YLinked},
    {"W-LINKED", InheritanceModel::WLinked},
    {"Z-LINKED", InheritanceModel::ZLinked},
    {"MITOCHONDRIAL", InheritanceModel::Maternal},
    {"XLINKED", InheritanceModel::XLinked},
    {"YLINKED", InheritanceModel::YLinked},
    {"WLINKED", InheritanceModel::WLinked},
    {"ZLINKED", InheritanceModel::ZLinked}
};

dng::InheritanceModel dng::inheritance_model(const std::string &pattern) {
    InheritanceModel model = dng::utility::key_switch_tuple(pattern, inheritance_keys,
                                                inheritance_keys[0]).second;
    if (model == InheritanceModel::Unknown){
        throw std::invalid_argument("Inheritance model '" + pattern
            + "' is not supported. Supported values are: "
            "[autosomal, mitochondrial, paternal, x-linked, y-linked, w-linked, z-linked]");
    }
    return model;
}

std::string dng::to_string(InheritanceModel model) {
    if(model != InheritanceModel::Unknown) {
        for(auto &&a : inheritance_keys) {
            if(std::get<1>(a) != model) {
                continue;
            }
            return std::get<0>(a);
        }
    }
    throw std::invalid_argument("Unable to convert model '" + std::to_string((int)model)
            + "' to string.");
    return {};   
}

/*
RULES FOR LINKING READ GROUPS TO PEOPLE.

0) Read all read groups in bam files. Map RG to Library to Sample.

1) If tissue information is present, build a tissue tree connecting samples
   to zygotic genotypes.  Build mutation matrices for every unique branch
   length.

2) If no tissue information is present in pedigree, check to see if there is a
   sample with the same label as the individual.  If so, connect that sample to
   the individual with a somatic branch of length 1.

3) If a sample has multiple libraries, connect the libraries to sample with a
   library branch.

4) If a library has multiple read-groups, concat the read-groups.

*/

bool dng::RelationshipGraph::Construct(const Pedigree& pedigree,
        const libraries_t& libs, double mu, double mu_somatic, double mu_library,
        bool normalize_somatic_trees) {
    return Construct(pedigree, libs, InheritanceModel::Autosomal, mu,
                     mu_somatic, mu_library, normalize_somatic_trees);
}

bool dng::RelationshipGraph::Construct(const Pedigree& pedigree,
        const libraries_t& libs, InheritanceModel model,
        double mu, double mu_somatic, double mu_library,
        bool normalize_somatic_trees) {
    using namespace std;

    inheritance_model_ = model;

    // Construct a boost::graph of the pedigree and somatic information
    Graph pedigree_graph;

    first_founder_ = 0;
    first_somatic_ = parse_pedigree_table(pedigree_graph, pedigree,
        normalize_somatic_trees);
 
    // Find first germline node that is not a child of DUMMY_INDEX
    for(first_nonfounder_ = 1; first_nonfounder_ < first_somatic_; ++first_nonfounder_) {
        auto id = edge(DUMMY_INDEX, first_nonfounder_, pedigree_graph);
        if(!id.second)
            break;
    }
    // Disconnect founders from DUMMY_INDEX
    clear_vertex(DUMMY_INDEX, pedigree_graph);

    // Connect somatic to libraries and save the names of the libraries that
    // were successfully connected.
    first_library_ = num_vertices(pedigree_graph);
    
    add_libraries_to_graph(pedigree_graph, libs);

    num_nodes_ = num_vertices(pedigree_graph);

    // Multiply edge lengths by mutation rates
    update_edge_lengths(pedigree_graph, mu, mu_somatic, mu_library);

    // Remove edges that are non-informative
    simplify_pedigree(pedigree_graph);

    // Prune pedigree
    prune_pedigree(pedigree_graph, inheritance_model_);

    // Apply prefixes to vertex labels to identify germline, somatic, and library nodes
    prefix_vertex_labels(pedigree_graph);

    // Convert vertices in the graph into nodes for peeling operations
    std::vector<size_t> node_ids = ConstructNodes(pedigree_graph);

    family_labels_t family_labels; // (num_families);
    std::vector<vertex_t> pivots;  // (num_families, dummy_index);
    CreateFamiliesInfo(pedigree_graph, family_labels, pivots);

    CreatePeelingOps(pedigree_graph, node_ids, family_labels, pivots);
    ConstructPeelingMachine();

    //PrintMachine(cerr);
    return true;
}

void dng::RelationshipGraph::ConstructPeelingMachine() {
    using namespace dng::peel;
    peeling_functions_.clear();
    peeling_functions_ops_.clear();
    peeling_reverse_functions_.clear();
    peeling_functions_.reserve(peeling_ops_.size());
    peeling_functions_ops_.reserve(peeling_ops_.size());
    peeling_reverse_functions_.reserve(peeling_ops_.size());
    std::vector<std::size_t> lower_written(num_nodes_, -1);
    for(std::size_t i = 0 ; i < peeling_ops_.size(); ++i) {
        peel::Op a = peeling_ops_[i];
        const auto &fam = family_members_[i];
        int b = (int)a;
        auto w = fam[info[b].writes_to];
        bool do_fast = false;
        switch(a) {
        case Op::DOWN:
            // If the lower of the parent has never been written to, we can use the fast version
            do_fast = (lower_written[fam[0]] != -1);
            break;
        case Op::TOCHILD:
            // If the we only have one child, we can use the fast version
            do_fast = (fam.size() == 3);
            break;
        case Op::TOMOTHER:
        case Op::TOFATHER:
        case Op::UP:
            // If the lower of the destination has never been written to, we can use the fast version
            do_fast = (lower_written[w] == -1);
            break;
        default:
            assert(false); // should never get here
            break;
        }
        b = do_fast ? (int)Op::UPFAST + b : b;

        peeling_functions_ops_.push_back(static_cast<peel::Op>(b));
        peeling_functions_.push_back(functions[b]);
        peeling_reverse_functions_.push_back(reverse_functions[b]);

        // If the operation writes to a lower value, make note of it
        if(info[b].writes_lower) {
            lower_written[w] = i;
        }
    }
}

std::vector<std::string> dng::RelationshipGraph::BCFHeaderLines() const {
    using namespace std;
    vector<string> ret = {
        "##META=<ID=OriginalMR,Type=Float,Number=1,Description=\"Mutation rate\">",
        "##META=<ID=FatherMR,Type=Float,Number=1,Description=\"Paternal mutation rate\">",
        "##META=<ID=MotherMR,Type=Float,Number=1,Description=\"Maternal mutation rate\">",
        "##META=<ID=Ploidy,Type=Integer,Number=1,Description=\"Ploidy\">",
        "##META=<ID=Germline,Type=Flag,Number=0,Description=\"Contains germline events\">",
        "##META=<ID=Somatic,Type=Flag,Number=0,Description=\"Contains somatic events\">",
        "##META=<ID=Library,Type=Flag,Number=0,Description=\"Contains library events\">"
    };

    for(size_t child = 0; child != transitions_.size(); ++child) {
        auto & parents = transitions_[child];
        string line;
        switch(parents.type) {
        case TransitionType::Trio:
            line += "##PEDIGREE=<Child=" + labels_[child];
            line += ",Father=" + labels_[parents.parent1];
            line += ",Mother=" + labels_[parents.parent2];
            line += ",FatherMR=" + utility::to_pretty(parents.length1);
            line += ",MotherMR=" + utility::to_pretty(parents.length2);
            break;
        case TransitionType::Pair:
            line += "##PEDIGREE=<Derived=" + labels_[child];
            line += ",Original=" + labels_[parents.parent1];
            line += ",OriginalMR=" + utility::to_pretty(parents.length1);
            break;
        case TransitionType::Founder:
        default:
            break;    
        }
        line += ",Ploidy=" + utility::to_pretty(ploidies_[child]);
        if(parents.is_germline) {
            line += ",Germline=1";
        }
        if(parents.is_somatic) {
            line += ",Somatic=1";
        }
        if(parents.is_library) {
            line += ",Library=1";
        }        
        ret.push_back(line + ">");
    }
    return ret;
}

namespace {

void prune_pedigree_autosomal(Graph &pedigree_graph);
void prune_pedigree_ylinked(Graph &pedigree_graph);
void prune_pedigree_xlinked(Graph &pedigree_graph);
void prune_pedigree_wlinked(Graph &pedigree_graph);
void prune_pedigree_zlinked(Graph &pedigree_graph);
void prune_pedigree_maternal(Graph &pedigree_graph);
void prune_pedigree_paternal(Graph &pedigree_graph);

void prune_pedigree(Graph &pedigree_graph, InheritanceModel model) {
    switch(model) {
    case InheritanceModel::Autosomal:
        return prune_pedigree_autosomal(pedigree_graph);
    case InheritanceModel::YLinked:
        return prune_pedigree_ylinked(pedigree_graph);
    case InheritanceModel::XLinked:
        return prune_pedigree_xlinked(pedigree_graph);
    case InheritanceModel::WLinked:
        return prune_pedigree_wlinked(pedigree_graph);
    case InheritanceModel::ZLinked:
        return prune_pedigree_zlinked(pedigree_graph);
    case InheritanceModel::Maternal:
        return prune_pedigree_maternal(pedigree_graph);
    case InheritanceModel::Paternal:
        return prune_pedigree_paternal(pedigree_graph);
    default:
        throw std::invalid_argument("Selected inheritance model invalid or not implemented yet.");
    }
}

void prune_pedigree_autosomal(Graph &pedigree_graph) {
    auto ploidies  = get(boost::vertex_ploidy, pedigree_graph);
    auto vertex_range = boost::make_iterator_range(vertices(pedigree_graph));
    for (vertex_t v : vertex_range) {
        ploidies[v] = 2;
    }
}

void prune_pedigree_ylinked(Graph &pedigree_graph) {
    auto sexes  = get(boost::vertex_sex, pedigree_graph);
    auto ploidies  = get(boost::vertex_ploidy, pedigree_graph);

    auto vertex_range = boost::make_iterator_range(vertices(pedigree_graph));
    for (vertex_t v : vertex_range) {
        switch(sexes[v]) {
        case Sex::Female:
            clear_vertex(v,pedigree_graph);
            ploidies[v] = 0;
            break;
        case Sex::Male:
            ploidies[v] = 1;
            break;
        case Sex::Unknown:
        default:
            if(out_degree(v, pedigree_graph) != 0) {
                throw std::invalid_argument("Y-linked inheritance requires every individual to have a known sex.");
            }
        }
    }
}

void prune_pedigree_xlinked(Graph &pedigree_graph) {
    auto is_y = [&](edge_t e) -> bool {
        if(get(boost::edge_type, pedigree_graph, e) != EdgeType::Paternal) {
            return false;
        }
        vertex_t a = source(e, pedigree_graph);
        vertex_t b = target(e, pedigree_graph);
        return (get(boost::vertex_sex, pedigree_graph, std::max(a,b)) == Sex::Male);
    };
    remove_edge_if(is_y, pedigree_graph);

    auto sexes  = get(boost::vertex_sex, pedigree_graph);
    auto ploidies  = get(boost::vertex_ploidy, pedigree_graph);
    auto vertex_range = boost::make_iterator_range(vertices(pedigree_graph));
    for (vertex_t v : vertex_range) {
        switch(sexes[v]) {
        case Sex::Female:
            ploidies[v] = 2;
            break;
        case Sex::Male:
            ploidies[v] = 1;
            break;
        case Sex::Unknown:
        default:
            if(out_degree(v, pedigree_graph) != 0) {
                throw std::runtime_error("X-linked inheritance requires every individual to have a known sex.");
            }
        }
    }
}

void prune_pedigree_wlinked(Graph &pedigree_graph) {
    auto sexes  = get(boost::vertex_sex, pedigree_graph);
    auto ploidies  = get(boost::vertex_ploidy, pedigree_graph);

    auto vertex_range = boost::make_iterator_range(vertices(pedigree_graph));
    for (vertex_t v : vertex_range) {
        switch(sexes[v]) {
        case Sex::Male:
            clear_vertex(v,pedigree_graph);
            ploidies[v] = 0;
            break;
        case Sex::Female:
            ploidies[v] = 1;
            break;
        case Sex::Unknown:
        default:
            if(out_degree(v, pedigree_graph) != 0) {
                throw std::runtime_error("W-linked inheritance requires every individual to have a known sex.");
            }
        }
    }
}

void prune_pedigree_zlinked(Graph &pedigree_graph) {
    auto is_w = [&](edge_t e) -> bool {
        if(get(boost::edge_type, pedigree_graph, e) != EdgeType::Maternal) {
            return false;
        }
        vertex_t a = source(e, pedigree_graph);
        vertex_t b = target(e, pedigree_graph);
        return (get(boost::vertex_sex, pedigree_graph, std::max(a,b)) == Sex::Female);
    };
    remove_edge_if(is_w, pedigree_graph);

    auto sexes  = get(boost::vertex_sex, pedigree_graph);
    auto ploidies  = get(boost::vertex_ploidy, pedigree_graph);
    auto vertex_range = boost::make_iterator_range(vertices(pedigree_graph));
    for (vertex_t v : vertex_range) {
        switch(sexes[v]) {
        case Sex::Male:
            ploidies[v] = 2;
            break;
        case Sex::Female:
            ploidies[v] = 1;
            break;
        case Sex::Unknown:
        default:
            if(out_degree(v, pedigree_graph) != 0) {
                throw std::runtime_error("Z-linked inheritance requires every individual to have a known sex.");
            }
        }
    }
}

void prune_pedigree_maternal(Graph &pedigree_graph) {
    auto is_p = [&](edge_t e) -> bool {
        auto edge_type = get(boost::edge_type, pedigree_graph, e);
        return (edge_type == EdgeType::Paternal || edge_type == EdgeType::Spousal);
    };
    remove_edge_if(is_p, pedigree_graph);

    auto ploidies  = get(boost::vertex_ploidy, pedigree_graph);
    auto vertex_range = boost::make_iterator_range(vertices(pedigree_graph));
    for (vertex_t v : vertex_range) {
        ploidies[v] = 1;
    }
}

void prune_pedigree_paternal(Graph &pedigree_graph) {
    auto is_m = [&](edge_t e) -> bool {
        auto edge_type = get(boost::edge_type, pedigree_graph, e);
        return (edge_type == EdgeType::Maternal || edge_type == EdgeType::Spousal);
    };
    remove_edge_if(is_m, pedigree_graph);

    auto ploidies  = get(boost::vertex_ploidy, pedigree_graph);
    auto vertex_range = boost::make_iterator_range(vertices(pedigree_graph));
    for (vertex_t v : vertex_range) {
        ploidies[v] = 1;
    }
}

vertex_t parse_pedigree_table(Graph &pedigree_graph,
        const dng::Pedigree &pedigree, bool normalize_somatic_trees) {
    using namespace std;
    using Sex = dng::Pedigree::Sex;
    using member_t = unsigned int;
    struct child_t {
        member_t id;
        bool maternal;
    };

    static_assert(DUMMY_INDEX == 0, "DUMMY_INDEX is something other than zero. Many code assumptions have changed.");
    static_assert(NULL_INDEX == -1, "NULL_INDEX is something other than -1. Many code assumptions have changed.");

    const member_t pedigree_dummy = pedigree.NumberOfMembers();

    // make a copy of names and sexes from pedigree
    vector<std::string> pedigree_names;
    vector<Sex> pedigree_sexes;
    pedigree_names.reserve(pedigree_dummy+1);
    pedigree_sexes.reserve(pedigree_dummy+1);
    for(member_t j = 0; j < pedigree_dummy; ++j) {
        auto & member = pedigree.GetMember(j);
        pedigree_names.push_back(member.child);
        pedigree_sexes.push_back(member.sex);
    }
    pedigree_names.push_back("R()()T");
    pedigree_sexes.push_back(Sex::Unknown);

    // identify parents and add nodes as needed
    vector<vector<child_t>> pedigree_children(pedigree_dummy+1);
    for(member_t j = 0; j < pedigree_dummy; ++j) {
        auto & member = pedigree.GetMember(j);
        member_t dad_id = pedigree.LookupMemberPosition(member.dad);
        member_t mom_id = pedigree.LookupMemberPosition(member.mom);
        // Selfing is not supported
        if (dad_id == mom_id && dad_id != pedigree_dummy) {
            throw std::invalid_argument("Unable to construct graph for pedigree; selfing is not supported");
        }
        // Check for sanity
        if (pedigree_sexes[dad_id] == Sex::Female) {
            throw std::invalid_argument("Unable to construct graph for pedigree; the father of '" +
                member.child + "' is female.");
        }
        if (pedigree_sexes[mom_id] == Sex::Male ) {
            throw std::invalid_argument("Unable to construct graph for pedigree; the mother of '" +
                member.child + "' is male.");
        }
        // if only one parent exists, create the other one
        if(dad_id == pedigree_dummy && mom_id < pedigree_dummy) {
            dad_id = pedigree_names.size();
            pedigree_names.push_back("unknown_dad_of_" + member.child);
            pedigree_sexes.push_back((pedigree_sexes[mom_id] == Sex::Female) ? Sex::Male : Sex::Unknown);
            // Identify the new node as a founder
            pedigree_children[pedigree_dummy].push_back({dad_id,0});
            pedigree_children[pedigree_dummy].push_back({dad_id,1});
            // Expand to accomodate new node
            pedigree_children.emplace_back();
            // Check for consistency
            assert(pedigree_names.size() == dad_id+1);
            assert(pedigree_sexes.size() == dad_id+1);
            assert(pedigree_children.size() == dad_id+1);
        }
        if(mom_id == pedigree_dummy && dad_id < pedigree_dummy) {
            mom_id = pedigree_names.size();
            pedigree_names.push_back("unknown_mom_of_" + member.child);
            pedigree_sexes.push_back((pedigree_sexes[dad_id] == Sex::Male) ? Sex::Female : Sex::Unknown);
            // Identify the new node as a founder
            pedigree_children[pedigree_dummy].push_back({mom_id,0});
            pedigree_children[pedigree_dummy].push_back({mom_id,1});
            // Expand to accommodate new node
            pedigree_children.emplace_back();
            // Check for consistency
            assert(pedigree_names.size() == mom_id+1);
            assert(pedigree_sexes.size() == mom_id+1);
            assert(pedigree_children.size() == mom_id+1);
        }
        pedigree_children[dad_id].push_back({j,0});
        pedigree_children[mom_id].push_back({j,1});
    }
    // Build Graph
    pedigree_graph.clear();
    for(size_t i = 0; i < pedigree_names.size();++i) {
        // Create a germline vertex with an empty label 
        add_vertex({{},VertexType::Germline},pedigree_graph);
    }
    auto labels = get(boost::vertex_label, pedigree_graph);
    auto sexes  = get(boost::vertex_sex, pedigree_graph);

    vector<vertex_t> touched(pedigree_children.size(), NULL_INDEX);
    vector<vertex_t> vertices(pedigree_children.size());
    queue<member_t> visited;

    // Add a dummy individual to the graph
    vertex_t counter = DUMMY_INDEX;
    labels[counter] = pedigree_names[pedigree_dummy];
    sexes[counter] = pedigree_sexes[pedigree_dummy];
    ++counter;
    visited.push(pedigree_dummy);

    // Continue with the rest of the pedigree
    while(!visited.empty()) {
        auto id = visited.front();
        auto vert = vertices[id];
        visited.pop();
        for(auto pedchild : pedigree_children[id]) {
            // create vertex for child after we have visited both its parents
            if(touched[pedchild.id] == NULL_INDEX) {
                touched[pedchild.id] = vert;
            } else {
                // add vertex
                auto child = counter++;
                vertices[pedchild.id] = child;
                labels[child] = pedigree_names[pedchild.id];
                sexes[child] = pedigree_sexes[pedchild.id];
                // add the meiotic edges
                auto mom =  pedchild.maternal ? vert : touched[pedchild.id];
                auto dad = !pedchild.maternal ? vert : touched[pedchild.id];
                add_edge(mom, child, {EdgeType::Maternal, 1.0f}, pedigree_graph);
                add_edge(dad, child, {EdgeType::Paternal, 1.0f}, pedigree_graph);

                // Check to see if mom and dad have been seen before
                auto id = edge(dad, mom, pedigree_graph);
                if (!id.second) { //Connect dad-mom to make a trio
                    add_edge(dad, mom, EdgeType::Spousal, pedigree_graph);
                }

                if(pedchild.id < pedigree_dummy) {
                    // Process newick file
                    std::size_t current_index = num_vertices(pedigree_graph);
                    int res = parse_newick(pedigree.GetMember(pedchild.id).attribute,
                            child, pedigree_graph, normalize_somatic_trees);
                    if (res == 0) {
                        // this line has a blank somatic line, so use the name from the pedigree
                        vertex_t v = add_vertex({labels[child], VertexType::Somatic}, pedigree_graph);
                        add_edge(child, v, {EdgeType::Mitotic, 1.0f}, pedigree_graph);
                    } else if (res == -1) {
                        throw std::invalid_argument("Unable to parse somatic data for individual '" +
                                                 pedigree_names[pedchild.id] + "'.");
                    }
                    // Mark the sex of the somatic nodes
                    for (vertex_t i = current_index; i < num_vertices(pedigree_graph); ++i) {
                        sexes[i] = sexes[child];
                    }
                }

                visited.push(pedchild.id);
            }
        }
    }
    // Sanity Check
    assert(counter == pedigree_names.size());
    return counter;
}

void add_libraries_to_graph(Graph &pedigree_graph, const libraries_t &libs) {
    auto sexes  = get(boost::vertex_sex, pedigree_graph);
    auto labels = get(boost::vertex_label, pedigree_graph);
    auto types  = get(boost::vertex_type, pedigree_graph);
    auto library_labels = get(boost::vertex_library_label, pedigree_graph);

    std::map<std::string, vertex_t> soma;
    auto vertex_range = boost::make_iterator_range(vertices(pedigree_graph));
    for(vertex_t v : vertex_range) {
        if(types[v] == VertexType::Somatic && !labels[v].empty()) {
            soma[labels[v]] = v;
        }
    }    

    // Add library nodes to graph
    for (size_t i = 0; i < libs.names.size(); ++i) {
        auto it = soma.find(libs.samples[i]);
        if(it == soma.end()) {
            continue;
        }
        vertex_t u = it->second;
        std::string name = labels[u];
        if(libs.names[i] != labels[u]) {
            name += DNG_LABEL_SEPARATOR;
            name += libs.names[i];
        }
        vertex_t v = add_vertex({name, VertexType::Library}, pedigree_graph);
        add_edge(u, v, {EdgeType::Library, 1.0f}, pedigree_graph);
        sexes[v] = sexes[u];
        library_labels[v] = libs.names[i];
    }
}

void update_edge_lengths(Graph &pedigree_graph,
        double mu_meiotic, double mu_somatic, double mu_library) {
    boost::graph_traits<Graph>::edge_iterator ei, ei_end;
    auto edge_types = get(boost::edge_type, pedigree_graph);
    auto lengths = get(boost::edge_length, pedigree_graph);

    auto range = boost::make_iterator_range(edges(pedigree_graph));
    for (edge_t e : range) {
        switch(edge_types[e]) {
        case EdgeType::Maternal:
        case EdgeType::Paternal:
            lengths[e] *= mu_meiotic;
            break;
        case EdgeType::Mitotic:
            lengths[e] *= mu_somatic;
            break;
        case EdgeType::Library:
            lengths[e] *= mu_library;
            break;
        default:
            break;
        }
    }
}

void prefix_vertex_labels(dng::detail::graph::Graph &pedigree_graph) {
    using namespace dng::detail::graph;

    auto labels = get(boost::vertex_label, pedigree_graph);
    auto types = get(boost::vertex_type, pedigree_graph);

    auto vertex_range = boost::make_iterator_range(vertices(pedigree_graph));
    for(vertex_t v : vertex_range) {
        const char *ch = "";
        switch(types[v]) {
        case VertexType::Germline:
            ch = (DNG_LABEL_PREFIX_GERMLINE DNG_LABEL_SEPARATOR);
            break;
        case VertexType::Somatic:
            ch = (DNG_LABEL_PREFIX_SOMATIC DNG_LABEL_SEPARATOR);
            break;
        case VertexType::Library:
            ch = (DNG_LABEL_PREFIX_LIBRARY DNG_LABEL_SEPARATOR);
            break;
        default:
            break;
        }
        labels[v].insert(0,ch);
    }
}


void simplify_pedigree(Graph &pedigree_graph) {
    using boost::make_iterator_range;

    auto edge_types = get(boost::edge_type, pedigree_graph);
    auto lengths = get(boost::edge_length, pedigree_graph);
    auto types = get(boost::vertex_type, pedigree_graph);

    auto vertex_range = boost::adaptors::reverse(make_iterator_range(vertices(pedigree_graph)));

    for (vertex_t v : vertex_range) {
        if(types[v] == VertexType::Library) {
            continue;
        }
        // identify children
        auto edge_range = make_iterator_range(out_edges(v, pedigree_graph));
        // NOTE: This assumes that children is a reasonable number
        // and will kill performance if it is not.
        std::vector<vertex_t> meiotic_children;
        for(edge_t && e : edge_range) {
            vertex_t u = target(e, pedigree_graph);
            if( u > v &&
                (edge_types[e] == EdgeType::Maternal || 
                 edge_types[e] == EdgeType::Paternal)) {
                meiotic_children.push_back(u);
            }
        }

        // Remove spousal edges with no children
        remove_out_edge_if(v, [&](edge_t e) -> bool {
            if(edge_types[e] != EdgeType::Spousal) {
                return false;
            }
            vertex_t u = target(e, pedigree_graph);
            auto spousal_edge_range = make_iterator_range(out_edges(u, pedigree_graph));
            for(edge_t && e : spousal_edge_range) {
                vertex_t o = target(e, pedigree_graph);
                auto it = boost::range::find(meiotic_children, o);
                if(it != meiotic_children.end()) {
                    return false;
                }
            }
            return true;
        }, pedigree_graph);

        size_t children = 0, ancestors = 0, spouses = 0;
        edge_range = make_iterator_range(out_edges(v, pedigree_graph));
        for (auto && e : edge_range) {
            if (edge_types[e] == EdgeType::Spousal) {
                spouses += 1;
            } else if (target(e, pedigree_graph) > v) {
                children += 1;
            } else {
                ancestors += 1;
            }
        }
        if (children == 0) {
            // this node has no descendants
            clear_vertex(v, pedigree_graph);
        } else if (children >= 2 || spouses != 0) {
            /*noop*/;
        } else if (ancestors > 0) {
            assert(ancestors < 3); 
            edge_t edge_trio[3];
            vertex_t vertex_trio[3];//
            int child_index = ancestors;
            auto it = edge_range.begin();
            for (size_t j = 0; j <= ancestors; ++j) {
                edge_trio[j] = *it++;
                vertex_trio[j] = target(edge_trio[j], pedigree_graph);
            }
            for (size_t p = 0; p < child_index; ++p) {
                if (vertex_trio[child_index] < vertex_trio[p]) {
                    boost::swap(vertex_trio[child_index], vertex_trio[p]);
                    boost::swap(edge_trio[child_index], edge_trio[p]);
                }
                add_edge(vertex_trio[p], vertex_trio[child_index],
                         {edge_types[edge_trio[p]], lengths[edge_trio[p]] + lengths[edge_trio[child_index]]},
                         pedigree_graph);
            }
            clear_vertex(v, pedigree_graph);
        }
    }
}

} // namespace 

std::vector<size_t> dng::RelationshipGraph::ConstructNodes(const Graph &pedigree_graph) {
    // node_ids[vertex] will convert vertex_id to node position.
    std::vector<size_t> node_ids(num_vertices(pedigree_graph), -1);

    labels_.clear();
    labels_.reserve(128);
    ploidies_.clear();
    ploidies_.reserve(128);

    auto labels = get(boost::vertex_label, pedigree_graph);
    auto ploidies = get(boost::vertex_ploidy, pedigree_graph);
    auto library_labels = get(boost::vertex_library_label, pedigree_graph);

    auto vertex_range = boost::make_iterator_range(vertices(pedigree_graph));
    for(vertex_t v : vertex_range) {
        // Skip vertices with no edges
        if (out_degree(v, pedigree_graph) == 0) {
            continue;
        }
        if(!library_labels[v].empty()) {
            library_names_.push_back(library_labels[v]);
        }
        const auto vid = labels_.size();
        node_ids[v] = vid;

        if (labels[v].empty() || labels[v].back() == DNG_LABEL_SEPARATOR_CHAR) {
            labels_.push_back(labels[v] + "unnamed_node_" + utility::to_pretty(vid));
        } else {
            labels_.push_back(labels[v]);
        }
        ploidies_.push_back(ploidies[v]);
    }
    num_nodes_ = labels_.size();

    auto update_position = [&node_ids](size_t pos, size_t last) -> size_t {
        for (; pos < node_ids.size() && node_ids[pos] == -1; ++pos)
            /*noop*/;
        return (pos < node_ids.size()) ? node_ids[pos] : last;
    };

    first_founder_ = update_position(first_founder_, num_nodes_);
    first_nonfounder_ = update_position(first_nonfounder_, num_nodes_);
    first_somatic_ = update_position(first_somatic_, num_nodes_);
    first_library_ = update_position(first_library_, num_nodes_);

    return node_ids;
}

void dng::RelationshipGraph::CreateFamiliesInfo(Graph &pedigree_graph,
        family_labels_t &family_labels, std::vector<vertex_t> &pivots) {

    auto groups = get(boost::vertex_group, pedigree_graph);
    auto families = get(boost::edge_family, pedigree_graph);

    // Calculate the connected components.  This defines independent sections
    // of the graph.
    std::size_t num_groups = connected_components(pedigree_graph, groups);

    // Calculate the biconnected components and articulation points.
    // This defines "nuclear" families and pivot individuals.
    // Nodes which have no edges will not be part of any family.
    std::vector<vertex_t> articulation_vertices;
    std::size_t num_families =
            biconnected_components(pedigree_graph, families,
                                   back_inserter(articulation_vertices)).first;

    family_labels = family_labels_t(num_families);
    pivots = std::vector<vertex_t>(num_families, DUMMY_INDEX);

    // Determine which edges belong to which nuclear families.
    boost::graph_traits<Graph>::edge_iterator ei, ei_end;
    for (tie(ei, ei_end) = edges(pedigree_graph); ei != ei_end; ++ei) {
        family_labels[families[*ei]].push_back(*ei);
    }

    // Determine the last family in each group.  All singleton groups will have
    // a value of -1 since they have no family assignment.
    using root_families_t = std::deque<std::size_t> ;
    root_families_t root_families(num_groups, -1);
    for (std::size_t f = 0; f < family_labels.size(); ++f) {
        // last one wins
        auto first_edge = family_labels[f][0];
        auto src_vertex = source(first_edge, pedigree_graph);
        root_families[groups[src_vertex]] = f;

    }

    // Identify the pivot for each family.
    // The pivot will be the last art. point that has an edge in
    // the group.  The pivot of the last group doesn't matter.
    for (auto a : articulation_vertices) {
        boost::graph_traits<Graph>::out_edge_iterator ei, ei_end;
        for (tie(ei, ei_end) = out_edges(a, pedigree_graph); ei != ei_end; ++ei) {
            // Just overwrite existing value so that the last one wins.
            pivots[families[*ei]] = a;
        }
    }

    // Root Pivots are special
    for (auto f : root_families) {
        if (f != -1) { //Assign to non-singleton groups
            pivots[f] = DUMMY_INDEX;
        }
    }
}

void dng::RelationshipGraph::CreatePeelingOps(
        const Graph &pedigree_graph, const std::vector<size_t> &node_ids,
        family_labels_t &family_labels, std::vector<vertex_t> &pivots) {

    auto edge_types = get(boost::edge_type, pedigree_graph);
    auto lengths = get(boost::edge_length, pedigree_graph);
    auto vertex_types = get(boost::vertex_type, pedigree_graph);

    using VertexType = detail::graph::VertexType;

    ClearFamilyInfo();

    transitions_.resize(num_nodes_);

    constexpr size_t null_id = static_cast<size_t>(-1);

    // Setup founder Transitions
    for (std::size_t i = first_founder_; i < first_nonfounder_; ++i) {
        transitions_[i] = {TransitionType::Founder, null_id, null_id, 0.0, 0.0,false,false,false};
    }

    // Detect Family Structure and pivot positions
    for (std::size_t k = 0; k < family_labels.size(); ++k) {
        auto &family_edges = family_labels[k];

        // Sort edges based on type and target
        boost::sort(family_edges, [&](edge_t x, edge_t y) -> bool {
            return (edge_types(x) < edge_types(y)) && (target(x, pedigree_graph) < target(y, pedigree_graph));
        });

        // Find the range of the parent types
        auto it = boost::find_if(family_edges, [&](edge_t x) -> bool {
            return (edge_types(x) != EdgeType::Spousal);
        });
        size_t num_spousal_edges = distance(family_edges.begin(), it);

        // Check to see what type of graph we have
        if (num_spousal_edges == 0) {
            // If we do not have a parent-child single branch,
            // we can't construct the pedigree.
            if (family_edges.size() != 1) {
                throw std::invalid_argument("Unable to construct peeler for pedigree;  "
                        "do not have a parent-child single branch");
            }
            // Create a mitotic peeling operation.
            auto child_index = target(*it, pedigree_graph);
            auto parent_index = source(*it, pedigree_graph);
            size_t parent = node_ids[parent_index];
            size_t child = node_ids[child_index];
            
            transitions_[child] = {TransitionType::Pair, parent, null_id, lengths[*it], 0.0};

            auto child_type = vertex_types(child_index);
            auto parent_type = vertex_types(parent_index);

            bool temp_types[3] = {false,false,false};
            for(int x = static_cast<int>(parent_type); x <= static_cast<int>(child_type); ++x) {
                temp_types[x] = true;
            }
            transitions_[child].is_germline = temp_types[0];
            transitions_[child].is_somatic  = temp_types[1];
            transitions_[child].is_library  = temp_types[2];

            family_members_.push_back({parent, child});

            if (node_ids[pivots[k]] == child) {
                peeling_ops_.push_back(peel::Op::DOWN);
            } else {
                peeling_ops_.push_back(peel::Op::UP);
                if (node_ids[pivots[k]] == node_ids[DUMMY_INDEX]) {
                    roots_.push_back(parent);
                }
            }
        } else if (num_spousal_edges == 1) {
            // If this family contains no children, skip it
            if (it == family_edges.end()) {
                continue;
            }
            // We have a nuclear family with 1 or more children

            auto dad_index = source(family_edges.front(), pedigree_graph);
            auto mom_index = target(family_edges.front(), pedigree_graph);
            size_t dad = node_ids[dad_index];
            size_t mom = node_ids[mom_index];

            family_members_.push_back({dad, mom});
            auto &family_members = family_members_.back();

            for (;it != family_edges.end();++it) {
                auto child_index = target(*it, pedigree_graph);

                size_t child = node_ids[child_index];
                if(edge_types(*it) == EdgeType::Maternal) {
                    transitions_[child] = {TransitionType::Trio, dad, mom, 0, lengths[*it]};
                    
                    auto child_type = vertex_types(child_index);
                    auto parent_type = vertex_types(mom_index);

                    bool temp_types[3] = {false,false,false};
                    for(int x = static_cast<int>(parent_type); x <= static_cast<int>(child_type); ++x) {
                        temp_types[x] = true;
                    }
                    transitions_[child].is_germline = temp_types[0];
                    transitions_[child].is_somatic  = temp_types[1];
                    transitions_[child].is_library  = temp_types[2];

                    family_members.push_back(child);
                } else {
                    assert(edge_types(*it) == EdgeType::Paternal);
                    assert(transitions_[child].type == TransitionType::Trio);
                    transitions_[child].length1 = lengths[*it];
                }
            }
            if (node_ids[pivots[k]] == node_ids[DUMMY_INDEX]) {
                // A family without a pivot is a root family
                peeling_ops_.push_back(peel::Op::TOFATHER);
                roots_.push_back(family_members[0]);

            } else {
                auto pivot_pos = boost::range::find(family_members, node_ids[pivots[k]]);
                size_t p = distance(family_members.begin(), pivot_pos);

                if (p == 0) {
                    peeling_ops_.push_back(peel::Op::TOFATHER);
                } else if (p == 1) {
                    peeling_ops_.push_back(peel::Op::TOMOTHER);
                } else if (p == 2) {
                    peeling_ops_.push_back(peel::Op::TOCHILD);
                } else {
                    peeling_ops_.push_back(peel::Op::TOCHILD);
                    boost::swap(family_members[p], family_members[2]);
                }
            }

        } else {
            throw std::runtime_error("Unable to construct peeler for pedigree; Not a zero-loop pedigree");
        }
    }
}

void dng::RelationshipGraph::ClearFamilyInfo(){
    roots_.clear();
    roots_.reserve(16);
    family_members_.clear();
    family_members_.reserve(128);
    peeling_ops_.clear();
    peeling_ops_.reserve(128);
    transitions_.clear();
    transitions_.reserve(128);
}
