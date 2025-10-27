/*
 *  utils.h - Common utility routines.
 *
 *  Copyright (C) 1998-1999  Jeffrey S. Freedman
 *  Copyright (C) 2000-2022  The Exult Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef UTILS_H
#define UTILS_H

#include "common_types.h"

#include <dirent.h>

#include <cstring>
#include <functional>
#include <iosfwd>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <string_view>

#ifndef ATTR_PRINTF
#	ifdef __GNUC__
#		define ATTR_PRINTF(x, y) __attribute__((format(printf, (x), (y))))
#	else
#		define ATTR_PRINTF(x, y)
#	endif
#endif

inline int ReadInt(std::istream& in, int def = 0) {
	int num;
	if (in.eof()) {
		return def;
	}
	in >> num;
	if (in.fail()) {
		return def;
	}
	in.ignore(std::numeric_limits<std::streamsize>::max(), '/');
	return num;
}

inline unsigned int ReadUInt(std::istream& in, int def = 0) {
	unsigned int num;
	if (in.eof()) {
		return def;
	}
	in >> num;
	if (in.fail()) {
		return def;
	}
	in.ignore(std::numeric_limits<std::streamsize>::max(), '/');
	return num;
}

inline void WriteInt(std::ostream& out, int num, bool final = false) {
	out << num;
	if (final) {
		out << std::endl;
	} else {
		out << '/';
	}
}

inline void WriteInt(std::ostream& out, unsigned int num, bool final = false) {
	out << num;
	if (final) {
		out << std::endl;
	} else {
		out << '/';
	}
}

inline std::string ReadStr(char*& eptr, int off = 1) {
	eptr += off;
	char*       pos = std::strchr(eptr, '/');
	std::string retval(eptr, pos - eptr);
	eptr = pos;
	return retval;
}

inline std::string ReadStr(std::istream& in) {
	std::string retval;
	std::getline(in, retval, '/');
	return retval;
}

inline void WriteStr(
		std::ostream& out, const std::string& str, bool final = false) {
	out << str;
	if (final) {
		out << std::endl;
	} else {
		out << '/';
	}
}

/*
 *  Get file size without undefined behavior.
 */
inline size_t get_file_size(std::istream& in) {
	const auto start = in.tellg();
	in.seekg(0);
	in.ignore(std::numeric_limits<std::streamsize>::max());
	const size_t len = in.gcount();
	in.seekg(start);
	return len;
}

// Sets factories for creating istreams/ostreams.  Intended to be called once
// during initialization before using any U7open...() calls and is not
// guaranteed to be thread-safe.
template <typename streampointer>
using TU7streamFactory = std::function<streampointer(
		const char* s, std::ios_base::openmode mode)>;

using U7IstreamFactory = TU7streamFactory<std::unique_ptr<std::istream>>;

using U7OstreamFactory = TU7streamFactory<std::unique_ptr<std::ostream>>;

void U7set_istream_factory(U7IstreamFactory factory);
void U7set_ostream_factory(U7OstreamFactory factory);

// Manually sets the home directory rather than trying to infer it from the
// environment. Intended to be called once during initialization before using
// U7open...() calls and is not guaranteed to be thread-safe.
void U7set_home(std::string home);

std::unique_ptr<std::istream> U7open_in(
		const std::string& fname,
		bool is_text = false    // Should the file be opened in text mode
);

inline std::unique_ptr<std::istream> U7open_in(
		const char* fname,
		bool        is_text = false    // Should the file be opened in text mode
) {
	// forward to std::string version
	return U7open_in(std::string(fname), is_text);
}

std::shared_ptr<std::istream> U7open_in(
		const std::pmr::string& fname,
		bool is_text = false    // Should the file be opened in text mode
);
std::unique_ptr<std::ostream> U7open_out(
		const std::string& fname,
		bool is_text = false    // Should the file be opened in text mode
);
std::shared_ptr<std::ostream> U7open_out(
		const std::pmr::string& fname,
		bool is_text = false    // Should the file be opened in text mode
);

inline std::unique_ptr<std::ostream> U7open_out(
		const char* fname,
		bool        is_text = false    // Should the file be opened in text mode
) {
	// forward to std::string version
	return U7open_out(std::string(fname), is_text);
}

std::unique_ptr<std::istream> U7open_static(const char* fname, bool is_text);
DIR*                          U7opendir(const std::pmr::string& fname);
DIR*                          U7opendir(const std::string& fname);

inline DIR* U7opendir(const char* fname) {
	return U7opendir(std::string(fname));
}

void U7remove(const std::string& fname);
void U7remove(const std::pmr::string& fname);

inline void U7remove(const char* fname) {
	return U7remove(std::string(fname));
}

bool U7exists(const std::string& fname);
bool U7exists(const std::pmr::string& fname);

inline bool U7exists(const char* fname) {
	return U7exists(std::string(fname));
}

int U7mkdir(const std::string& dirname, int mode = 0755, bool parents = false);
int U7mkdir(
		const std::pmr::string& dirname, int mode = 0755, bool parents = false);

inline int U7mkdir(const char* dirname, int mode = 0755, bool parents = false) {
	return U7mkdir(std::string(dirname), mode, parents);
}

int U7rmdir(const std::pmr::string& dirname, bool recursive);
int U7rmdir(const std::string& dirname, bool recursive);

inline int U7rmdir(const char* dirname, bool recursive) {
	return U7rmdir(std::string(dirname), recursive);
}

#ifdef _WIN32
void redirect_output(const char* prefix = "std");
void cleanup_output(const char* prefix = "std");
#endif
void setup_data_dir(const std::string& data_path, const char* runpath);
void setup_program_paths();
#if defined(MACOSX) || defined(SDL_PLATFORM_IOS)
void setup_app_bundle_resource();
#endif

int U7chdir(const char* dirname);

void U7copy(const char* src, const char* dest);

bool is_system_path_defined(std::string_view path);
void store_system_paths();
void reset_system_paths();
void clear_system_path(const std::string& key);
void add_system_path(const std::string& key, const std::string& value);
void clone_system_path(const std::string& new_key, const std::string& old_key);
std::string      get_system_path(const std::string& path);
std::pmr::string get_system_path(const std::pmr::string& path);

inline std::string get_system_path(const char* path) {
	std::string sp(path);
	return get_system_path(sp);
}

#define BUNDLE_CHECK(x, y) \
	((is_system_path_defined("<BUNDLE>") && U7exists((x))) ? (x) : (y))

void        to_uppercase(std::string& str);
std::string to_uppercase(const std::string& str);

int    Log2(uint32 n);
uint32 msb32(uint32 x);
int    fgepow2(uint32 n);

char* newstrdup(const char* s);
char* Get_mapped_name(const char* from, int num, char* to);
int   Find_next_map(int start, int maxtry);

std::string_view get_filename_from_path(std::string_view path);
std::string_view get_directory_from_path(
		std::string_view path, bool keepslash = false);

template <typename Base>
class MakeTransparentClass : public Base {
public:
	using Base::Base;
	using is_transparent = void;
};

#endif /* _UTILS_H_ */
