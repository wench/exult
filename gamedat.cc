/*
 *  gamedat.cc - Create gamedat files from a savegame.
 *
 *  Copyright (C) 2000-2025  The Exult Team
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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "gamedat.h"

#include "Audio.h"
#include "Configuration.h"
#include "Flex.h"
#include "Newfile_gump.h"
#include "Notebook_gump.h"
#include "Settings.h"
#include "Yesno_gump.h"
#include "actors.h"
#include "cheat.h"
#include "databuf.h"
#include "exceptions.h"
#include "exult.h"
#include "fnames.h"
#include "game.h"
#include "gameclk.h"
#include "gamemap.h"
#include "gamewin.h"
#include "listfiles.h"
#include "mouse.h"
#include "party.h"
#include "span.h"
#include "ucmachine.h"
#include "utils.h"
#include "version.h"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#	include <io.h>
#endif

#if defined(XWIN)
#	include <sys/stat.h>
#endif

// Zip file support
#ifdef HAVE_ZIP_SUPPORT
#	include "files/zip/unzip.h"
#	include "files/zip/zip.h"
#endif

using std::cout;
using std::endl;
using std::ifstream;
using std::ios;
using std::istream;
using std::localtime;
using std::ofstream;
using std::ostream;
using std::size_t;
using std::strchr;
using std::strcmp;
using std::string;
using std::stringstream;
using std::strncpy;
using std::time;
using std::time_t;
using std::tm;

void GameDat::clear_saveinfos() {
	std::lock_guard lock(save_info_mutex);
	save_infos.clear();
	saveinfo_future = std::shared_future<void>();
}

/*
 *  Write files from flex assuming first 13 characters of
 *  each flex object are an 8.3 filename.
 */
void GameDat::restore_flex_files(IDataSource& in, const char* basepath) {
	in.seek(0x54);    // Get to where file count sits.
	const size_t numfiles = in.read4();
	in.seek(0x80);    // Get to file info.

	// Read pos., length of each file.
	struct file_info {
		size_t location;
		size_t length;
	};

	std::vector<file_info> finfo(numfiles);
	for (auto& [location, length] : finfo) {
		location = in.read4();    // The position, then the length.
		length   = in.read4();
	}
	const size_t baselen = strlen(basepath);
	for (const auto& [location, length] : finfo) {    // Now read each file.
		// Get file length.
		size_t len = length;
		if (len <= 13) {
			continue;
		}
		len -= 13;
		in.seek(location);    // Get to it.
		char fname[50];       // Set up name.
		strcpy(fname, basepath);
		in.read(&fname[baselen], 13);
		size_t namelen = strlen(fname);
		// Watch for names ending in '.'.
		if (fname[namelen - 1] == '.') {
			fname[namelen - 1] = 0;
		}
		// Now read the file.
		auto buf = in.readN(len);
		if (!memcmp(&fname[baselen], "map", 3)) {
			// Multimap directory entry.
			// Just for safety, we will force-terminate the filename
			// at an appropriate position.
			namelen        = baselen + 5;
			fname[namelen] = 0;

			IBufferDataView ds(buf, len);
			if (!Flex::is_flex(&ds)) {
				// Save is most likely corrupted. Ignore the file but keep
				// reading the savegame.
				std::cerr << "Error reading flex: file '" << fname
						  << "' is not a valid flex file. This probably means "
							 "a corrupt save game."
						  << endl;
			} else {
				// fname should be a path hare.
				U7mkdir(fname, 0755);
				// Append trailing slash:
				fname[namelen]     = '/';
				fname[namelen + 1] = 0;
				restore_flex_files(ds, fname);
			}
			continue;
		}
		auto pOut = U7open_out(fname);
		if (!pOut) {
			gwin->abort("Error opening '%s'.", fname);
		}
		OStreamDataSource sout(pOut.get());
		sout.write(buf.get(), len);    // Then write it out.
		if (!sout.good()) {
			gwin->abort("Error writing '%s'.", fname);
		}
		gwin->cycle_load_palette();
	}
}

// In gamemgr/modmgr.cc because it is also needed by ES.
extern string get_game_identity(const char* savename, const string& title);

/*
 *  Write out the gamedat directory from a saved game.
 *
 *  Output: Aborts if error.
 */

void GameDat::restore_gamedat(
		const char* fname    // Name of savegame file.
) {
	// Check IDENTITY.
	string       id = get_game_identity(fname, Game::get_gametitle());
	const string static_identity
			= get_game_identity(INITGAME, Game::get_gametitle());
	bool user_ignored_identity_mismatch = false;
	// Note: "*" means an old game.
	if (id.empty() || (id[0] != '*' && static_identity != id)) {
		std::string msg("Wrong identity '");
		msg += id;
		msg += "'.  Open anyway?";
		if (!Yesno_gump::ask(msg.c_str())) {
			return;
		}
		user_ignored_identity_mismatch = true;
	}
	// Check for a ZIP file first
#ifdef HAVE_ZIP_SUPPORT
	if (restore_gamedat_zip(fname)) {
		return;
	}
#endif

	// Display red plasma during load...
	gwin->setup_load_palette();

	U7mkdir("<GAMEDAT>", 0755);    // Create dir. if not already there. Don't
	// use GAMEDAT define cause that's got a
	// trailing slash
	IFileDataSource in(fname);
	if (!in.good()) {
		if (!Game::is_editing()
			&& Game::get_game_type()
					   != EXULT_DEVEL_GAME) {    // Ok if map-editing or devel
												 // game.
			throw file_read_exception(fname);
		}
		std::cerr << "Warning (map-editing): Couldn't open '" << fname << "'"
				  << endl;
		return;
	}

	U7remove(USEDAT);
	U7remove(USEVARS);
	U7remove(U7NBUF_DAT);
	U7remove(NPC_DAT);
	U7remove(MONSNPCS);
	U7remove(FLAGINIT);
	U7remove(GWINDAT);
	U7remove(IDENTITY);
	U7remove(GSCHEDULE);
	U7remove("<STATIC>/flags.flg");
	U7remove(GSCRNSHOT);
	U7remove(GSAVEINFO);
	U7remove(GNEWGAMEVER);
	U7remove(GEXULTVER);
	U7remove(KEYRINGDAT);
	U7remove(NOTEBOOKXML);

	cout.flush();

	restore_flex_files(in, GAMEDAT);

	cout.flush();

	gwin->load_finished();

	if (user_ignored_identity_mismatch)
	{
		// create new identity file if user agreed to open
		OFileDataSource out(IDENTITY);
		out.write(static_identity);
	}
}


