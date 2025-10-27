/*
Copyright (C) 2001-2022 The Exult Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "listfiles.h"

#include "utils.h"

#include <cstring>
#include <iostream>
#include <string>

using std::pmr::string;

// TODO: If SDL ever adds directory traversal to rwops, update U7ListFiles() to
//       use it.

// System Specific Code for Windows
#if defined(_WIN32)

// Need this for FindFirstFileA, FindNextFileA, FindClose
#	include <windows.h>

int U7ListFilesImp(string& path, FileList& files, bool quiet) {
	WIN32_FIND_DATAA fileinfo;
	HANDLE           handle;

	handle          = FindFirstFileA(path.c_str(), &fileinfo);
	auto last_error = GetLastError();
	// Now collect the filename
	if (handle != INVALID_HANDLE_VALUE) {
		std::string_view path_prefix = get_directory_from_path(path, true);

		do {
			const size_t nLen = strnlen(fileinfo.cFileName, MAX_PATH - 1);
			// Skip . and .. directories
			if (fileinfo.cFileName[0] == '.'
				&& (nLen == 1 || (nLen == 2 && fileinfo.cFileName[1] == '.'))
				&& (fileinfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
				continue;
			}
			string filename;
			filename.reserve(nLen + path_prefix.size());
			filename = path_prefix;
			filename += fileinfo.cFileName;

#	ifdef DEBUG
			if (!quiet) {
				std::cerr << filename << std::endl;
			}
#	endif
			files.push_back(std::move(filename));

		} while (FindNextFileA(handle, &fileinfo));
		last_error = GetLastError();
	} else if (last_error == ERROR_FILE_NOT_FOUND) {
		// No files found is not an error treat it as no more files
		last_error = ERROR_NO_MORE_FILES;
	}

	bool failed = last_error != ERROR_NO_MORE_FILES;
	if (failed && !quiet) {
		std::cerr << "U7ListFiles: Error listing files for mask " << path
				  << ", error code " << last_error << std::endl;
		LPSTR lpMsgBuf;
		if (FormatMessageA(
					FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
							| FORMAT_MESSAGE_IGNORE_INSERTS,
					nullptr, last_error,
					MAKELANGID(
							LANG_NEUTRAL,
							SUBLANG_DEFAULT),    // Default language
					reinterpret_cast<LPSTR>(&lpMsgBuf), 0, nullptr)
			!= 0) {
			std::cerr << lpMsgBuf << std::endl;
			LocalFree(lpMsgBuf);
		} else {
			auto fm_last_error = GetLastError();
			std::cerr << "FormatMessage failed with error " << fm_last_error
					  << " while trying to get error message when "
						 "listing files"
					  << std::endl;
		}
	}

#	ifdef DEBUG
	if (!quiet) {
		std::cerr << files.size() << " filenames" << std::endl;
	}
#	endif

	if (handle != INVALID_HANDLE_VALUE) {
		FindClose(handle);
	}
	return failed ? last_error : 0;
}

#else    // This system has glob.h

#	include <glob.h>

#	ifdef ANDROID
#		ifdef __GNUC__
#			pragma GCC diagnostic push
#			pragma GCC diagnostic ignored "-Wold-style-cast"
#			pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#			if !defined(__llvm__) && !defined(__clang__)
#				pragma GCC diagnostic ignored "-Wuseless-cast"
#			endif
#		endif    // __GNUC__
#		include <SDL3/SDL_system.h>
#		ifdef __GNUC__
#			pragma GCC diagnostic pop
#		endif    // __GNUC__
#	endif

static int U7ListFilesImp(const string& path, FileList& files, bool quiet) {
	glob_t globres;
	int    err = glob(path.c_str(), GLOB_NOSORT, nullptr, &globres);

	switch (err) {
	case 0:    // OK
		for (size_t i = 0; i < globres.gl_pathc; i++) {
			files.push_back(globres.gl_pathv[i]);
		}
		globfree(&globres);
		return 0;
	case 3:    // no matches
		return 0;
	default:    // error
		if (!quiet) {
			std::cerr << "Glob error " << err << std::endl;
		}
		return err;
	}
}
#endif
int U7ListFiles(std::string_view mask, FileList& files, bool quiet) {
	string path(get_system_path(mask));
	int    result = U7ListFilesImp(path, files, quiet);
#ifdef ANDROID
	// TODO: If SDL ever adds directory traversal to rwops use it instead of
	// glob() so that we pick up platform-specific paths and behaviors like
	// this.
	if (result != 0) {
		result = U7ListFilesImp(
				SDL_GetAndroidInternalStoragePath() + ("/" + path), files,
				quiet);
	}
#endif
	return result;
}
