/*
 * Copyright (c) 2015-2016 Reed A. Cartwright
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

#include <dng/depths.h>
#include <dng/utility.h>
#include <dng/io/ad.h>
#include <dng/io/utility.h>

#include <dng/detail/varint.h>

#include <dng/hts/bcf.h>

#include <algorithm>
#include <cstdlib>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/karma.hpp>
#include <boost/fusion/include/std_pair.hpp>

#include <boost/range/begin.hpp>
#include <boost/range/end.hpp>

using namespace dng::pileup;

namespace dng { namespace pileup {
AlleleDepths::match_labels_t AlleleDepths::MatchLabel;
AlleleDepths::match_indexes_t AlleleDepths::MatchIndexes;
}}

void dng::io::Ad::Clear() {
    contigs_.clear();
    contig_map_.clear();
    contig_attributes_.clear();

    input_libraries_ = {};
    output_libraries_ = {};
    
    indexes_.clear();
    extra_headers_.clear();

    last_location_ = 0;
    last_data_.clear();

    if(format_ == Format::AD) {
        id_.version = 0x0001;
        id_.name = "AD";        
    } else {
        id_.version = 0x0001;
        id_.name = "TAD";
    }
    id_.attributes.clear();
    counter_ = 0;
}

std::string dng::io::Ad::HeaderString() const {    
    // Write Header Line
    std::ostringstream output;
    // Write Contig Lines
    for(size_t i=0; i<contigs_.size(); ++i) {
        auto sq = contigs_[i];
        output << "@SQ\tSN:" << sq.name << "\tLN:" << sq.length;
        if(!contig_attributes_[i].empty()) {
            output << '\t' << contig_attributes_[i];
        }
        output << '\n';
    }

    // Write Library Lines
    for(size_t i=0; i < output_libraries_.names.size(); ++i) {
        output << "@AD\tID:" << output_libraries_.names[i] <<
                  "\tSM:" << output_libraries_.samples[i];
        if(!output_libraries_.attributes[i].empty()) {
            output << '\t' << output_libraries_.attributes[i];
        }
        output << '\n';
    }

    // Write the rest of the tokens
    for(auto it = extra_headers_.begin(); it != extra_headers_.end(); ++it) {
        output << *it;
        for(++it; it != extra_headers_.end() && *it != "\n"; ++it) {
            output << '\t' << *it;
        }
        output << '\n';
    }
    return output.str();
}


template<typename It>
void dng::io::Ad::ParseHeaderTokens(It it, It it_last) {
    using namespace boost;
    using namespace std;
    using boost::algorithm::starts_with;
    using boost::algorithm::iequals;
    using boost::algorithm::to_upper;
    namespace ss = boost::spirit::standard;
    namespace qi = boost::spirit::qi;
    using ss::space;
    using qi::uint_;
    using qi::lit;
    using qi::phrase_parse;
    qi::uint_parser<uint8_t> uint8_;

    for(; it != it_last; ++it) {
        if(*it == "@SQ") {
            contig_t sq;
            std::string attr;
            for(++it; it != it_last && *it != "\n"; ++it) {
                if(starts_with(*it, "SN:")) {
                    sq.name = it->substr(3);
                } else if(starts_with(*it, "LN:")) {
                    const char *first = it->c_str()+3;
                    const char *last  = it->c_str()+it->length();

                    if(!phrase_parse(first, last, uint_ , space, sq.length) || first != last) {
                        throw runtime_error("ERROR: Unable to parse AD/TAD header: Unable to parse sequence length from header attribute '" + *it + "'");
                    }
                } else {
                    if(!attr.empty()) {
                        attr += '\t';
                    }
                    attr += *it;
                }
            }
            contigs_.push_back(std::move(sq));
            contig_attributes_.push_back(std::move(attr));
        } else if(*it == "@AD") {
            std::string name, sample, attr;
            for(++it; it != it_last && *it != "\n"; ++it) {       
                if(starts_with(*it, "ID:")) {
                    name = it->substr(3);
                } else if(starts_with(*it, "SM:")) {
                    sample = it->substr(3);
                } else {
                    if(!attr.empty()) {
                        attr += '\t';
                    }
                    attr += *it;
                }
            }
            input_libraries_.names.push_back(std::move(name));
            input_libraries_.samples.push_back(std::move(sample));
            input_libraries_.attributes.push_back(std::move(attr));
        } else if(*it == "@HD") {
            // sequence dictionaries may contain HD tags, drop them
            for(++it; it != it_last && *it != "\n"; ++it) {
                /*noop*/;
            }
        } else if(*it == "@ID") {
            throw runtime_error("ERROR: Unable to parse AD/TAD header: @ID can only be specified once.");
        } else if((*it)[0] == '@') {
            // store all the information for this line into the extra_headers_ field
            extra_headers_.push_back(std::move(*it));
            for(++it; it != it_last; ++it) {
                if(*it == "\n") {
                    extra_headers_.push_back(std::move(*it));
                    break;
                }
                extra_headers_.push_back(std::move(*it));
            }
        } else {
            throw runtime_error("ERROR: Unable to parse AD/TAD header: Something went wrong. "
                "Expected the @TAG at the beginning of a header line but got '" + *it +"'.");            
        }
    }
    FinishHeader();
}

void dng::io::Ad::AddHeaderLines(const std::string& lines) {
    // Tokenize header
    auto tokens = utility::make_tokenizer_dropempty(lines);
    // Parse Tokens
    ParseHeaderTokens(tokens.begin(), tokens.end());
}