/*
 sof 'gamed*/

constexpr static const std::array bgsavefiles{
		GEXULTVER, GNEWGAMEVER, NPC_DAT, MONSNPCS,  USEVARS,
		USEDAT,    FLAGINIT,    GWINDAT, GSCHEDULE, NOTEBOOKXML};

constexpr static const std::array sisavefiles{
		GEXULTVER, GNEWGAMEVER, NPC_DAT,   MONSNPCS,   USEVARS,    USEDAT,
		FLAGINIT,  GWINDAT,     GSCHEDULE, KEYRINGDAT, NOTEBOOKXML};

void GameDat::SavefileFromDataSource(
		Flex_writer& flex,
		IDataSource& source,    // read from here
		const char*  fname      // store data using this filename
) {
	flex.write_file(fname, source);
}

/*
 *  Save a single file into an IFF repository.
 */

void GameDat::Savefile(
		Flex_writer& flex,
		const char*  fname    // Name of file to save.
) {
	IFileDataSource source(fname);
	if (!source.good()) {
		if (Game::is_editing()) {
			return;    // Newly developed game.
		}
		throw file_read_exception(fname);
	}
	SavefileFromDataSource(flex, source, fname);
}

inline void GameDat::save_gamedat_chunks(Game_map* map, Flex_writer& flex) {
	for (int schunk = 0; schunk < 12 * 12; schunk++) {
		char iname[128];
		// Check to see if the ireg exists before trying to
		// save it; prevents crash when creating new maps
		// for existing games
		if (U7exists(map->get_schunk_file_name(U7IREG, schunk, iname))) {
			Savefile(flex, iname);
		} else {
			flex.empty_object();    // TODO: Get rid of this by making it
									// redundant.
		}
	}
}

/*
 *  Save 'gamedat' into a given file.
 *
 *  Output: 0 if error (reported).
 */

void GameDat::save_gamedat(
		const char* fname,      // File to create.
		const char* savename    // User's savegame name.
) {
	// First check for compressed save game

	
#ifdef HAVE_ZIP_SUPPORT
	// Try to save as a zip file
	if (Settings::get().disk.save_compression_level > 0 && save_gamedat_zip(fname, savename)) {
		read_save_infos_async(true);
		return;
	}
#endif

	// setup correct file list
	tcb::span<const char* const> savefiles;
	if (Game::get_game_type() == BLACK_GATE) {
		savefiles = bgsavefiles;
	} else {
		savefiles = sisavefiles;
	}

	// Count up #files to write.
	// First map outputs IREG's directly to
	// gamedat flex, while all others have a flex
	// of their own contained in gamedat flex.
	size_t count = savefiles.size() + 12 * 12 + 2;	
	for (auto* map : gwin->get_maps()) {
		if (map) {
			count++;
		}
	}
	// Use samename for title.
	OFileDataSource out(fname);
	Flex_writer     flex(out, savename, count);
	
	// We need to explicitly save these as they are no longer included in
	// savefiles span and must be first
	// Screenshot and Saveinfo are optional
	// Identity is required
	if(U7exists(GSCRNSHOT))
	{
		Savefile(flex, GSCRNSHOT);
	}
	if(U7exists(GSAVEINFO))
	{
		Savefile(flex, GSAVEINFO);
	}
	Savefile(flex, IDENTITY);

for (const auto* savefile : savefiles) {
		Savefile(flex, savefile);
	}
	// Now the Ireg's.
	for (auto* map : gwin->get_maps()) {
		if (!map) {
			continue;
		}
		if (!map->get_num()) {
			// Map 0 is a special case.
			save_gamedat_chunks(map, flex);
		} else {
			// Multimap directory entries. Each map is stored in their
			// own flex file contained inside the general gamedat flex.
			{
				char dname[32];
				map->get_mapped_name(GAMEDAT, dname);
				Flex_writer mapflex = flex.start_nested_flex(dname, 12 * 12);
				// Save chunks to nested flex
				save_gamedat_chunks(map, mapflex);
			}
		}
	}
	read_save_infos_async(true);
}

void GameDat::DeleteSaveGame(const std::string& fname) {
	U7remove(fname);

	// Update save_info
	std::lock_guard lock(save_info_mutex);

	auto it = std::find_if(
			save_infos.begin(), save_infos.end(), [&fname](const SaveInfo& si) {
				return si.filename() == fname;
			});


	if (it != save_infos.end()) {

		int itype = static_cast<int>(it->type);

		// Update first_free if needed
		if (first_free[itype] > it->num) {
			first_free[itype] = it->num;
		}

		save_infos.erase(it);
	}

}

std::string GameDat::get_save_filename(int num, SaveInfo::Type type) {
	// preallocate string to a size that should be big enough
	std::string fname(std::size(SAVENAME3) + 3, 0);
	for (;;) {
		auto needed = snprintf(
				fname.data(), fname.size(), SAVENAME3,
		num,
				Game::get_game_type() == BLACK_GATE     ? "bg"
				: Game::get_game_type() == SERPENT_ISLE ? "si"
														: "dev",
				type == SaveInfo::Type::AUTOSAVE    ? "_a"
				: type == SaveInfo::Type::QUICKSAVE ? "_q"
				: type == SaveInfo::Type::CRASHSAVE ? "_c"
													: ""
		
		
		);
		// snprintf failed
		if (needed < 0) {
			return "";
		}
		// buffer was too small so resize bigger
		if (size_t(needed) > fname.size()) {
			fname.resize(needed);
			continue;
		}
		// Had enough space so resize down to exact string size
		else {
			fname.resize(needed);
			return fname;
		}
	}
	return "";
}


