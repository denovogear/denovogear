/*
 * Copyright (c) 2014-2016 Reed A. Cartwright
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

#include <dng/utility.h>

#include <boost/filesystem/convenience.hpp>

namespace dng { namespace utility {

// extracts extension and filename from both file.ext and ext:file.foo
// returns {ext, filename.ext}
// trims whitespace as well
std::pair<std::string, std::string> extract_file_type(const char* path) {
    assert(path != nullptr);
    std::locale loc;

    // trim initial whitespace
    const char *sp = path;
    while(*sp != 0 && std::isspace(*sp,loc)) {
        ++sp;
    }
    if(*sp == 0) {
        return {};
    }
    // Identify first colon, last dot, and last not whitespace
    const char *colon = nullptr;
    const char *dot = nullptr;
    const char *ep = sp;
    for(const char *p = sp; *p != 0; ++p) {
        if(*p == ':' && colon == nullptr) {
            colon = p;
            ep = p;
        } else if(*p == '.' && p != sp) {
            dot = p;
            ep = p;
        } else if(!std::isspace(*p,loc)) {
            ep = p;
        }
    }
    if(colon != nullptr) {
        assert(sp <= colon);
        assert(colon <= ep);
        return {std::string{sp, colon}, std::string{colon+1,ep+1}};
    } else if(dot != nullptr) {
        assert(dot <= ep);
        assert(sp <= ep);
        return {std::string{dot+1,ep+1}, std::string{sp,ep+1}};
    }
    assert(sp <= ep);
    return {{}, std::string{sp,ep+1}};
}

static const std::string file_category_keys[] = {
    "bam","sam","cram",
    "bcf","vcf",
    "ad","tad"
};

FileCat file_category(const std::string &ext) {
    switch(key_switch_iequals(ext, file_category_keys)) {
    case 0:
    case 1:
    case 2:
        return FileCat::Sequence;
    case 3:
    case 4:
        return FileCat::Variant;
    case 5:
    case 6:
    	return FileCat::Pileup;
    default:
    	break;
    };
    return FileCat::Unknown;
}


FileCat input_category(const std::string &in, FileCatSet mask, FileCat def) {

    std::string ext = extract_file_type(in).first;
    if(ext == "gz" || ext == "gzip" || ext == "bgz") {
    	std::string extr = boost::filesystem::change_extension(in, "").string();
    	ext = extract_file_type(extr).first;
    }

    FileCat cat = file_category(ext);
    if(cat == FileCat::Unknown)
        cat = def;
    if(mask & cat) {
        return cat;
    } else {
        throw std::runtime_error("Argument error: file type '" + ext + "' not supported. Input file was '" + in + "'.");
    }
    return FileCat::Unknown;
}

std::pair<std::string, std::string> timestamp() {
    using namespace std;
    using namespace std::chrono;
    std::string buffer(127, '\0');
    auto now = system_clock::now();
    auto now_t = system_clock::to_time_t(now);
    size_t sz = strftime(&buffer[0], 127, "%FT%T%z",
                         localtime(&now_t));
    buffer.resize(sz);
    auto epoch = std::chrono::duration_cast<std::chrono::milliseconds>(
                     now.time_since_epoch());
    return {buffer, to_string(epoch.count())};
}

std::string vcf_timestamp() {
    auto stamp = timestamp();
    return "Date=" + stamp.first + ",Epoch=" + stamp.second;
}

} }