int dng::io::Ad::ReadHeaderTad() {
    using namespace boost;
    using namespace std;
    using boost::algorithm::starts_with;
    using boost::algorithm::iequals;
    using boost::algorithm::to_upper;
    namespace ss = boost::spirit::standard;
    namespace qi = boost::spirit::qi;
    using ss::space;
    using qi::uint_;
    using qi::lit;
    using qi::phrase_parse;
    qi::uint_parser<uint8_t> uint8_;

    // Construct the tokenizer
    typedef tokenizer<char_separator<char>,
            std::istreambuf_iterator<char>> tokenizer;
    char_separator<char> sep("\t", "\n");
    tokenizer tokens(istreambuf_range(stream_), sep);

    // Error if the first character isn't '@'
    if(stream_.peek() != '@') {
        return 0;
    }

    // Store all the tokens in the header until we reach the first data line
    vector<string> store;
    for(auto tok = tokens.begin(); tok != tokens.end(); ++tok) {
        store.push_back(std::move(*tok));
        if(*tok == "\n") {
            if(stream_.peek() != '@') {
                break;
            }
        }
    }

    // Setup header information
    auto it = store.begin();
    if(it == store.end() || *it != "@ID") {
        throw runtime_error("ERROR: Unable to parse TAD header: @ID missing from first line.");
    }
    // Parse @ID line which identifies the file format and version.
    for(++it; it != store.end(); ++it) {
        if(*it == "\n") {
            ++it;
            break;
        } else if(starts_with(*it, "FF:")) {
            id_.name = it->substr(3);
            to_upper(id_.name);
            if(id_.name != "TAD") {
                throw runtime_error("ERROR: Unable to parse TAD header: Unknown file format '" + id_.name + "'");
            }
        } else if(starts_with(*it, "VN:")) {
            const char *first = it->c_str()+3;
            const char *last  = it->c_str()+it->length();
            pair<uint8_t,uint8_t> bytes;
            if(!phrase_parse(first, last, uint8_ >> ( '.' >> uint8_ || lit('\0')), space, bytes) || first != last) {
                throw runtime_error("ERROR: Unable to parse TAD header: File format version information '" + *it + "' not major.minor.");
            }
            id_.version = ((uint16_t)bytes.first << 8) | bytes.second;
        } else {
            id_.attributes.push_back(std::move(*it));
        }
    }
    ParseHeaderTokens(it, store.end());
    return 1;
}

int dng::io::Ad::WriteHeaderTad() {
    std::string header = HeaderString();
    stream_ << "@ID\tFF:" << id_.name << "\tVN:" << (id_.version >> 8) << "." << (id_.version & 0xFF);
    for(auto && a : id_.attributes) {
        stream_ << '\t' << a;
    }
    stream_ << '\n';
    stream_.write(header.data(), header.size());
    return 1;
}

int dng::io::Ad::ReadTad(AlleleDepths *pline) {
    using namespace std;
    using namespace boost;
    namespace ss = boost::spirit::standard;
    namespace qi = boost::spirit::qi;
    using ss::space;
    using qi::uint_;
    using qi::phrase_parse;

    if(pline == nullptr) {
        // discard this line
        while(stream_ && stream_.get() != '\n') {
            /*noop*/;
        }
        return 1;
    }
    // Construct the tokenizer
    typedef tokenizer<char_separator<char>,
            std::istreambuf_iterator<char>> tokenizer;
    char_separator<char> sep("\t", "\n");
    tokenizer tokens(istreambuf_range(stream_), sep);
    vector<string> store;
    auto it = tokens.begin();
    for(; it != tokens.end() && *it == "\n"; ++it) {
        /* skip empty lines */;
    }
    for(; it != tokens.end(); ++it) {
        if(*it == "\n") {
            break;
        }
        store.push_back(*it);
    }
    // end of file, so read nothing
    if(store.empty()) {
        return 0;
    }
    // check to make sure store has the proper size
    if(store.size() != 3+input_libraries_.names.size()) {
        throw runtime_error("ERROR: Unable to parse TAD record: Expected " + utility::to_pretty(3+input_libraries_.names.size()) 
            + " columns. Found " + utility::to_pretty(store.size()) + " columns.");
    }

    // parse contig
    auto mit = contig_map_.find(store[0]);
    if(mit == contig_map_.end()) {
        throw runtime_error("ERROR: Unable to parse TAD record: Contig Name '" + store[0] + "' is unknown.");
    }
    int contig_num = mit->second;
    
    // parse position
    int position;
    auto first = store[1].cbegin();
    auto last  = store[1].cend();

    if(!phrase_parse(first, last, uint_, space, position) || first != last) {
        throw runtime_error("ERROR: Unable to parse TAD record: Unable to parse position '" + store[1] + "' as integer.");
    }
    pline->location(utility::make_location(contig_num,position-1));

    // parse type and resize
    int8_t ty = AlleleDepths::MatchLabel(store[2]);
    if(ty == -1) {
        throw runtime_error("ERROR: Unable to parse TAD record: Type '" + store[2] + "' is unknown.");        
    }
    pline->resize(ty,output_libraries_.names.size());

    // parse data
    vector<int> data;
    bool success;
    for(size_t i=3;i<store.size();++i) {
        tie(data,success) = utility::parse_int_list(store[i]);
        if(!success) {
            throw runtime_error("ERROR: Unable to parse TAD record: unable to parse comma-separated depths '" + store[i] + "'.");
        } else if(data.size() != pline->num_nucleotides()) {
            throw runtime_error("ERROR: Unable to parse TAD record: incorrect number of depths parsed from '" + store[i]
                + "'. Expected " + utility::to_pretty(pline->num_nucleotides())
                + " columns. Found " + utility::to_pretty(data.size()));
        }
        const size_t pos = indexes_[i-3];
        if(pos != -1) {        
            for(size_t j=0;j<data.size();++j) {
                (*pline)(pos,j) = data[j];
            }
        }
    }
    return 1;
}

int dng::io::Ad::WriteTad(const AlleleDepths& line) {
    using dng::utility::location_to_contig;
    using dng::utility::location_to_position;
    typedef AlleleDepths::size_type size_type;

    if(line.data_size() == 0) {
        return 0;
    }

    const location_t loc = line.location();
    const size_type nlib = line.num_libraries();
    const size_type nnuc = line.num_nucleotides();
    auto contig_num = location_to_contig(loc);
    if(contig_num >= contigs_.size()) {
        return 0;
    }

    stream_ << contigs_[contig_num].name << '\t';
    stream_ << location_to_position(loc)+1 << '\t';
    stream_ << line.type_info().label_upper;
    for(AlleleDepths::size_type y = 0; y != nlib; ++y) {
        stream_  << '\t';
        size_type x = 0;
        for(; x != nnuc-1; ++x) {
            stream_  << line(y,x) << ',';
        }
        stream_  << line(y,x);
    }
    stream_ << '\n';
    return 1;
}