void GameDat::save_gamedat(SaveInfo::Type type, const char* savename) {
	int         index = first_free[int(type)];
	int limit = INT_MAX;
	if (type == SaveInfo::Type::QUICKSAVE) {
		limit = Settings::get().disk.quicksave_count;
	}
	if (type == SaveInfo::Type::AUTOSAVE) {
		limit = Settings::get().disk.autosave_count;
	}

	if (limit == 0)
	{
		std::cerr << "Attempted to make " << (type == SaveInfo::Type::AUTOSAVE
				? "AutoSave"
				: "QuickSave") << " while disabled" << std::endl;
		return;    // Autosaves or quicksaves disabled
	} else if (index >= limit) {
		index = oldest[int(type)];
	}

	char name[50];

	// Create default savename wuth ISO date and Time if none given.
	if (!savename || !*savename) {
		const time_t t = time(nullptr);
		if (type == SaveInfo::Type::AUTOSAVE) {
			strcpy(name, "AutoSave ");
		} else if (type == SaveInfo::Type::QUICKSAVE) {
			strcpy(name, "QuickSave ");
		} else if (type == SaveInfo::Type::CRASHSAVE) {
			strcpy(name, "CrashSave ");
		} else {
			strcpy(name, "Save ");
		}
		size_t len = strlen(name);
		if (strftime(name + len, sizeof(name) - len, "%F %T", localtime(&t))
			== 0) {
			// null terminate and strip trailing space if strftime fails
			name[len - 1] = 0;
		}
		savename = name;
	}

	std::string fname
			= get_save_filename(index, type);    // Set up name.
	save_gamedat(fname.c_str(), savename);

	// Update save_info
}

void GameDat::ResortSaveInfos() {
	
	wait_for_saveinfo_read();

	std::lock_guard lock(save_info_mutex);

	if (save_infos.size()) {
		std::sort(save_infos.begin(), save_infos.end());
	}
}

/*
 *  Read in the saved game names.
 */
void GameDat::read_save_infos() {
	std::lock_guard lock(save_info_mutex);

	char mask[256];
	snprintf(
			mask, sizeof(mask), SAVENAME2,
			GAME_BG   ? "bg"
			: GAME_SI ? "si"
					  : "dev");
	auto save_mask = get_system_path(mask);

	FileList filenames;
	U7ListFiles(save_mask, filenames, true);

	// If save_mask is the same and we've already read the save infos do nothing
	if (std::string_view(save_mask) == this->save_mask && save_infos.size() == filenames.size()) {
		return;
	}
	this->save_mask = save_mask;
	save_infos.clear();

	// Sort filenames
	if (filenames.size()) {
		std::sort(filenames.begin(), filenames.end());
	}

	// Setup basic details
	save_infos.reserve(filenames.size());
	for (auto& filename : filenames) {
		save_infos.emplace_back(std::string(filename));
	}

	
	first_free.fill(-1);
	oldest.fill(0);

	std::array<int, SaveInfo::NUM_TYPES> last;
	last.fill(-1);

	std::array<SaveInfo*, SaveInfo::NUM_TYPES> oldestinfo;
	oldestinfo.fill(nullptr);

	// Read and cache all details
	for (auto& saveinfo : save_infos) {
		saveinfo.readable = get_saveinfo(
				saveinfo.filename(), saveinfo.savename, saveinfo.screenshot,
				saveinfo.details, saveinfo.party);

		// Handling of regular savegame with a savegame number
		if (saveinfo.type != SaveInfo::Type::UNKNOWN && saveinfo.num >= 0) {
			
			int itype = int(saveinfo.type);

			// Only try to figure oldest if saveinfo is readable and details were read
			// If no savegame is good oldest will default to 0
			if (saveinfo.readable) {

				if (!oldestinfo[itype] || saveinfo.details.CompareRealTime(oldestinfo[itype]->details) < 0)
				{
					oldest[itype] = saveinfo.num;
					oldestinfo[itype] = &saveinfo;

				}
			}

			// First free not yet found
			if (first_free[itype] == -1) {
				// If the last save was not 1 before this there is a gap wer can
				// use
				if (last[itype] + 1 != saveinfo.num) {
					first_free[itype] = last[itype] + 1;
				}

				last[itype] = saveinfo.num;
			}

			
		}
	}
	// If no gaps found set forst free of each type to last +1
	for (int type = 0; type < int(SaveInfo::Type::NUM_TYPES); ++type) {
		if (first_free[type] == -1) {
			first_free[type] = last[type] + 1;
		}
	}

	// Sort infos
	if (save_infos.size()) {
		std::sort(save_infos.begin(), save_infos.end());
	}
}

void GameDat::read_save_infos_async(bool force) {
	std::lock_guard lock(save_info_mutex);
	if (force) {
		clear_saveinfos();
	}
	if ((!saveinfo_future.valid()
		 || saveinfo_future.wait_for(std::chrono::seconds(0))
					== std::future_status::ready)
		&& save_infos.empty()) {
		saveinfo_future = std::async(std::launch::async, [this]() {
			try {
				read_save_infos();
			} catch (const std::exception& e) {
				std::cerr << "Error reading save infos: " << e.what()
						  << std::endl;
			}
		});
	}
}

void GameDat::wait_for_saveinfo_read() {
	if (saveinfo_future.valid()) {
		saveinfo_future.wait();
	}
}

const std::vector<GameDat::SaveInfo>* GameDat::GetSaveGameInfos(bool force) {
	// read save infos if needed
	read_save_infos_async(force);

	// wait for read to finish
	wait_for_saveinfo_read();

	return &save_infos;
}

