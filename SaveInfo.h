/*
Copyright (C) 2025 The Exult Team

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
#ifndef SAVEINFO_H_INCLUDED
#define SAVEINFO_H_INCLUDED

#include <ctype.h>

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <string>

class Shape_file;

#define MAX_SAVEGAME_NAME_LEN 0x50

struct SaveGame_Details {
	// Time that the game was saved (needed????)
	char  real_minute;    // 1
	char  real_hour;      // 2
	char  real_day;       // 3
	char  real_month;     // 4
	short real_year;      // 6

	// The Game Time that the save was done at
	char  game_minute;    // 7
	char  game_hour;      // 8
	short game_day;       // 10

	short save_count;    // 12
	char  party_size;    // 13

	char unused;    // 14 Quite literally unused

	char real_second;    // 15

	// Incase we want to add more later
	char reserved0;        // 16
	char reserved1[48];    // 64
};

struct SaveGame_Party {
	char         name[18];    // 18
	short        shape;       // 20
	unsigned int exp;         // 24
	unsigned int flags;       // 28
	unsigned int flags2;      // 32

	unsigned char food;        // 33
	unsigned char str;         // 34
	unsigned char combat;      // 35
	unsigned char dext;        // 36
	unsigned char intel;       // 37
	unsigned char magic;       // 38
	unsigned char mana;        // 39
	unsigned char training;    // 40
	short         health;      // 42

	short shape_file;    // 44

	// Incase we want to add more later
	int reserved1;    // 48
	int reserved2;    // 52
	int reserved3;    // 56
	int reserved4;    // 60
	int reserved5;    // 64
};

class SaveInfo {
	// No direct access to filename
	std::string filename_;

public:
	int num = -1;

	std::string                       savename;
	bool                              readable = false;
	std::unique_ptr<SaveGame_Details> details;
	std::unique_ptr<SaveGame_Party[]> party;
	std::unique_ptr<Shape_file>       screenshot;

	// Default constructor is allowed
	SaveInfo() {}

	// Move Costructor from a std::string filename
	SaveInfo(std::string&& filename) : filename_(std::move(filename)) {
		// Filename is likely a path so find the last directory separator
		size_t filename_start = filename_.find_last_of("/\\");

		// No diretory separators so actual filename starts at 0
		if (filename_start == std::string::npos) {
			filename_start = 0;
		}
		// Find where the savenume number starts
		size_t number_start
				= filename_.find_first_of("0123456789", filename_start);

		if (number_start == std::string::npos
			|| number_start < filename_start + 5) {
			// the savegame filename is not in the expected format
			// this should never happen as filename glob should only list
			// filenames in mostly the correct format
			// EXULT*.sav
			// Don't attempt to parse the savegame number
			num  = -1;
			type = UNKNOWN;
			return;
			// quicksaves have Q before the number
		} else if (std::tolower(filename_[number_start - 1]) == 'q') {
			type = QUICKSAVE;
			// autosaves have A before the number
		} else if (std::tolower(filename_[number_start - 1]) == 'a') {
			type = AUTOSAVE;
			// crashsaves have C before the number
		} else if (std::tolower(filename_[number_start - 1]) == 'c') {
			type = CRASHSAVE;
			// regular saves have t from exult as character before the number
		} else if (std::tolower(filename_[number_start - 1]) == 't') {
			type = REGULAR;
		} else {
			// Filename format is unknown
			num  = -1;
			type = UNKNOWN;
			return;
		}

		num = strtol(filename_.c_str() + number_start, nullptr, 10);
	}

	// No copy constructor as screenshot can't be copied because Shape_file has
	// no copy constructor

	SaveInfo(const SaveInfo&) = delete;
	SaveInfo(SaveInfo&&)      = default;

	// Copy from exising object but with move for a Screenshot
	SaveInfo(const SaveInfo& other, std::unique_ptr<Shape_file>&& newscreenshot)
			: filename_(other.filename_), num(other.num),
			  savename(other.savename), readable(other.readable),
			  details(std::make_unique<SaveGame_Details>(*other.details)),
			  party(std::make_unique<SaveGame_Party[]>(
					  other.details->party_size)),
			  screenshot(std::move(newscreenshot)) {
		// Copy the party
		std::copy(
				other.party.get(), other.party.get() + details->party_size,
				party.get());
	}

	// No copy assignment operator, only move
	SaveInfo& operator=(const SaveInfo&) = delete;
	SaveInfo& operator=(SaveInfo&&)      = default;

	enum Type {
		UNKNOWN = -1,
		REGULAR = 0,
		AUTOSAVE,
		QUICKSAVE,
		CRASHSAVE,
		NUM_TYPES
	} type = UNKNOWN;

	// const getter for filename
	const std::string& filename() const {
		return filename_;
	}

	int compare(const SaveInfo* other) const noexcept {
		// First by type in reverse order
		if (type != other->type) {
			return other->type - type;
		}
		// Check by time first, if possible
		if (details && other->details) {
			if (details->real_year < other->details->real_year) {
				return 1;
			}
			if (details->real_year > other->details->real_year) {
				return -1;
			}

			if (details->real_month < other->details->real_month) {
				return 1;
			}
			if (details->real_month > other->details->real_month) {
				return -1;
			}

			if (details->real_day < other->details->real_day) {
				return 1;
			}
			if (details->real_day > other->details->real_day) {
				return -1;
			}

			if (details->real_hour < other->details->real_hour) {
				return 1;
			}
			if (details->real_hour > other->details->real_hour) {
				return -1;
			}

			if (details->real_minute < other->details->real_minute) {
				return 1;
			}
			if (details->real_minute > other->details->real_minute) {
				return -1;
			}

			if (details->real_second < other->details->real_second) {
				return 1;
			}
			if (details->real_second > other->details->real_second) {
				return -1;
			}
		} else if (details) {    // If the other doesn't have time we are first
			return -1;
		} else if (other->details) {    // If we don't have time we are last
			return 1;
		}

		// Lastly just sort by filename
		return filename_.compare(other->filename_);
	}

	bool operator<(const SaveInfo& other) const noexcept {
		return compare(&other) < 0;
	}
};

#endif    // SAVEINFO_H_INCLUDED