const char ad_header[9] = "\255AD\002\r\n\032\n";

int dng::io::Ad::ReadHeaderAd() {
    using namespace std;
    using namespace boost;
    // read beginning of header number
    char buffer[8+2+4+8];
    stream_.read(buffer, sizeof(buffer));
    if(!stream_) {
        throw runtime_error("ERROR: Unable to parse AD header: missing beginning values.");
    }
    // test magic
    if(strncmp(&buffer[0], ad_header, 8) != 0) {
        throw runtime_error("ERROR: Unable to parse AD header: identifying number (magic) not found.");
    }
    // test version
    uint16_t version = 0;
    memcpy(&version, &buffer[8], 2);
    if(version != 0x0001) {
        throw runtime_error("ERROR: Unable to parse AD header: unknown version " + to_string(version >> 8) + '.' + to_string(version & 0xFF) + '.');
    }
    id_.name = "AD";
    id_.version = version;

    // Read size of header
    int32_t tad_sz = 0;
    memcpy(&tad_sz, &buffer[10], 4);
    // Read Header
    std::string tad_header(tad_sz,'\0');
    stream_.read(&tad_header[0], tad_sz);
    if(!stream_) {
        throw runtime_error("ERROR: Unable to parse AD header: TAD header block missing.");
    }
    // Shrink header in case it is null padded
    auto pos = tad_header.find_first_of('\0');
    if(pos != string::npos) {
        tad_header.resize(pos);
    }
    
    AddHeaderLines(tad_header); 
    return 1;
}

int dng::io::Ad::WriteHeaderAd() {
    auto header = HeaderString();
    if(header.size() > INT_MAX) {
        // Header is too big to write
        return 0;
    }
    // Write out magic header
    stream_.write(ad_header,8);
    // Write out 2-bytes to represent file format version information in little-endian
    stream_.write((const char*)&id_.version, 2);
    // Write 4 bytes to represent header size in little-endian format
    int32_t sz = header.size();
    stream_.write((const char*)&sz, 4);
    // Write out 8-bytes to represent future flags
    uint64_t zero = 0;
    stream_.write((const char*)&zero, 8);
    // Write the header
    stream_.write(header.data(), sz);
    return 1;
}

int dng::io::Ad::ReadAd(AlleleDepths *pline) {
    namespace varint = dng::detail::varint;

    // read location and type
    auto result = varint::get(stream_.rdbuf());
    if(!result.second) {
        return 0;
    }
    location_t loc = (result.first >> 7);
    int8_t rec_type = result.first & 0x7F;
    if(utility::location_to_contig(loc) > 0) {
        // absolute positioning
        loc -= (1LL << 32);
        // read reference information into buffer
        for(size_t i=0;i<input_libraries_.names.size();++i) {
            result = varint::get(stream_.rdbuf());
            if(!result.second) {
                return 0;
            }          
            last_data_[i] = result.first;
        }
    } else {
        // relative positioning
        loc += last_location_ + 1;
        // read reference information into buffer
        for(size_t i=0;i<input_libraries_.names.size();++i) {
            auto zzresult = varint::get_zig_zag(stream_.rdbuf());
            if(!zzresult.second) {
                return 0;
            }          
            last_data_[i] += zzresult.first;
        }
    }
    last_location_ = loc;

    if(pline == nullptr) {
        int width = AlleleDepths::type_info_table[rec_type].width;
        for(size_t i=input_libraries_.names.size(); i < input_libraries_.names.size()*width; ++i) {
            result = varint::get(stream_.rdbuf());
            if(!result.second) {
                return 0;
            }
        }
        return 1;
    }
    pline->location(loc);
    pline->resize(rec_type,output_libraries_.names.size());

    // Reference depths
    for(size_t i=0;i<input_libraries_.names.size();++i) {
        const size_t pos = indexes_[i];
        if(pos != -1) {
            (*pline)(pos,0) = last_data_[i];
        }
    }
    // Alternate depths
    for(size_t j=1;j< pline->num_nucleotides();++j) {
        for(size_t i=0; i < input_libraries_.names.size(); ++i) {
            result = varint::get(stream_.rdbuf());
            if(!result.second) {
                return 0;
            }
            const size_t pos = indexes_[i];
            if(pos != -1) {
                (*pline)(pos,j) = result.first;
            }
        }
    }
    return 1;
}

int dng::io::Ad::WriteAd(const AlleleDepths& line) {
    static_assert(sizeof(AlleleDepths::type_info_table) / sizeof(AlleleDepths::type_info_t) == 128,
        "AlleleDepths::type_info_table does not have 128 elements.");
    static_assert(sizeof(location_t) == 8, "location_t is not a 64-bit value.");

    using dng::utility::location_to_contig;
    using dng::utility::location_to_position;
    typedef AlleleDepths::size_type size_type;

    if(line.data_size() == 0) {
        return 0;
    }
    location_t loc = line.location();
    // The Ad format only holds up to 2^25-2 contigs 
    if(location_to_contig(loc) > 0x01FFFFFE) {
        return 0;
    }

    if(counter_ == 0 || location_to_contig(loc) != location_to_contig(last_location_)) {
        // use absolute positioning
        // Adjust the contig number by 1
        last_location_ = loc;
        loc += (1LL << 32);
    } else {
        // Make sure we are sorted here
        assert(loc > last_location_);
        location_t diff = loc - last_location_ - 1;
        last_location_ = loc;
        loc = diff;
        assert(location_to_contig(loc) == 0);
    }
    // set counter
    counter_ += 1;

    // Fetch color and check if it is non-negative
    int8_t color = line.color();
    assert(color >= 0);
    uint64_t u = (loc << 7) | color;
    // write out contig, position, and location
    namespace varint = dng::detail::varint;
    if(!varint::put(stream_.rdbuf(), u)) {
        return 0;
    }
    // write out data
    if(location_to_contig(loc) == 0) {
        // when using relative positioning output reference depths relative to previous
        for(size_type i = 0; i < output_libraries_.names.size(); ++i) {
            int64_t n = line(i,0)-last_data_[i];
            if(!varint::put_zig_zag(stream_.rdbuf(),n)) {
                return 0;
            }
        }
    } else {
        // when using absolute positioning output reference depths normally
        for(size_type i = 0; i < output_libraries_.names.size(); ++i) {
            if(!varint::put(stream_.rdbuf(),line(i,0))) {
                return 0;
            }
        }
    }
    // output all non-reference depths normally
    for(size_t j=1;j<line.num_nucleotides();++j) {
        for(size_t i=0; i < line.num_libraries(); ++i) {
            if(!varint::put(stream_.rdbuf(),line(i,j))) {
                return 0;
            }
        }
    }
    // Save reference depths
    for(size_type i = 0; i < output_libraries_.names.size(); ++i) {
        last_data_[i] = line(i,0);
    }

    return 1;
}