void GameDat::write_saveinfo(bool screenshot) {
	// Update save count
	save_count++;
	const int party_size = partyman->get_count() + 1;

	{
		OFileDataSource out(
				GSAVEINFO);    // Open file; throws an exception - Don't care

		const time_t t        = time(nullptr);
		tm*          timeinfo = localtime(&t);

		// This order must match struct SaveGame_Details

		// Time that the game was saved
		out.write1(timeinfo->tm_min);
		out.write1(timeinfo->tm_hour);
		out.write1(timeinfo->tm_mday);
		out.write1(timeinfo->tm_mon + 1);
		out.write2(timeinfo->tm_year + 1900);

		auto clock = gwin->get_clock();

		// The Game Time that the save was done at
		out.write1(clock->get_minute());
		out.write1(clock->get_hour());
		out.write2(clock->get_day());

		out.write2(save_count);
		out.write1(party_size);

		out.write1(0);    // Unused

		out.write1(timeinfo->tm_sec);    // 15

		// Packing for the rest of the structure
		for (size_t j = offsetof(SaveGame_Details, reserved0);
			 j < sizeof(SaveGame_Details); j++) {
			out.write1(0);
		}

		for (auto npc : partyman->IterateWithMainActor) {
			std::string name(npc->get_npc_name());
			name.resize(sizeof(SaveGame_Party::name) - 1, '\0');
			out.write(name.c_str(), sizeof(SaveGame_Party::name));
			out.write2(npc->get_shapenum());

			out.write4(npc->get_property(Actor::exp));
			out.write4(npc->get_flags());
			out.write4(npc->get_flags2());

			out.write1(npc->get_property(Actor::food_level));
			out.write1(npc->get_property(Actor::strength));
			out.write1(npc->get_property(Actor::combat));
			out.write1(npc->get_property(Actor::dexterity));
			out.write1(npc->get_property(Actor::intelligence));
			out.write1(npc->get_property(Actor::magic));
			out.write1(npc->get_property(Actor::mana));
			out.write1(npc->get_property(Actor::training));

			out.write2(npc->get_property(Actor::health));
			out.write2(npc->get_shapefile());

			// Write zeros for the reseved yet to be used parts of the structure
			for (size_t j = offsetof(SaveGame_Party, reserved1);
				 j < sizeof(SaveGame_Party); j++) {
				out.write1(0);
			}
		}
	}

	if (screenshot) {
		std::cout << "Creating screenshot for savegame" << std::endl;
		// Save Shape
		std::unique_ptr<Shape_file> map = gwin->create_mini_screenshot();
		// Open file; throws an exception - Don't care
		OFileDataSource out(GSCRNSHOT);
		map->save(&out);
	} else if (U7exists(GSCRNSHOT)) {
		// Delete the old one if it exists
		U7remove(GSCRNSHOT);
	}

	{
		// Current Exult version
		// Open file; throws an exception - Don't care
		auto out_stream = U7open_out(GEXULTVER);
		if (out_stream) {
			getVersionInfo(*out_stream);
		}
	}

	// Exult version that started this game
	if (!U7exists(GNEWGAMEVER)) {
		OFileDataSource out(GNEWGAMEVER);
		const string    unkver("Unknown");
		out.write(unkver);
	}
}

void GameDat::read_saveinfo() {
	IFileDataSource ds(GSAVEINFO);
	if (ds.good()) {
		ds.skip(10);    // Skip 10 bytes.
		save_count = ds.read2();
	} else {
		save_count = 0;
	}
}

bool GameDat::read_saveinfo(
		IDataSource* in, SaveGame_Details& details,
		std::vector<SaveGame_Party>& party) {
	details = SaveGame_Details();
	party.clear();

	if (in->getAvail() < sizeof(SaveGame_Details)) {
		// Not enough data
		return false;
	}
	// This order must match struct SaveGame_Details
	// Time that the game was saved
	details.real_minute = in->read1();
	details.real_hour   = in->read1();
	details.real_day    = in->read1();
	details.real_month  = in->read1();
	details.real_year   = in->read2();

	// The Game Time that the save was done at
	details.game_minute = in->read1();
	details.game_hour   = in->read1();
	details.game_day    = in->read2();

	details.save_count = in->read2();
	auto party_size    = in->read1();

	details.unused = in->read1();    // Unused

	details.real_second = in->read1();    // 15

	// Packing for the rest of the structure
	in->skip(sizeof(SaveGame_Details) - offsetof(SaveGame_Details, reserved0));

	if (party_size > EXULT_PARTY_MAX || party_size == 0) {
		// Corrupted savegame
		return false;
	}
	if (in->getAvail() < party_size * sizeof(SaveGame_Party)) {
		// Not enough data
		return false;
	}

	party.reserve(party_size);
	while (party_size--) {
		auto& p = party.emplace_back();
		in->read(p.name, sizeof(p.name));
		// Make sure it's null terminated
		p.name[sizeof(p.name) - 1] = 0;
		p.shape                    = in->read2();

		p.exp    = in->read4();
		p.flags  = in->read4();
		p.flags2 = in->read4();

		p.food     = in->read1();
		p.str      = in->read1();
		p.combat   = in->read1();
		p.dext     = in->read1();
		p.intel    = in->read1();
		p.magic    = in->read1();
		p.mana     = in->read1();
		p.training = in->read1();

		p.health     = in->read2();
		p.shape_file = in->read2();

		// Skip all the reserved fields not yet used
		in->skip(sizeof(SaveGame_Party) - offsetof(SaveGame_Party, reserved1));
	}
	details.good = true;
	return true;
}

bool GameDat::get_saveinfo(
		const std::string& filename, std::string& name,
		std::unique_ptr<Shape_file>& map, SaveGame_Details& details,
		std::vector<SaveGame_Party>& party) {
	// First check for compressed save game
#ifdef HAVE_ZIP_SUPPORT
	if (get_saveinfo_zip(filename.c_str(), name, map, details, party)) {
		return true;
	}
#endif

	IFileDataSource in(filename);
	if (!in.good()) {
		throw file_read_exception(filename.c_str());
	}
	// in case of an error.
	// Always try to Read Name
	name.resize(0x50);
	in.read(name.data(), 0x4F);

	// Isn't a flex, can't actually read it
	if (!Flex::is_flex(&in)) {
		return false;
	}

	// Now get dir info
	in.seek(0x54);    // Get to where file count sits.
	const size_t numfiles = in.read4();
	in.seek(0x80);    // Get to file info.

	// Read pos., length of each file.
	struct file_info {
		size_t location;
		size_t length;
	};

	std::vector<file_info> finfo(numfiles);
	for (auto& [location, length] : finfo) {
		location = in.read4();    // The position, then the length.
		length   = in.read4();
	}

	// Always first two entires
	for (size_t i = 0; i < 2; i++) {    // Now read each file.
		// Get file length.
		auto& [location, length] = finfo[i];
		size_t len               = length;
		if (len <= 13) {
			continue;
		}
		len -= 13;
		in.seek(location);    // Get to it.
		char fname[50];     // Set up name.
		strcpy(fname, GAMEDAT);
		in.read(&fname[sizeof(GAMEDAT) - 1], 13);
		const size_t namelen = strlen(fname);
		// Watch for names ending in '.'.
		if (fname[namelen - 1] == '.') {
			fname[namelen - 1] = 0;
		}

		if (!strcmp(fname, GSCRNSHOT)) {
			auto ds = in.makeSource(len);
			map     = std::make_unique<Shape_file>(ds.get());
		} else if (!strcmp(fname, GSAVEINFO)) {
			read_saveinfo(&in, details, party);
		}
	}
	return true;
}

