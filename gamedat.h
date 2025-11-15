/*

GAMEDAT Directiory and SaveGame Handling

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

#include <array>
#include <algorithm>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>
#include <future>
#include <mutex>
#include "singles.h"

class IDataSource;
class Flex_writer;
class unzFile;
class zipFile;
class Shape_file;

#define MAX_SAVEGAME_NAME_LEN 0x50

class GameDat : protected Game_singletons {

public:
struct SaveGame_Details {
	bool good = false;

	operator bool() const {
		return good;
	}

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

	short save_count;        // 12
	char  old_party_size;    // 13 Party size is stored on disk here but this
							 // field is no longer used

	char unused;    // 14 Quite literally unused

	char real_second;    // 15

	// Incase we want to add more later
	char reserved0;        // 16
	char reserved1[48];    // 64

		// Compare functions for display sorting Newer comes first 
	int CompareRealTime(const SaveGame_Details& other) const noexcept;
	int CompareGameTime(const SaveGame_Details& other) const noexcept;
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

	std::string                 savename;
	bool                        readable = false;
	SaveGame_Details            details;
	std::vector<SaveGame_Party> party;
	std::unique_ptr<Shape_file> screenshot;

	// Default constructor is allowed
	SaveInfo() {}

	// Move Costructor from a std::string filename
	SaveInfo(std::string&& filename);

	// No copy constructor as screenshot can't be copied because Shape_file has
	// no copy constructor

	SaveInfo(const SaveInfo&) = delete;
	SaveInfo(SaveInfo&&)      = default;

	// Copy from exising object but with move for a Screenshot
	SaveInfo(const SaveInfo& other, std::unique_ptr<Shape_file>&& newscreenshot)
			: filename_(other.filename_), num(other.num),
			  savename(other.savename), readable(other.readable),
			  details(other.details), party(other.party),
			  screenshot(std::move(newscreenshot)) {
		// Copy the party
	}

	// No copy assignment operator, only move
	SaveInfo& operator=(const SaveInfo&) = delete;
	SaveInfo& operator=(SaveInfo&&)      = default;
 
	enum class Type {
		UNKNOWN = -1,
		REGULAR = 0,
		AUTOSAVE,
		QUICKSAVE,
		CRASHSAVE,
		NUM_TYPES
	} type = Type::UNKNOWN;

	static const int NUM_TYPES = static_cast<int>(Type::NUM_TYPES);

	// const getter for filename
	const std::string& filename() const {
		return filename_;
	}

		int compare(const SaveInfo& other) const noexcept;

		bool operator<(const SaveInfo& other) const noexcept {
			return compare(other) < 0;
		}
};
	int save_count;

	std::vector<SaveInfo>                save_infos;
	std::array<int, SaveInfo::NUM_TYPES> first_free;
	std::array<int, SaveInfo::NUM_TYPES> oldest;
	std::string                          save_mask;
	std::shared_future<void>             saveinfo_future;
	std::recursive_mutex                 save_info_mutex;

	static void init(){
	gamedat = new GameDat();

	}

public:
	friend class Game_Window;
	GameDat();

	static GameDat* get() { return gamedat;}

	void read_save_infos_async(bool force);
	void get_saveinfo(
			std::unique_ptr<Shape_file>& map, SaveGame_Details& details,
			std::vector<SaveGame_Party>& party, bool current);

	// Get Vector of all savegame info
	const std::vector<SaveInfo>* GetSaveGameInfos(bool force);

	void wait_for_saveinfo_read();


	// Get the filename for savegame num of specified SaveInfo:Type
	std::string get_save_filename(int num, SaveInfo::Type type);


	// Emergency save Creates a new save in the next available index
	// It preserves the existing GAMEDAT
	// and it does not paint the saving game message on screen or create the
	// miniscreenshot
	void MakeEmergencySave(const char* savename = nullptr);

	void write_saveinfo(
			bool screenshot = true);    // Write the save info to gamedat
	// Explode a savegame into "gamedat".
	void restore_gamedat(const char* fname);

	// Save "gamedat".
	void save_gamedat(const char* fname, const char* savename);

	// Save gamedat to a new savegame of the specified SaveInfo:Type with the
	// given name
	void save_gamedat(SaveInfo::Type type, const char* savename);
	void read_saveinfo();    // Read the save info from gamedat
	void read_saveinfo(bool newgame);    // Read the save info from gamedat

	void restore_flex_files(IDataSource& in, const char* basepath);

 	bool get_saveinfo(
		const std::string& filename, std::string& name,
		std::unique_ptr<Shape_file>& map, SaveGame_Details& details,
		std::vector<SaveGame_Party>& party);

	void clear_saveinfos();

void SavefileFromDataSource(
		Flex_writer& flex,
		IDataSource& source,    // read from here
		const char*  fname      // store data using this filename
);
void Savefile(
		Flex_writer& flex,
		const char*  fname    // Name of file to save.
);
void save_gamedat_chunks(Game_map* map, Flex_writer& flex);

bool read_saveinfo(
		IDataSource* in, SaveGame_Details& details,
		std::vector<SaveGame_Party>& party);
#ifdef HAVE_ZIP_SUPPORT
	bool get_saveinfo_zip(
			const char* fname, std::string& name,
			std::unique_ptr<Shape_file>& map, SaveGame_Details& details,
			std::vector<SaveGame_Party>& party);
	bool save_gamedat_zip(const char* fname, const char* savename);
	bool Restore_level2(unzFile& unzipfile, const char* dirname, int dirlen);
	bool restore_gamedat_zip(const char* fname);
	bool Save_level2(zipFile& zipfile, const char* fname);
	bool Save_level1(zipFile& zipfile, const char* fname,bool rquired = true);
	bool Begin_level2(zipFile& zipfile, int mapnum);
	bool End_level2(zipFile& zipfile);
#endif

	void read_save_infos();


public:
	// Queue an autosave to occur at the next possible time. Safe to call at any
	// time including during usecode execution
	void Queue_Autosave(
			int gflag = -1, int map_from = -1, int map_to = -1,
			int sc_from = -1, int sc_to = -1);

	void Autosave_Now(
			bool write_gamedat = false, const char* savemessage = nullptr,
			int gflag = -1, int map_from = -1, int map_to = -1,
			int sc_from = -1, int sc_to = -1);

	void Quicksave();

	void Savegame(const char* fname, const char* savename);
	void Savegame(const char* savename);

	void Extractgame(const char* fname, bool doread);

	void ResortSaveInfos();
	
	// Delete a savegame file. This will invalidate any existing SaveInfo
	// vectors
	void DeleteSaveGame(const std::string& fname);
};

#endif    // SAVEINFO_H_INCLUDED