constexpr int MAX_VARINT_SIZE = 10;
std::pair<uint64_t,bool> dng::detail::varint::get_fallback(bytebuf_t *in, uint64_t result) {
    assert(in != nullptr);
    assert((result & 0x80) == 0x80);
    assert((result & 0xFF) == result);
    for(int i=1;i<MAX_VARINT_SIZE;i++) {
        // remove the most recent MSB
        result -= (0x80LL << (7*i-7));
        // grab a character
        bytebuf_t::int_type n = in->sbumpc();
        // if you have reached the end of stream, return error
        if(bytebuf_t::traits_type::eq_int_type(n, bytebuf_t::traits_type::eof())) {
            return {0,false};
        }
        // Convert back to a char and save in a 64-bit num.
        uint64_t u = static_cast<uint8_t>(bytebuf_t::traits_type::to_char_type(n));
        // shift these bits and add them to result
        result += (u << (7*i));
        // if MSB is not set, return the result
        if(!(u & 0x80)) {
            return {result,true};
        }
    }
    // if you have read more bytes than expected, return error
    return {0,false};
}

bool dng::detail::varint::put(bytebuf_t *out, uint64_t u) {
    assert(out != nullptr);
    while(u >= 0x80) {
        uint8_t b = static_cast<uint8_t>(u | 0x80);
        bytebuf_t::int_type n = out->sputc(b);
        if(bytebuf_t::traits_type::eq_int_type(n, bytebuf_t::traits_type::eof())) {
            return false;
        }
        u >>= 7;
    }
    uint8_t b = static_cast<uint8_t>(u);
    bytebuf_t::int_type n = out->sputc(b);
    if(bytebuf_t::traits_type::eq_int_type(n, bytebuf_t::traits_type::eof())) {
        return false;
    }
    return true;
}