void GameDat::get_saveinfo(
		std::unique_ptr<Shape_file>& map, SaveGame_Details& details,
		std::vector<SaveGame_Party>& party, bool current) {
	{
		map.reset();
		details = SaveGame_Details();
		party.clear();
		if (current) {
			// Current screenshot
			map = gwin->create_mini_screenshot();

			// Current Details

			details.save_count = save_count;

			auto clock = gwin->get_clock();

			details.game_day
					= static_cast<short>(clock->get_total_hours() / 24);
			details.game_hour   = clock->get_hour();
			details.game_minute = clock->get_minute();

			const time_t t        = time(nullptr);
			tm*          timeinfo = localtime(&t);

			details.real_day    = timeinfo->tm_mday;
			details.real_hour   = timeinfo->tm_hour;
			details.real_minute = timeinfo->tm_min;
			details.real_month  = timeinfo->tm_mon + 1;
			details.real_year   = timeinfo->tm_year + 1900;
			details.real_second = timeinfo->tm_sec;
			details.good        = true;
			// Current Party
			party.clear();
			party.reserve(partyman->get_count() + 1);

			for (auto npc : partyman->IterateWithMainActor) {
				auto&       sgp_current = party.emplace_back();
				std::string namestr     = npc->get_npc_name();
				std::strncpy(
						sgp_current.name, namestr.c_str(),
						std::size(sgp_current.name) - 1);
				sgp_current.name[std::size(sgp_current.name) - 1] = 0;
				sgp_current.shape      = npc->get_shapenum();
				sgp_current.shape_file = npc->get_shapefile();

				sgp_current.dext     = npc->get_property(Actor::dexterity);
				sgp_current.str      = npc->get_property(Actor::strength);
				sgp_current.intel    = npc->get_property(Actor::intelligence);
				sgp_current.health   = npc->get_property(Actor::health);
				sgp_current.combat   = npc->get_property(Actor::combat);
				sgp_current.mana     = npc->get_property(Actor::mana);
				sgp_current.magic    = npc->get_property(Actor::magic);
				sgp_current.training = npc->get_property(Actor::training);
				sgp_current.exp      = npc->get_property(Actor::exp);
				sgp_current.food     = npc->get_property(Actor::food_level);
				sgp_current.flags    = npc->get_flags();
				sgp_current.flags2   = npc->get_flags2();
			}

		} else {
			{
				IFileDataSource ds(GSAVEINFO);
				if (ds.good()) {
					read_saveinfo(&ds, details, party);
				} else {
					details = SaveGame_Details();
					party.clear();
				}
			}

			{
				IFileDataSource ds(GSCRNSHOT);
				if (ds.good()) {
					map = std::make_unique<Shape_file>(&ds);
				} else {
					map.reset();
				}
			}
		}
	}
}

// Zip file support
#ifdef HAVE_ZIP_SUPPORT

static const char* remove_dir(const char* fname) {
	const char* base = strchr(fname, '/');    // Want the base name.
	if (!base) {
		base = strchr(fname, '\\');
	}
	if (base) {
		return base + 1;
	}

	return fname;
}

bool GameDat::get_saveinfo_zip(
		const char* fname, std::string& name, std::unique_ptr<Shape_file>& map,
		SaveGame_Details& details, std::vector<SaveGame_Party>& party) {
	// If a flex, so can't read it
	if (Flex::is_flex(fname)) {
		return false;
	}

	IFileDataSource ds(fname);
	unzFile         unzipfile = unzOpen(&ds);
	if (!unzipfile) {
		return false;
	}

	// Name comes from comment
	name.resize(0x50);

	if (unzGetGlobalComment(unzipfile, name.data(), 0x4F) <= 0) {
		name = "UNNAMED";
	}

	// Things we need
	unz_file_info file_info;

	// Get the screenshot first
	if (unzLocateFile(unzipfile, remove_dir(GSCRNSHOT), 2) == UNZ_OK) {
		unzGetCurrentFileInfo(
				unzipfile, &file_info, nullptr, 0, nullptr, 0, nullptr, 0);

		std::vector<char> buf(file_info.uncompressed_size);
		unzOpenCurrentFile(unzipfile);
		unzReadCurrentFile(unzipfile, buf.data(), file_info.uncompressed_size);
		if (unzCloseCurrentFile(unzipfile) == UNZ_OK) {
			IBufferDataView ds(buf.data(), file_info.uncompressed_size);
			map = std::make_unique<Shape_file>(&ds);
		}
	}

	// Now saveinfo
	if (unzLocateFile(unzipfile, remove_dir(GSAVEINFO), 2) == UNZ_OK) {
		unzGetCurrentFileInfo(
				unzipfile, &file_info, nullptr, 0, nullptr, 0, nullptr, 0);

		std::vector<char> buf(file_info.uncompressed_size);
		unzOpenCurrentFile(unzipfile);
		unzReadCurrentFile(unzipfile, buf.data(), file_info.uncompressed_size);
		if (unzCloseCurrentFile(unzipfile) == UNZ_OK) {
			IBufferDataView ds(buf.data(), buf.size());
			read_saveinfo(&ds, details, party);
		}
	}

	return true;
}

