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
#pragma once
#ifndef DNG_DETAIL_UNIT_TEST_H
#define DNG_DETAIL_UNIT_TEST_H

#ifdef BOOST_TEST_MODULE
#include <boost/test/unit_test.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

// Declare a class capable of preforming unit tests on public, private, and protected members.
#define DNG_UNIT_TEST(name) \
    friend class name

// Declare a nullary function capable of preforming unit tests on public, private, and protected members.
#define DNG_UNIT_TEST_FUNCTION(name) \
    friend void name()


namespace dng {
namespace detail {

	// RAII class that creates and opens a temporary file for reading and writing
	// and deletes it when done.
	struct AutoTempFile {
    boost::filesystem::path path;
    boost::filesystem::fstream file;

    AutoTempFile() {
        using namespace boost::filesystem;
        path = temp_directory_path();
        path /= unique_path();
        file.open(path, std::ios::out); //create file
        file.close();
        file.open(path); // open rw
    } 
    ~AutoTempFile() {
        using namespace boost::filesystem;
        file.close();
        remove(path);
    }
};

}
}

#else

#define DNG_UNIT_TEST(name) 

#define DNG_UNIT_TEST_FUNCTION(name) 

#endif

#endif // DNG_DETAIL_UNIT_TEST_H