const AlleleDepths::type_info_t
    AlleleDepths::type_info_table[128] = {
    {0,   1, "A",     "a",     "A",         0, {0,3,2,1}},
    {1,   1, "C",     "c",     "C",         1, {1,3,2,0}},
    {2,   1, "G",     "g",     "G",         2, {2,3,1,0}},
    {3,   1, "T",     "t",     "T",         3, {3,2,1,0}},
    {4,   2, "AC",    "ac",    "A,C",       0, {0,1,3,2}},
    {5,   2, "AG",    "ag",    "A,G",       0, {0,2,3,1}},
    {6,   2, "AT",    "at",    "A,T",       0, {0,3,2,1}},
    {7,   2, "CA",    "ca",    "C,A",       1, {1,0,3,2}},
    {8,   2, "CG",    "cg",    "C,G",       1, {1,2,3,0}},
    {9,   2, "CT",    "ct",    "C,T",       1, {1,3,2,0}},
    {10,  2, "GA",    "ga",    "G,A",       2, {2,0,3,1}},
    {11,  2, "GC",    "gc",    "G,C",       2, {2,1,3,0}},
    {12,  2, "GT",    "gt",    "G,T",       2, {2,3,1,0}},
    {13,  2, "TA",    "ta",    "T,A",       3, {3,0,2,1}},
    {14,  2, "TC",    "tc",    "T,C",       3, {3,1,2,0}},
    {15,  2, "TG",    "tg",    "T,G",       3, {3,2,1,0}},
    {16,  3, "ACG",   "acg",   "A,C,G",     0, {0,1,2,3}},
    {17,  3, "ACT",   "act",   "A,C,T",     0, {0,1,3,2}},
    {18,  3, "AGC",   "agc",   "A,G,C",     0, {0,2,1,3}},
    {19,  3, "AGT",   "agt",   "A,G,T",     0, {0,2,3,1}},
    {20,  3, "ATC",   "atc",   "A,T,C",     0, {0,3,1,2}},
    {21,  3, "ATG",   "atg",   "A,T,G",     0, {0,3,2,1}},
    {22,  3, "CAG",   "cag",   "C,A,G",     1, {1,0,2,3}},
    {23,  3, "CAT",   "cat",   "C,A,T",     1, {1,0,3,2}},
    {24,  3, "CGA",   "cga",   "C,G,A",     1, {1,2,0,3}},
    {25,  3, "CGT",   "cgt",   "C,G,T",     1, {1,2,3,0}},
    {26,  3, "CTA",   "cta",   "C,T,A",     1, {1,3,0,2}},
    {27,  3, "CTG",   "ctg",   "C,T,G",     1, {1,3,2,0}},
    {28,  3, "GAC",   "gac",   "G,A,C",     2, {2,0,1,3}},
    {29,  3, "GAT",   "gat",   "G,A,T",     2, {2,0,3,1}},
    {30,  3, "GCA",   "gca",   "G,C,A",     2, {2,1,0,3}},
    {31,  3, "GCT",   "gct",   "G,C,T",     2, {2,1,3,0}},
    {32,  3, "GTA",   "gta",   "G,T,A",     2, {2,3,0,1}},
    {33,  3, "GTC",   "gtc",   "G,T,C",     2, {2,3,1,0}},
    {34,  3, "TAC",   "tac",   "T,A,C",     3, {3,0,1,2}},
    {35,  3, "TAG",   "tag",   "T,A,G",     3, {3,0,2,1}},
    {36,  3, "TCA",   "tca",   "T,C,A",     3, {3,1,0,2}},
    {37,  3, "TCG",   "tcg",   "T,C,G",     3, {3,1,2,0}},
    {38,  3, "TGA",   "tga",   "T,G,A",     3, {3,2,0,1}},
    {39,  3, "TGC",   "tgc",   "T,G,C",     3, {3,2,1,0}},
    {40,  4, "ACGT",  "acgt",  "A,C,G,T",   0, {0,1,2,3}},
    {41,  4, "ACTG",  "actg",  "A,C,T,G",   0, {0,1,3,2}},
    {42,  4, "AGCT",  "agct",  "A,G,C,T",   0, {0,2,1,3}},
    {43,  4, "AGTC",  "agtc",  "A,G,T,C",   0, {0,2,3,1}},
    {44,  4, "ATCG",  "atcg",  "A,T,C,G",   0, {0,3,1,2}},
    {45,  4, "ATGC",  "atgc",  "A,T,G,C",   0, {0,3,2,1}},
    {46,  4, "CAGT",  "cagt",  "C,A,G,T",   1, {1,0,2,3}},
    {47,  4, "CATG",  "catg",  "C,A,T,G",   1, {1,0,3,2}},
    {48,  4, "CGAT",  "cgat",  "C,G,A,T",   1, {1,2,0,3}},
    {49,  4, "CGTA",  "cgta",  "C,G,T,A",   1, {1,2,3,0}},
    {50,  4, "CTAG",  "ctag",  "C,T,A,G",   1, {1,3,0,2}},
    {51,  4, "CTGA",  "ctga",  "C,T,G,A",   1, {1,3,2,0}},
    {52,  4, "GACT",  "gact",  "G,A,C,T",   2, {2,0,1,3}},
    {53,  4, "GATC",  "gatc",  "G,A,T,C",   2, {2,0,3,1}},
    {54,  4, "GCAT",  "gcat",  "G,C,A,T",   2, {2,1,0,3}},
    {55,  4, "GCTA",  "gcta",  "G,C,T,A",   2, {2,1,3,0}},
    {56,  4, "GTAC",  "gtac",  "G,T,A,C",   2, {2,3,0,1}},
    {57,  4, "GTCA",  "gtca",  "G,T,C,A",   2, {2,3,1,0}},
    {58,  4, "TACG",  "tacg",  "T,A,C,G",   3, {3,0,1,2}},
    {59,  4, "TAGC",  "tagc",  "T,A,G,C",   3, {3,0,2,1}},
    {60,  4, "TCAG",  "tcag",  "T,C,A,G",   3, {3,1,0,2}},
    {61,  4, "TCGA",  "tcga",  "T,C,G,A",   3, {3,1,2,0}},
    {62,  4, "TGAC",  "tgac",  "T,G,A,C",   3, {3,2,0,1}},
    {63,  4, "TGCA",  "tgca",  "T,G,C,A",   3, {3,2,1,0}},
    {64,  1, "NA",    "na",    "N,A",       4, {0,3,2,1}},
    {65,  1, "NC",    "nc",    "N,C",       4, {1,3,2,0}},
    {66,  1, "NG",    "ng",    "N,G",       4, {2,3,1,0}},
    {67,  1, "NT",    "nt",    "N,T",       4, {3,2,1,0}},
    {68,  2, "NAC",   "nac",   "N,A,C",     4, {0,1,3,2}},
    {69,  2, "NAG",   "nag",   "N,A,G",     4, {0,2,3,1}},
    {70,  2, "NAT",   "nat",   "N,A,T",     4, {0,3,2,1}},
    {71,  2, "NCA",   "nca",   "N,C,A",     4, {1,0,3,2}},
    {72,  2, "NCG",   "ncg",   "N,C,G",     4, {1,2,3,0}},
    {73,  2, "NCT",   "nct",   "N,C,T",     4, {1,3,2,0}},
    {74,  2, "NGA",   "nga",   "N,G,A",     4, {2,0,3,1}},
    {75,  2, "NGC",   "ngc",   "N,G,C",     4, {2,1,3,0}},
    {76,  2, "NGT",   "ngt",   "N,G,T",     4, {2,3,1,0}},
    {77,  2, "NTA",   "nta",   "N,T,A",     4, {3,0,2,1}},
    {78,  2, "NTC",   "ntc",   "N,T,C",     4, {3,1,2,0}},
    {79,  2, "NTG",   "ntg",   "N,T,G",     4, {3,2,1,0}},
    {80,  3, "NACG",  "nacg",  "N,A,C,G",   4, {0,1,2,3}},
    {81,  3, "NACT",  "nact",  "N,A,C,T",   4, {0,1,3,2}},
    {82,  3, "NAGC",  "nagc",  "N,A,G,C",   4, {0,2,1,3}},
    {83,  3, "NAGT",  "nagt",  "N,A,G,T",   4, {0,2,3,1}},
    {84,  3, "NATC",  "natc",  "N,A,T,C",   4, {0,3,1,2}},
    {85,  3, "NATG",  "natg",  "N,A,T,G",   4, {0,3,2,1}},
    {86,  3, "NCAG",  "ncag",  "N,C,A,G",   4, {1,0,2,3}},
    {87,  3, "NCAT",  "ncat",  "N,C,A,T",   4, {1,0,3,2}},
    {88,  3, "NCGA",  "ncga",  "N,C,G,A",   4, {1,2,0,3}},
    {89,  3, "NCGT",  "ncgt",  "N,C,G,T",   4, {1,2,3,0}},
    {90,  3, "NCTA",  "ncta",  "N,C,T,A",   4, {1,3,0,2}},
    {91,  3, "NCTG",  "nctg",  "N,C,T,G",   4, {1,3,2,0}},
    {92,  3, "NGAC",  "ngac",  "N,G,A,C",   4, {2,0,1,3}},
    {93,  3, "NGAT",  "ngat",  "N,G,A,T",   4, {2,0,3,1}},
    {94,  3, "NGCA",  "ngca",  "N,G,C,A",   4, {2,1,0,3}},
    {95,  3, "NGCT",  "ngct",  "N,G,C,T",   4, {2,1,3,0}},
    {96,  3, "NGTA",  "ngta",  "N,G,T,A",   4, {2,3,0,1}},
    {97,  3, "NGTC",  "ngtc",  "N,G,T,C",   4, {2,3,1,0}},
    {98,  3, "NTAC",  "ntac",  "N,T,A,C",   4, {3,0,1,2}},
    {99,  3, "NTAG",  "ntag",  "N,T,A,G",   4, {3,0,2,1}},
    {100, 3, "NTCA",  "ntca",  "N,T,C,A",   4, {3,1,0,2}},
    {101, 3, "NTCG",  "ntcg",  "N,T,C,G",   4, {3,1,2,0}},
    {102, 3, "NTGA",  "ntga",  "N,T,G,A",   4, {3,2,0,1}},
    {103, 3, "NTGC",  "ntgc",  "N,T,G,C",   4, {3,2,1,0}},
    {104, 4, "NACGT", "nacgt", "N,A,C,G,T", 4, {0,1,2,3}},
    {105, 4, "NACTG", "nactg", "N,A,C,T,G", 4, {0,1,3,2}},
    {106, 4, "NAGCT", "nagct", "N,A,G,C,T", 4, {0,2,1,3}},
    {107, 4, "NAGTC", "nagtc", "N,A,G,T,C", 4, {0,2,3,1}},
    {108, 4, "NATCG", "natcg", "N,A,T,C,G", 4, {0,3,1,2}},
    {109, 4, "NATGC", "natgc", "N,A,T,G,C", 4, {0,3,2,1}},
    {110, 4, "NCAGT", "ncagt", "N,C,A,G,T", 4, {1,0,2,3}},
    {111, 4, "NCATG", "ncatg", "N,C,A,T,G", 4, {1,0,3,2}},
    {112, 4, "NCGAT", "ncgat", "N,C,G,A,T", 4, {1,2,0,3}},
    {113, 4, "NCGTA", "ncgta", "N,C,G,T,A", 4, {1,2,3,0}},
    {114, 4, "NCTAG", "nctag", "N,C,T,A,G", 4, {1,3,0,2}},
    {115, 4, "NCTGA", "nctga", "N,C,T,G,A", 4, {1,3,2,0}},
    {116, 4, "NGACT", "ngact", "N,G,A,C,T", 4, {2,0,1,3}},
    {117, 4, "NGATC", "ngatc", "N,G,A,T,C", 4, {2,0,3,1}},
    {118, 4, "NGCAT", "ngcat", "N,G,C,A,T", 4, {2,1,0,3}},
    {119, 4, "NGCTA", "ngcta", "N,G,C,T,A", 4, {2,1,3,0}},
    {120, 4, "NGTAC", "ngtac", "N,G,T,A,C", 4, {2,3,0,1}},
    {121, 4, "NGTCA", "ngtca", "N,G,T,C,A", 4, {2,3,1,0}},
    {122, 4, "NTACG", "ntacg", "N,T,A,C,G", 4, {3,0,1,2}},
    {123, 4, "NTAGC", "ntagc", "N,T,A,G,C", 4, {3,0,2,1}},
    {124, 4, "NTCAG", "ntcag", "N,T,C,A,G", 4, {3,1,0,2}},
    {125, 4, "NTCGA", "ntcga", "N,T,C,G,A", 4, {3,1,2,0}},
    {126, 4, "NTGAC", "ntgac", "N,T,G,A,C", 4, {3,2,0,1}},
    {127, 4, "NTGCA", "ntgca", "N,T,G,C,A", 4, {3,2,1,0}}
};