// Level 2 Compression
bool GameDat::Restore_level2(
		unzFile& unzipfile, const char* dirname, int dirlen) {
	std::vector<char>       filebuf;
	std::unique_ptr<char[]> dynamicname;
	char                    fixedname[50];    // Set up name.
	const size_t            oname2offset = sizeof(GAMEDAT) + dirlen - 1;
	char*                   oname2;
	if (oname2offset + 13 > std::size(fixedname)) {
		dynamicname = std::make_unique<char[]>(oname2offset + 13);
		oname2      = dynamicname.get();
	} else {
		oname2 = fixedname;
	}

	strncpy(oname2, dirname, oname2offset);
	char* oname = oname2;
	oname2 += oname2offset;

	if (unzOpenCurrentFile(unzipfile) != UNZ_OK) {
		std::cerr << "Couldn't open current file" << std::endl;
		return false;
	}

	while (!unzeof(unzipfile)) {
		// Read Filename
		oname2[12] = 0;
		if (unzReadCurrentFile(unzipfile, oname2, 12) != 12) {
			std::cerr << "Couldn't read for filename" << std::endl;
			return false;
		}

		// Check to see if was are at the end of the list
		if (*oname2 == 0) {
			break;
		}

		// Get file length.
		unsigned char size_buffer[4];
		if (unzReadCurrentFile(unzipfile, size_buffer, 4) != 4) {
			std::cerr << "Couldn't read for size" << std::endl;
			return false;
		}
		const unsigned char* ptr  = size_buffer;
		const int            size = little_endian::Read4(ptr);

		if (size) {
			// Watch for names ending in '.'.
			const int namelen = strlen(oname2);
			if (oname2[namelen - 1] == '.') {
				oname2[namelen - 1] = 0;
			}

			// Now read the file.
			filebuf.resize(size);

			if (unzReadCurrentFile(unzipfile, filebuf.data(), filebuf.size())
				!= size) {
				std::cerr << "Couldn't read for buf" << std::endl;
				return false;
			}

			// Then write it out.
			auto pOut = U7open_out(oname);
			if (!pOut) {
				std::cerr << "couldn't open " << oname << std::endl;
				return false;
			}
			auto& out = *pOut;
			out.write(filebuf.data(), filebuf.size());

			if (!out.good()) {
				std::cerr << "out was bad" << std::endl;
				return false;
			}
			gwin->cycle_load_palette();
		}
	}

	return unzCloseCurrentFile(unzipfile) == UNZ_OK;
}

/*
 *  Write out the gamedat directory from a saved game.
 *
 *  Output: Aborts if error.
 */

bool GameDat::restore_gamedat_zip(
		const char* fname    // Name of savegame file.
) {
	// If a flex, so can't read it
	try {
		if (Flex::is_flex(fname)) {
			return false;
		}
	} catch (const file_exception& /*f*/) {
		return false;    // Ignore if not found.
	}
	// Display red plasma during load...
	gwin->setup_load_palette();
	IFileDataSource ds(fname);
	unzFile         unzipfile = unzOpen(&ds);
	if (!unzipfile) {
		return false;
	}

	U7mkdir("<GAMEDAT>", 0755);    // Create dir. if not already there. Don't
	// use GAMEDAT define cause that's got a
	// trailing slash
	U7remove(USEDAT);
	U7remove(USEVARS);
	U7remove(U7NBUF_DAT);
	U7remove(NPC_DAT);
	U7remove(MONSNPCS);
	U7remove(FLAGINIT);
	U7remove(GWINDAT);
	U7remove(IDENTITY);
	U7remove(GSCHEDULE);
	U7remove("<STATIC>/flags.flg");
	U7remove(GSCRNSHOT);
	U7remove(GSAVEINFO);
	U7remove(GNEWGAMEVER);
	U7remove(GEXULTVER);
	U7remove(KEYRINGDAT);
	U7remove(NOTEBOOKXML);

	cout.flush();

	unz_global_info global;
	unzGetGlobalInfo(unzipfile, &global);

	// Now read each file.
	std::string oname = {};    // Set up name.
	oname             = GAMEDAT;

	char* oname2    = oname.data() + std::size(GAMEDAT) - 1;
	bool  level2zip = false;

	do {
		unz_file_info file_info;

		// For safer handling, better do it in two steps.
		unzGetCurrentFileInfo(
				unzipfile, &file_info, nullptr, 0, nullptr, 0, nullptr, 0);
		// Get the needed buffer size.
		const int filenamelen = file_info.size_filename;
		// make sure oname is of the right size
		oname.resize(filenamelen + std::size(GAMEDAT) - 1);
		oname2 = oname.data() + std::size(GAMEDAT) - 1;

		unzGetCurrentFileInfo(
				unzipfile, nullptr, oname2, filenamelen, nullptr, 0, nullptr,
				0);

		// Get file length.
		const int len = file_info.uncompressed_size;
		if (len <= 0) {
			continue;
		}

		// Level 2 compression handling
		if (level2zip) {
			// Files for map # > 0; create dir first.
			U7mkdir(oname.c_str(), 0755);
			// Put a final marker in the dir name.
			if (!Restore_level2(unzipfile, oname.c_str(), filenamelen)) {
				gwin->abort("Error reading level2 from zip '%s'.", fname);
			}
			continue;
		} else if (!std::strcmp("GAMEDAT", oname2)) {
			// Put a final marker in the dir name.
			if (!Restore_level2(unzipfile, oname.c_str(), 0)) {
				gwin->abort("Error reading level2 from zip '%s'.", fname);
			}
			// Flag that this is a level 2 save.
			level2zip = true;
			continue;
		}

		// Get rid of trailing nulls at the end
		while (!oname.back()) {
			oname.pop_back();
		}
		// Watch for names ending in '.'.
		if (oname.back() == '.') {
			oname.pop_back();
		}
		// Watch out for multimap games.
		for (char& c : oname) {
			// May need to create a mapxx directory here
			if (c == '/') {
				c = 0;
				U7mkdir(oname.data(), 0755);
				c = '/';
			}
		}

		// Open the file in the zip
		if (unzOpenCurrentFile(unzipfile) != UNZ_OK) {
			gwin->abort("Error opening current from zipfile '%s'.", fname);
		}

		// Now read the file.
		std::vector<char> buf(len);
		if (unzReadCurrentFile(unzipfile, buf.data(), buf.size()) != len) {
			gwin->abort("Error reading current from zip '%s'.", fname);
		}

		// now write it out.
		auto pOut = U7open_out(oname);
		if (!pOut) {
			gwin->abort("Error opening '%s'.", oname.c_str());
		}
		auto& out = *pOut;
		out.write(buf.data(), buf.size());
		if (!out.good()) {
			gwin->abort("Error writing to '%s'.", oname.c_str());
		}

		// Close the file in the zip
		if (unzCloseCurrentFile(unzipfile) != UNZ_OK) {
			gwin->abort("Error closing current in zip '%s'.", fname);
		}

		gwin->cycle_load_palette();
	} while (unzGoToNextFile(unzipfile) == UNZ_OK);

	cout.flush();

	gwin->load_finished();

	return true;
}

// Level 1 Compression
bool GameDat::Save_level1(zipFile& zipfile, const char* fname, bool required) {
	IFileDataSource ds(fname);
	if (!ds.good()) {
		if (Game::is_editing() || !required) {
			return false;    // Newly developed game. or file is not required
		}
		throw file_read_exception(fname);
	}

	const size_t size = ds.getSize();
	const auto   buf  = ds.readN(size);

	zipOpenNewFileInZip(
			zipfile, remove_dir(fname), nullptr, nullptr, 0, nullptr, 0,
			nullptr, Z_DEFLATED, Z_BEST_COMPRESSION);

	zipWriteInFileInZip(zipfile, buf.get(), size);

	return zipCloseFileInZip(zipfile) == ZIP_OK;
}

// Level 2 Compression
bool GameDat::Begin_level2(zipFile& zipfile, int mapnum) {
	char oname[8];    // Set up name.
	if (mapnum == 0) {
		strcpy(oname, "GAMEDAT");
		oname[7] = 0;
	} else {
		strcpy(oname, "map");
		constexpr static const char hexLUT[] = "0123456789abcdef";
		oname[3]                             = hexLUT[mapnum / 16];
		oname[4]                             = hexLUT[mapnum % 16];
		oname[5]                             = 0;
	}
	return zipOpenNewFileInZip(
				   zipfile, oname, nullptr, nullptr, 0, nullptr, 0, nullptr,
				   Z_DEFLATED, Z_BEST_COMPRESSION)
		   == ZIP_OK;
}

bool GameDat::Save_level2(zipFile& zipfile, const char* fname) {
	IFileDataSource ds(fname);
	if (!ds.good()) {
		if (Game::is_editing()) {
			return false;    // Newly developed game.
		}
		throw file_read_exception(fname);
	}

	const size_t      size = ds.getSize();
	std::vector<char> buf(std::max<size_t>(13, size), 0);

	// Filename first
	const char* fname2 = strrchr(fname, '/');
	if (!fname2) {
		fname2 = strchr(fname, '\\');
	}
	if (fname2) {
		fname2++;
	} else {
		fname2 = fname;
	}
	strncpy(buf.data(), fname2, 13);
	int err = zipWriteInFileInZip(zipfile, buf.data(), 12);

	// Size of the file
	if (err == ZIP_OK) {
		// Must be platform independent
		auto* ptr = buf.data();
		little_endian::Write4(ptr, size);
		err = zipWriteInFileInZip(zipfile, buf.data(), 4);
	}

	// Now the actual file
	if (err == ZIP_OK) {
		ds.read(buf.data(), size);
		err = zipWriteInFileInZip(zipfile, buf.data(), size);
	}

	return err == ZIP_OK;
}

bool GameDat::End_level2(zipFile& zipfile) {
	uint32 zeros = 0;

	// Write a terminator (12 zeros)
	int err = zipWriteInFileInZip(zipfile, &zeros, 4);
	if (err == ZIP_OK) {
		err = zipWriteInFileInZip(zipfile, &zeros, 4);
	}
	if (err == ZIP_OK) {
		err = zipWriteInFileInZip(zipfile, &zeros, 4);
	}

	return zipCloseFileInZip(zipfile) == ZIP_OK && err == ZIP_OK;
}

bool GameDat::save_gamedat_zip(
		const char* fname,      // File to create.
		const char* savename    // User's savegame name.
) {
	char iname[128];
	// If no compression return
	if (Settings::get().disk.save_compression_level < 1) {
		return false;
	}

	// setup correct file list
	tcb::span<const char* const> savefiles;
	if (Game::get_game_type() == BLACK_GATE) {
		savefiles = bgsavefiles;
	} else {
		savefiles = sisavefiles;
	}

	// Name
	{
		auto out = U7open_out(fname);
		if (out) {
			std::string title(savename);
			title.resize(0x50, '\0');
			out->write(title.data(), title.size());
		}
	}

	const auto  filestr = get_system_path(fname);
	zipFile           zipfile = zipOpen(filestr.c_str(), 1);

	// We need to explicitly save these as they are no longer included in
	// savefiles span and they should always be stored first and as level 1
	// screnshot and saveinfo are not required files
	Save_level1(zipfile, GSCRNSHOT,false);
	Save_level1(zipfile, GSAVEINFO,false);
	Save_level1(zipfile, IDENTITY,true);

	// Level 1 Compression
	if (Settings::get().disk.save_compression_level != 2) {
		for (const auto* savefile : savefiles) {
			if (!Save_level1(zipfile, savefile)) {
				throw file_write_exception(fname);
			}
		}

		// Now the Ireg's.

		for (const auto* map : gwin->get_maps()) {
			if (!map) {
				continue;
			}
			for (int schunk = 0; schunk < 12 * 12; schunk++) {
				// Check to see if the ireg exists before trying to
				// save it; prevents crash when creating new maps
				// for existing games
				if (U7exists(
							map->get_schunk_file_name(U7IREG, schunk, iname))) {
					if (!Save_level1(zipfile, iname)) {
						throw file_write_exception(fname);
					}
				}
			}
		}
	}
	// Level 2 Compression
	else {
		// Start the GAMEDAT file.
		if (!Begin_level2(zipfile, 0)) {
			throw file_write_exception(fname);
		}

		for (const char* savefilename : savefiles) {
			if (!Save_level2(zipfile, savefilename)) {
				throw file_write_exception(fname);
			}
		}

		// Now the Ireg's.
		for (const auto* map : gwin->get_maps()) {
			if (!map) {
				continue;
			}
			if (map->get_num() != 0) {
				// Finish the open file (GAMEDAT or mapXX).
				if (!End_level2(zipfile)
					|| !Begin_level2(zipfile, map->get_num())) {
					throw file_write_exception(fname);
				}
			}
			for (int schunk = 0; schunk < 12 * 12; schunk++) {
				// Check to see if the ireg exists before trying to
				// save it; prevents crash when creating new maps
				// for existing games
				if (U7exists(
							map->get_schunk_file_name(U7IREG, schunk, iname))) {
					if (!Save_level2(zipfile, iname)) {
						throw file_write_exception(fname);
					}
				}
			}
		}

		if (!End_level2(zipfile)) {
			throw file_write_exception(fname);
		}
	}
	
	if (zipfile.close(savename) != ZIP_OK) {
		throw file_write_exception(fname);
	}

	return true;
}

#endif