const AlleleDepths::type_info_gt_t
    AlleleDepths::type_info_gt_table[128] = {
    {0,   1,  {0,6,9,3,8,5,1,7,4,2}, {0,9,5,2}},
    {1,   1,  {2,7,9,4,8,5,1,6,3,0}, {2,9,5,0}},
    {2,   1,  {5,8,9,4,7,2,3,6,1,0}, {5,9,2,0}},
    {3,   1,  {9,8,5,7,4,2,6,3,1,0}, {9,5,2,0}},
    {4,   3,  {0,1,2,6,7,9,3,4,8,5}, {0,2,9,5}},
    {5,   3,  {0,3,5,6,8,9,1,4,7,2}, {0,5,9,2}},
    {6,   3,  {0,6,9,3,8,5,1,7,4,2}, {0,9,5,2}},
    {7,   3,  {2,1,0,7,6,9,4,3,8,5}, {2,0,9,5}},
    {8,   3,  {2,4,5,7,8,9,1,3,6,0}, {2,5,9,0}},
    {9,   3,  {2,7,9,4,8,5,1,6,3,0}, {2,9,5,0}},
    {10,  3,  {5,3,0,8,6,9,4,1,7,2}, {5,0,9,2}},
    {11,  3,  {5,4,2,8,7,9,3,1,6,0}, {5,2,9,0}},
    {12,  3,  {5,8,9,4,7,2,3,6,1,0}, {5,9,2,0}},
    {13,  3,  {9,6,0,8,3,5,7,1,4,2}, {9,0,5,2}},
    {14,  3,  {9,7,2,8,4,5,6,1,3,0}, {9,2,5,0}},
    {15,  3,  {9,8,5,7,4,2,6,3,1,0}, {9,5,2,0}},
    {16,  6,  {0,1,2,3,4,5,6,7,8,9}, {0,2,5,9}},
    {17,  6,  {0,1,2,6,7,9,3,4,8,5}, {0,2,9,5}},
    {18,  6,  {0,3,5,1,4,2,6,8,7,9}, {0,5,2,9}},
    {19,  6,  {0,3,5,6,8,9,1,4,7,2}, {0,5,9,2}},
    {20,  6,  {0,6,9,1,7,2,3,8,4,5}, {0,9,2,5}},
    {21,  6,  {0,6,9,3,8,5,1,7,4,2}, {0,9,5,2}},
    {22,  6,  {2,1,0,4,3,5,7,6,8,9}, {2,0,5,9}},
    {23,  6,  {2,1,0,7,6,9,4,3,8,5}, {2,0,9,5}},
    {24,  6,  {2,4,5,1,3,0,7,8,6,9}, {2,5,0,9}},
    {25,  6,  {2,4,5,7,8,9,1,3,6,0}, {2,5,9,0}},
    {26,  6,  {2,7,9,1,6,0,4,8,3,5}, {2,9,0,5}},
    {27,  6,  {2,7,9,4,8,5,1,6,3,0}, {2,9,5,0}},
    {28,  6,  {5,3,0,4,1,2,8,6,7,9}, {5,0,2,9}},
    {29,  6,  {5,3,0,8,6,9,4,1,7,2}, {5,0,9,2}},
    {30,  6,  {5,4,2,3,1,0,8,7,6,9}, {5,2,0,9}},
    {31,  6,  {5,4,2,8,7,9,3,1,6,0}, {5,2,9,0}},
    {32,  6,  {5,8,9,3,6,0,4,7,1,2}, {5,9,0,2}},
    {33,  6,  {5,8,9,4,7,2,3,6,1,0}, {5,9,2,0}},
    {34,  6,  {9,6,0,7,1,2,8,3,4,5}, {9,0,2,5}},
    {35,  6,  {9,6,0,8,3,5,7,1,4,2}, {9,0,5,2}},
    {36,  6,  {9,7,2,6,1,0,8,4,3,5}, {9,2,0,5}},
    {37,  6,  {9,7,2,8,4,5,6,1,3,0}, {9,2,5,0}},
    {38,  6,  {9,8,5,6,3,0,7,4,1,2}, {9,5,0,2}},
    {39,  6,  {9,8,5,7,4,2,6,3,1,0}, {9,5,2,0}},
    {40,  10, {0,1,2,3,4,5,6,7,8,9}, {0,2,5,9}},
    {41,  10, {0,1,2,6,7,9,3,4,8,5}, {0,2,9,5}},
    {42,  10, {0,3,5,1,4,2,6,8,7,9}, {0,5,2,9}},
    {43,  10, {0,3,5,6,8,9,1,4,7,2}, {0,5,9,2}},
    {44,  10, {0,6,9,1,7,2,3,8,4,5}, {0,9,2,5}},
    {45,  10, {0,6,9,3,8,5,1,7,4,2}, {0,9,5,2}},
    {46,  10, {2,1,0,4,3,5,7,6,8,9}, {2,0,5,9}},
    {47,  10, {2,1,0,7,6,9,4,3,8,5}, {2,0,9,5}},
    {48,  10, {2,4,5,1,3,0,7,8,6,9}, {2,5,0,9}},
    {49,  10, {2,4,5,7,8,9,1,3,6,0}, {2,5,9,0}},
    {50,  10, {2,7,9,1,6,0,4,8,3,5}, {2,9,0,5}},
    {51,  10, {2,7,9,4,8,5,1,6,3,0}, {2,9,5,0}},
    {52,  10, {5,3,0,4,1,2,8,6,7,9}, {5,0,2,9}},
    {53,  10, {5,3,0,8,6,9,4,1,7,2}, {5,0,9,2}},
    {54,  10, {5,4,2,3,1,0,8,7,6,9}, {5,2,0,9}},
    {55,  10, {5,4,2,8,7,9,3,1,6,0}, {5,2,9,0}},
    {56,  10, {5,8,9,3,6,0,4,7,1,2}, {5,9,0,2}},
    {57,  10, {5,8,9,4,7,2,3,6,1,0}, {5,9,2,0}},
    {58,  10, {9,6,0,7,1,2,8,3,4,5}, {9,0,2,5}},
    {59,  10, {9,6,0,8,3,5,7,1,4,2}, {9,0,5,2}},
    {60,  10, {9,7,2,6,1,0,8,4,3,5}, {9,2,0,5}},
    {61,  10, {9,7,2,8,4,5,6,1,3,0}, {9,2,5,0}},
    {62,  10, {9,8,5,6,3,0,7,4,1,2}, {9,5,0,2}},
    {63,  10, {9,8,5,7,4,2,6,3,1,0}, {9,5,2,0}},
    {64,  1,  {0,6,9,3,8,5,1,7,4,2}, {0,9,5,2}},
    {65,  1,  {2,7,9,4,8,5,1,6,3,0}, {2,9,5,0}},
    {66,  1,  {5,8,9,4,7,2,3,6,1,0}, {5,9,2,0}},
    {67,  1,  {9,8,5,7,4,2,6,3,1,0}, {9,5,2,0}},
    {68,  3,  {0,1,2,6,7,9,3,4,8,5}, {0,2,9,5}},
    {69,  3,  {0,3,5,6,8,9,1,4,7,2}, {0,5,9,2}},
    {70,  3,  {0,6,9,3,8,5,1,7,4,2}, {0,9,5,2}},
    {71,  3,  {2,1,0,7,6,9,4,3,8,5}, {2,0,9,5}},
    {72,  3,  {2,4,5,7,8,9,1,3,6,0}, {2,5,9,0}},
    {73,  3,  {2,7,9,4,8,5,1,6,3,0}, {2,9,5,0}},
    {74,  3,  {5,3,0,8,6,9,4,1,7,2}, {5,0,9,2}},
    {75,  3,  {5,4,2,8,7,9,3,1,6,0}, {5,2,9,0}},
    {76,  3,  {5,8,9,4,7,2,3,6,1,0}, {5,9,2,0}},
    {77,  3,  {9,6,0,8,3,5,7,1,4,2}, {9,0,5,2}},
    {78,  3,  {9,7,2,8,4,5,6,1,3,0}, {9,2,5,0}},
    {79,  3,  {9,8,5,7,4,2,6,3,1,0}, {9,5,2,0}},
    {80,  6,  {0,1,2,3,4,5,6,7,8,9}, {0,2,5,9}},
    {81,  6,  {0,1,2,6,7,9,3,4,8,5}, {0,2,9,5}},
    {82,  6,  {0,3,5,1,4,2,6,8,7,9}, {0,5,2,9}},
    {83,  6,  {0,3,5,6,8,9,1,4,7,2}, {0,5,9,2}},
    {84,  6,  {0,6,9,1,7,2,3,8,4,5}, {0,9,2,5}},
    {85,  6,  {0,6,9,3,8,5,1,7,4,2}, {0,9,5,2}},
    {86,  6,  {2,1,0,4,3,5,7,6,8,9}, {2,0,5,9}},
    {87,  6,  {2,1,0,7,6,9,4,3,8,5}, {2,0,9,5}},
    {88,  6,  {2,4,5,1,3,0,7,8,6,9}, {2,5,0,9}},
    {89,  6,  {2,4,5,7,8,9,1,3,6,0}, {2,5,9,0}},
    {90,  6,  {2,7,9,1,6,0,4,8,3,5}, {2,9,0,5}},
    {91,  6,  {2,7,9,4,8,5,1,6,3,0}, {2,9,5,0}},
    {92,  6,  {5,3,0,4,1,2,8,6,7,9}, {5,0,2,9}},
    {93,  6,  {5,3,0,8,6,9,4,1,7,2}, {5,0,9,2}},
    {94,  6,  {5,4,2,3,1,0,8,7,6,9}, {5,2,0,9}},
    {95,  6,  {5,4,2,8,7,9,3,1,6,0}, {5,2,9,0}},
    {96,  6,  {5,8,9,3,6,0,4,7,1,2}, {5,9,0,2}},
    {97,  6,  {5,8,9,4,7,2,3,6,1,0}, {5,9,2,0}},
    {98,  6,  {9,6,0,7,1,2,8,3,4,5}, {9,0,2,5}},
    {99,  6,  {9,6,0,8,3,5,7,1,4,2}, {9,0,5,2}},
    {100, 6,  {9,7,2,6,1,0,8,4,3,5}, {9,2,0,5}},
    {101, 6,  {9,7,2,8,4,5,6,1,3,0}, {9,2,5,0}},
    {102, 6,  {9,8,5,6,3,0,7,4,1,2}, {9,5,0,2}},
    {103, 6,  {9,8,5,7,4,2,6,3,1,0}, {9,5,2,0}},
    {104, 10, {0,1,2,3,4,5,6,7,8,9}, {0,2,5,9}},
    {105, 10, {0,1,2,6,7,9,3,4,8,5}, {0,2,9,5}},
    {106, 10, {0,3,5,1,4,2,6,8,7,9}, {0,5,2,9}},
    {107, 10, {0,3,5,6,8,9,1,4,7,2}, {0,5,9,2}},
    {108, 10, {0,6,9,1,7,2,3,8,4,5}, {0,9,2,5}},
    {109, 10, {0,6,9,3,8,5,1,7,4,2}, {0,9,5,2}},
    {110, 10, {2,1,0,4,3,5,7,6,8,9}, {2,0,5,9}},
    {111, 10, {2,1,0,7,6,9,4,3,8,5}, {2,0,9,5}},
    {112, 10, {2,4,5,1,3,0,7,8,6,9}, {2,5,0,9}},
    {113, 10, {2,4,5,7,8,9,1,3,6,0}, {2,5,9,0}},
    {114, 10, {2,7,9,1,6,0,4,8,3,5}, {2,9,0,5}},
    {115, 10, {2,7,9,4,8,5,1,6,3,0}, {2,9,5,0}},
    {116, 10, {5,3,0,4,1,2,8,6,7,9}, {5,0,2,9}},
    {117, 10, {5,3,0,8,6,9,4,1,7,2}, {5,0,9,2}},
    {118, 10, {5,4,2,3,1,0,8,7,6,9}, {5,2,0,9}},
    {119, 10, {5,4,2,8,7,9,3,1,6,0}, {5,2,9,0}},
    {120, 10, {5,8,9,3,6,0,4,7,1,2}, {5,9,0,2}},
    {121, 10, {5,8,9,4,7,2,3,6,1,0}, {5,9,2,0}},
    {122, 10, {9,6,0,7,1,2,8,3,4,5}, {9,0,2,5}},
    {123, 10, {9,6,0,8,3,5,7,1,4,2}, {9,0,5,2}},
    {124, 10, {9,7,2,6,1,0,8,4,3,5}, {9,2,0,5}},
    {125, 10, {9,7,2,8,4,5,6,1,3,0}, {9,2,5,0}},
    {126, 10, {9,8,5,6,3,0,7,4,1,2}, {9,5,0,2}},
    {127, 10, {9,8,5,7,4,2,6,3,1,0}, {9,5,2,0}}
};