void GameDat::MakeEmergencySave(const char* savename) {
	// Using mostly std::filesystem here insteaf of U7 functions to avoid
	// repeated looking up paths

	// Set default savegame name
	if (!savename) {
		savename = "Crash Save";
	}
	std::cerr << "Trying to create an emergency save named \"" << savename
			  << "\"" << std::endl;

	// Get the gamedat path and the crashtemp path
	std::string gamedatpath(get_system_path("<GAMEDAT>"));
	std::string crashtemppath(get_system_path("<GAMEDAT>.crashtemp"));

	// change <GAMEDAT> to point to crashtemp
	add_system_path("<GAMEDAT>", crashtemppath);

	// Remove old crashtemp if it exists
	std::filesystem::remove_all(crashtemppath);

	// create dorectory for crashtemp
	std::filesystem::create_directory(crashtemppath);

	// Copy the files from gamedat to crashtemp by iterating the directory
	// manually so we can continue on failure
	// std::filesystem::copy_all crashes on the exultserver file
	for (const auto& entry : std::filesystem::directory_iterator(gamedatpath)) {
		auto newpath = crashtemppath + "/" + entry.path().filename().string();

		// Copy files ignoring errors
		std::error_code ec;
		std::filesystem::copy_file(entry.path(), newpath, ec);
	}

	// Write out current gamestate to gamedat
	std::cerr << " attempting to save current gamestate to gamedat"
			  << std::endl;
	gwin->write(true);

	// save it as the save
	std::cerr << " attempting to save gamedat as \"" << savename << "\""
			  << std::endl;
	save_gamedat(SaveInfo::Type::CRASHSAVE, savename);

	// Remove crashtemp
	std::filesystem::remove_all(crashtemppath);

	// Put <GAMEDAT> back to how it was
	add_system_path("<GAMEDAT>", gamedatpath);
}

GameDat::GameDat() {}

void GameDat::Quicksave() {
	gwin->write();
	save_gamedat(SaveInfo::Type::QUICKSAVE, nullptr);
}

void GameDat::Savegame(const char* fname, const char* savename) {
	gwin->write();
	save_gamedat(fname, savename);
}

void GameDat::Savegame(const char* savename) {
	gwin->write();
	save_gamedat(SaveInfo::Type::REGULAR, savename);
}

void GameDat::Extractgame(const char* fname, bool doread) {
	// Only restore if a filename is given
	if (fname && *fname) {
		restore_gamedat(fname);
	}

	if (doread) {
		gwin->read();
	}
}


// Move Costructor from a std::string filename
GameDat::SaveInfo::SaveInfo(std::string&& filename)
		: filename_(std::move(filename)) {
	// Filename is likely a path so find the last directory separator
	size_t filename_start = filename_.find_last_of("/\\");

	// No diretory separators so actual filename starts at 0
	if (filename_start == std::string::npos) {
		filename_start = 0;
	} else
		{
		// Move to the character after the last directory separator
		filename_start++;
	}
	// Find where the savenume number starts
	size_t number_start = filename_.find_first_of("0123456789", filename_start);

	if (number_start == std::string::npos || number_start != filename_start + 5
		|| Pentagram::strncasecmp(filename_.c_str() + filename_.size() - 4, ".sav",4)) {
		// the savegame filename is not in the expected format
		// this should never happen as filename glob should only list
		// filenames in mostly the correct format
		// EXULT*.sav
		// Don't attempt to parse the savegame number
		num  = -1;
		type = Type::UNKNOWN;
		return;
	} 

	else if (filename_[filename_.size()-6] == '_') {
		// quicksaves have 'q' at the end
		if (std::tolower(filename_[filename_.size() - 5]) == 'q') {
			type = Type::QUICKSAVE;
			// autosaves have 'a' at the end
		} else if (std::tolower(filename_[filename_.size() - 5]) == 'a') {
			type = Type::AUTOSAVE;
			// crashsaves have 'a' at the end
		} else if (std::tolower(filename_[filename_.size() - 5]) == 'c') {
			type = Type::CRASHSAVE;
		}
		else {
			// Filename has unknown type tag
			num  = -1;
			type = Type::UNKNOWN;
			return;
		}
	}
	else {
	// No special type tag so regular savegame
		type = Type::REGULAR;
	} 

	num = strtol(filename_.c_str() + number_start, nullptr, 10);
}

int GameDat::SaveInfo::compare(const SaveInfo& other) const noexcept {
	if (type != other.type && Settings::get().disk.savegame_group_by_type) {
		return int(other.type) - int(type);
	}

	if (Settings::get().disk.savegame_sort_by
		== Settings::Disk::SORTBY_NAME) {
		int namecomp = Pentagram::strcasecmp(
				this->savename.c_str(), other.savename.c_str());

		if (namecomp != 0) {
			return namecomp;
		}
	}

	if (details && other.details) {
		// Sort by time


		if (Settings::get().disk.savegame_sort_by == Settings::Disk::SORTBY_GAMETIME)
		{
			int datecomp = details.CompareGameTime(other.details);
			if (datecomp != 0) {
				return datecomp;
			}

		} 

			int datecomp
				= details.CompareRealTime(other.details);
		if (datecomp != 0) {
			return datecomp;
		}


	} else if (details) {    // If the other doesn't have details we are
							 // first
		return -1;
	} else if (other.details) {    // If we don't have details we are last
		return 1;
	}

	// Lastly just sort by filename
	return filename_.compare(other.filename_);
}

int GameDat::SaveGame_Details::CompareRealTime(
		const SaveGame_Details& other) const noexcept {
	if (real_year != other.real_year) {
		return other.real_year-real_year ;
	}

	if (real_month != other.real_month) {
		return other.real_month-real_month;
	}

	if (real_day != other.real_day) {
		return other.real_day - real_day;
	}


	if (real_hour != other.real_hour) {
		return other.real_hour - real_hour;
	}

	if (real_minute != other.real_minute) {
		return other.real_minute - real_minute;
	}

	if (real_second != other.real_second) {
		return other.real_second - real_second;
	}
	return 0;
}

int GameDat::SaveGame_Details::CompareGameTime(
		const SaveGame_Details& other) const noexcept {
	if (game_day != other.game_day) {
		return other.game_day - game_day;
	}

	if (game_hour != other.game_hour) {
		return other.game_hour - game_hour;
	}
	if (game_minute != other.game_minute) {
		return other.game_minute - game_minute;
	}

	return 0;
}