const char AlleleDepths::hash_to_color[256] = {
     0, 1, 2, 3, 4, 7,12,15, 5, 9,10,14, 6, 8,11,13,-1,-1,-1,-1,-1,-1,-1,-1,18,26,29,37,20,24,31,35,
    -1,-1,-1,-1,16,23,32,39,-1,-1,-1,-1,21,25,30,34,-1,-1,-1,-1,17,22,33,38,19,27,28,36,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,45,49,54,58,-1,-1,-1,-1,-1,-1,-1,-1,43,51,52,60,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,44,48,55,59,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,41,46,57,62,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,42,50,53,61,-1,-1,-1,-1,
    -1,-1,-1,-1,40,47,56,63,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
};

using namespace hts::bcf;

const int AlleleDepths::alleles_diploid[10][2] = {
    {0,0},
    {0,1}, {1,1},
    {0,2}, {1,2}, {2,2},
    {0,3}, {1,3}, {2,3}, {3,3}
};

const int AlleleDepths::encoded_alleles_diploid_unphased[10][2] = {
    {encode_allele_unphased(0),encode_allele_unphased(0)},
    {encode_allele_unphased(0),encode_allele_unphased(1)},
    {encode_allele_unphased(1),encode_allele_unphased(1)},
    {encode_allele_unphased(0),encode_allele_unphased(2)},
    {encode_allele_unphased(1),encode_allele_unphased(2)},
    {encode_allele_unphased(2),encode_allele_unphased(2)},
    {encode_allele_unphased(0),encode_allele_unphased(3)},
    {encode_allele_unphased(1),encode_allele_unphased(3)},
    {encode_allele_unphased(2),encode_allele_unphased(3)},
    {encode_allele_unphased(3),encode_allele_unphased(3)}
};

const int AlleleDepths::encoded_alleles_haploid[4][2] = {
    {encode_allele_unphased(0),int32_vector_end},
    {encode_allele_unphased(1),int32_vector_end},
    {encode_allele_unphased(2),int32_vector_end},
    {encode_allele_unphased(3),int32_vector_end}
};
