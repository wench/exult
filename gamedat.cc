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
#include "istring.h"
#include "listfiles.h"
#include "mouse.h"
#include "party.h"
#include "span.h"
#include "ucmachine.h"
#include "utils.h"
#include "version.h"

#include <chrono>
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
	if (saveinfo_future.valid()) {
		save_info_cancel = true;
		saveinfo_future.wait();
	}
	Newfile_gump::SaveGameDetailsChanging();
	std::lock_guard lock(save_info_mutex);
	save_infos.clear();
	saveinfo_future = std::shared_future<void>();
}

/*
 *  Write files from flex assuming first Flex_writer::MAX_FILENAME_SIZE characters of
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
		if (len <= Flex_writer::MAX_FILENAME_SIZE) {
			continue;
		}
		len -= Flex_writer::MAX_FILENAME_SIZE;
		in.seek(location);    // Get to it.
		char fname[50];       // Set up name.
		strcpy(fname, basepath);
		in.read(&fname[baselen], Flex_writer::MAX_FILENAME_SIZE);
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
 *  List of 'gamedat' files to save (in addition to 'iregxx'):
 */
constexpr static const std::array bgsavefiles{
		GEXULTVER, GNEWGAMEVER, GPALETTE, NPC_DAT, MONSNPCS,  USEVARS,
		USEDAT,    FLAGINIT,    GWINDAT, GSCHEDULE, NOTEBOOKXML};

constexpr static const std::array sisavefiles{
		GEXULTVER, GNEWGAMEVER, GPALETTE, NPC_DAT, MONSNPCS,   USEVARS,    USEDAT,
		FLAGINIT,  GWINDAT,     GSCHEDULE, KEYRINGDAT, NOTEBOOKXML};

void GameDat::SaveToFlex(
		Flex_writer&      flex,
		std::pmr::string& fname    // Name of file to save.
) {
	if (gamedat_in_memory.active) {
		// Save from memory

		auto it = gamedat_in_memory.files->find(fname);

		if (it != gamedat_in_memory.files->end()) {
			flex.write_file(fname, it->second.data(), it->second.size());
			return;
		}
	}
	auto source = IFileDataSource(U7open_in(fname));
	if (!source.good()) {
		if (Game::is_editing()) {
			return;    // Newly developed game.
		}
		throw file_read_exception(fname);
	}
	size_t size = source.getSize();
	if (size <= MAX_SAVE_BUFFER) {
		source.read(gamedat_in_memory.save_buffer, size);
		flex.write_file(fname, gamedat_in_memory.save_buffer, size);
		return;
	} else {
		// File too big for buffer, let flex read it
		flex.write_file(fname, source);
	}
}

inline void GameDat::save_chunks_to_flex(Game_map* map, Flex_writer& flex) {
	for (int schunk = 0; schunk < 12 * 12; schunk++) {
		char iname[128];		
		// Check to see if the ireg exists before trying to
		// save it; prevents crash when creating new maps
		// for existing games
		std::pmr::string iname_pmr(
				map->get_schunk_file_name(U7IREG, schunk, iname));
		if (fileExists(iname_pmr)) {
			SaveToFlex(flex, iname_pmr);
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
		const std::pmr::string& fname,      // File to create.
		const char*             savename    // User's savegame name.
) {
	// Lock gamedat in memory for duration of save
	std::lock_guard lock(gamedat_in_memory.mutex);


#ifdef HAVE_ZIP_SUPPORT
	// Try to save as a zip file
	if (Settings::get().disk.save_compression_level > 0
		&& save_gamedat_zip(fname, savename)) {
		gamedat_in_memory.disable();
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
	OFileDataSource out(U7open_out(fname, false));
	auto flex_buffer = std::pmr::polymorphic_allocator<uint8_t>().allocate(
            Flex_writer::BufferSize(count));
	Flex_writer flex(out, savename, count, Flex_header::orig, flex_buffer);
	
	// We need to explicitly save these as they are no longer included in
	// savefiles span and must be first
	// Screenshot and Saveinfo are optional
	// Identity is required
	std::pmr::string pmrname(GSCRNSHOT);
	if (fileExists(pmrname))
	{
		SaveToFlex(flex, pmrname);
	}
	pmrname = GSAVEINFO;
	if (fileExists(pmrname))
	{
		SaveToFlex(flex, pmrname);
	}
	pmrname = IDENTITY;
	SaveToFlex(flex, pmrname);

for (const auto* savefile : savefiles) {
		std::pmr::string savefile_pmr(savefile);
		SaveToFlex(flex, savefile_pmr);
	}
	// Now the Ireg's.
	auto map_buffer = std::pmr::polymorphic_allocator<uint8_t>().allocate(
			Flex_writer::BufferSize(12 * 12));

	std::pmr::vector<uint8_t> outbuf;
	outbuf.reserve(
			1024
			* 512);    // preallocate 512KB for map flex files.144 superchunks
					   // can be expected to be in this order of size.
	for (auto* map : gwin->get_maps()) {
		if (!map) {
			continue;
		}
		if (!map->get_num()) {
			// Map 0 is a special case.
			save_chunks_to_flex(map, flex);
		} else {
			// Multimap directory entries. Each map is stored in their
			// own flex file contained inside the general gamedat flex.
			{
				char dname[128];
				map->get_mapped_name(GAMEDAT, dname);
				Flex_writer mapflex = flex.start_nested_flex(dname, 12 * 12, Flex_header::orig, map_buffer);
				// Save chunks to nested flex
				save_chunks_to_flex(map, mapflex);
			}
		}
	}
	// Done with gamedat in memory so disable it before reading save infos
	gamedat_in_memory.disable();

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
		if (itype > 0 && itype<SaveInfo::NUM_TYPES &&
			first_free[itype]> it->num) {
			first_free[itype] = it->num;
		}

		Newfile_gump::SaveGameDetailsChanging();
		save_infos.erase(it);
	} else {
		// This shouldn't happen unless someone called this function 
		// with a filename that wasn't a save game
		// But just incase reload all the save infos
		read_save_infos_async(true);
	}
}

std::pmr::string GameDat::
		get_save_filename(int num, SaveInfo::Type type) {
	// preallocate string to a size that should be big enough
	std::pmr::string fname(
			std::size(SAVENAME3) + 4, 0);
	for (;;) {
		auto needed = snprintf(
				fname.data(), fname.size(), SAVENAME3,
		num,
				Game::get_game_type() == BLACK_GATE     ? "bg"
				: Game::get_game_type() == SERPENT_ISLE ? "si"
														: "dev",
				type == SaveInfo::Type::AUTOSAVE    ? "_a"
				: type == SaveInfo::Type::FLAG_AUTOSAVE    ? "_f"
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

void GameDat::save_gamedat(
		SaveInfo::Type type,
		const char*    savename    // User's savegame name.
) {

		// Lock gamedat in memory during save
	std::unique_lock memlock(gamedat_in_memory.get_mutex());

	// Lock save_info during save as it will be updated after save
	std::lock_guard si_lock(save_info_mutex);

	if (type <= SaveInfo::Type::UNKNOWN || type >= SaveInfo::Type::NUM_TYPES) {
		throw exult_exception("Invalid save type");
	}
	int index = first_free[int(type)];
	int limit = INT_MAX;
	if (type == SaveInfo::Type::QUICKSAVE) {
		limit = Settings::get().disk.quicksave_count;
	}
	if (type == SaveInfo::Type::FLAG_AUTOSAVE) {
		limit = Settings::get().disk.flagautosave_count;
	}
	if (type == SaveInfo::Type::AUTOSAVE) {
		limit = Settings::get().disk.autosave_count;
	}
	// This should not happen
	if (index == -1) {
		index = limit;
	}

	if (limit == 0) {
		std::cerr << "Attempted to make "
				  << (type == SaveInfo::Type::QUICKSAVE ? "QuickSave" 
													   : "AutoSave")
				  << " while disabled" << std::endl;
		return;    // Autosaves or quicksaves disabled
	} else if (index >= limit) {
		index = oldest[int(type)];
	}

	char name[50];

	// Create default savename wuth ISO date and Time if none given.
	if (!savename || !*savename) {
		const time_t t = time(nullptr);
		if (type == SaveInfo::Type::AUTOSAVE ||type == SaveInfo::Type::FLAG_AUTOSAVE) {
			strcpy(name, Strings::AutoSave());
		} else if (type == SaveInfo::Type::QUICKSAVE) {
			strcpy(name, Strings::QuickSave());
		} else if (type == SaveInfo::Type::CRASHSAVE) {
			strcpy(name, Strings::CrashSave());
		} else {
			strcpy(name, Strings::Save());
		}
		size_t len = strlen(name);
		if (strftime(name + len, sizeof(name) - len, "%F %T", localtime(&t))
			== 0) {
			// null terminate and strip trailing space if strftime fails
			name[len - 1] = 0;
		}
		savename = name;
	}

	std::pmr::string fname = get_save_filename(
			index, type);    // Set up name.
	save_gamedat(fname, savename);


}

std::shared_future<void>& GameDat::save_gamedat_async(
		std::variant<SaveInfo::Type, const char*> type_or_filename,
		const char*                                 savename) {
	// Lock gamedat in memory during save
	std::unique_lock gimlock(gamedat_in_memory.get_mutex());

	// Make a copy of savename in gamedat memory pool in case it is on the stack
	// and goes out of scope but only if it is not null and not already from
	// gamedat memory pool
	if (savename && !gamedat_in_memory.pool.is_from_this_pool(savename)) {
		savename = gamedat_in_memory.pool.strdup(savename);
	}

	const char**p_filename = std::get_if<const char*>(&type_or_filename);
	std::pmr::string* filename 
			= p_filename && *p_filename ?gamedat_in_memory.pool.new_object<std::pmr::string>(
                              *p_filename):nullptr;
	SaveInfo::Type type = !p_filename? std::get<SaveInfo::Type>(type_or_filename)
							 : SaveInfo::Type::UNKNOWN;

	// Future of async save in progress or last save
	static std::shared_future<void> save_future = {};

	// Wait for last save to finish
	if (save_future.valid()) {
		save_future.wait();
	}
		gimlock.unlock();
		save_future = std::async(std::launch::async, [this, type, savename,filename]() {
		try {
			std::cout << "Starting async gamedat save..." << std::endl;
			if (type != SaveInfo::Type::UNKNOWN) {
				save_gamedat(type, savename);
			} else if (filename) {
				save_gamedat(*filename,savename);
			}
			std::cout << "Finished async gamedat save." << std::endl;
		} catch (const std::exception& e) {
			std::cerr << "Error saving gamedat async: " << e.what()
					  << std::endl;
		}
	});


		return save_future;

	}

void GameDat::ResortSaveInfos() {
	wait_for_saveinfo_read();

	std::lock_guard lock(save_info_mutex);

	if (save_infos.size()) {
		saveinfo_future = std::async(std::launch::async, [this]() {
			std::lock_guard lock2(save_info_mutex);
			try {
				std::sort(save_infos.begin(), save_infos.end());
			} catch (const std::exception& e) {
				std::cerr << "Error resorting save infos: " << e.what()
						  << std::endl;
			}
		});
	}
}

/*
 *  Read in the saved game names.
 */
void GameDat::read_save_infos() {
	std::lock_guard lock(save_info_mutex);

	// wait for the memory lock to be released 
	std::unique_lock memlock(gamedat_in_memory.get_mutex());

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
	if (save_info_cancel || (std::string_view(save_mask) == this->save_mask && save_infos.size() == filenames.size())) {
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
		if (save_info_cancel) {
			return;
		}

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
		if (save_info_cancel) {
			return;
		}
		saveinfo.readable = get_saveinfo(
				saveinfo.filename(), saveinfo.savename, saveinfo.screenshot,
				saveinfo.details, saveinfo.party,saveinfo.palette);

		// Handling of regular savegame with a savegame number
		if (saveinfo.type != SaveInfo::Type::UNKNOWN && saveinfo.num >= 0) {
			int itype = int(saveinfo.type);

			// Only try to figure oldest if saveinfo is readable and details
			// were read If no savegame is good oldest will default to 0
			if (saveinfo.readable) {
				if (!oldestinfo[itype]
					|| saveinfo.details.CompareRealTime(
							   oldestinfo[itype]->details)
							   > 0) {
					oldest[itype]     = saveinfo.num;
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
	// If no gaps found set first free of each type to last +1
	for (int type = 0; type < SaveInfo::NUM_TYPES; ++type) {
		if (first_free[type] == -1) {
			first_free[type] = last[type] + 1;
		}
	}

	// Sort infos
	if (!save_info_cancel && save_infos.size()) {
		std::sort(save_infos.begin(), save_infos.end());
	}

}

void GameDat::read_save_infos_async(bool force) {
	if (force) {
		clear_saveinfos();
	}
	std::lock_guard lock(save_info_mutex);
	if ((!saveinfo_future.valid()
		 || saveinfo_future.wait_for(std::chrono::seconds(0))
					== std::future_status::ready)
		&& save_infos.empty()) {
			save_info_cancel = false;
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

bool GameDat::are_save_infos_loaded() {
	return saveinfo_future.valid();
}

void GameDat::write_saveinfo(bool screenshot) {
	// Update save count
	save_count++;
	const int party_size = partyman->get_count() + 1;

	{
		auto out = Open_ODataSource(GSAVEINFO);

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

		out.write1(cheat.has_cheated()); 

		out.write1(timeinfo->tm_sec);    

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

	// Delete any existing screenshot
	 if (U7exists(GSCRNSHOT)) {
		// Delete the old one if it exists
		U7remove(GSCRNSHOT);
	}
	if (screenshot) {
		std::cout << "Creating screenshot for savegame" << std::endl;
		// Save Shape
		try {
			std::unique_ptr<Shape_file> map = gwin->create_mini_screenshot();
			ODataSourceFileOrVector     out = Open_ODataSource(GSCRNSHOT);

		map->save(&out);
		} catch (const std::exception& e) {
			std::cerr << "Error creating screenshot: " << e.what() << std::endl;
			// delete partial screenshot if there was an error
			U7remove(GSCRNSHOT);
		}

	} 

	{
		// Current Exult version
		{
			auto stream = Open_ostream(GEXULTVER);

			if (!stream || !stream->good()) {
				return;
			}
			getVersionInfo(*stream);
			stream->flush();
		}
	}

	// Exult version that started this game is missing
	if (!fileExists(GNEWGAMEVER)) {
		
		 Open_ODataSource(GNEWGAMEVER).write("Unknown");
	}
}

void GameDat::read_saveinfo(bool newgame) {
	// if newgame, reset save count just in case the initgame file contains saveinfo.dat 
	// U6 mod has one and it should be ignored so save count starts at 0 for players
	save_count = 0;
	cheat.set_cheated(false);
	if (newgame)
	{
		return;
		// Remove the saveinfo if exists to prevent reading it in the future
		if (U7exists(GSAVEINFO)) {
			U7remove(GSAVEINFO);
		}
	}
	IFileDataSource ds(GSAVEINFO);
	if (ds.good()) {
		ds.skip(10);    // Skip 10 bytes.
		save_count = ds.read2();
		ds.skip(1);
		cheat.set_cheated(ds.read1() != 0);

	}
	if (!ds.good()) {
		save_count = 0;
		cheat.set_cheated(false);
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

	details.cheated = in->read1()!=0;

	details.real_second = in->read1(); 

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

bool GameDat::fileExists(const std::pmr::string& fname) {
	return (gamedat_in_memory.active
			&& gamedat_in_memory.files->find(fname)
					   != gamedat_in_memory.files->end())
		   || U7exists(fname);
}

bool GameDat::get_saveinfo(
		const std::string& filename, std::string& name,
		std::unique_ptr<Shape_file>& map, SaveGame_Details& details,
		std::vector<SaveGame_Party>& party, std::unique_ptr<Palette>& palette) {
	// Clear out old info
	details = SaveGame_Details();
	party.clear();
	map.reset();
	name.clear();
	palette.reset();

	// First check for compressed save game
#ifdef HAVE_ZIP_SUPPORT
	if (get_saveinfo_zip(filename.c_str(), name, map, details, party, palette)) {
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
		if (len <= Flex_writer::MAX_FILENAME_SIZE) {
			continue;
		}
		len -= Flex_writer::MAX_FILENAME_SIZE;
		in.seek(location);    // Get to it.
		char fname[sizeof(GAMEDAT)-1+Flex_writer::MAX_FILENAME_SIZE];     // Set up name.
		strcpy(fname, GAMEDAT);
		in.read(&fname[sizeof(GAMEDAT) - 1], Flex_writer::MAX_FILENAME_SIZE);
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

		else if (!strcmp(fname, GPALETTE)) {
			auto ds = in.makeSource(len);
			palette     = std::make_unique<Palette>();
			palette->Deserialize(*ds);
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
			details.cheated    = cheat.has_cheated();

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
		SaveGame_Details& details, std::vector<SaveGame_Party>& party,
		std::unique_ptr<Palette>& palette) {
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
	// Palette
	if (unzLocateFile(unzipfile, remove_dir(GPALETTE), 2) == UNZ_OK) {
		unzGetCurrentFileInfo(
				unzipfile, &file_info, nullptr, 0, nullptr, 0, nullptr, 0);

		std::vector<char> buf(file_info.uncompressed_size);
		unzOpenCurrentFile(unzipfile);
		unzReadCurrentFile(unzipfile, buf.data(), file_info.uncompressed_size);
		if (unzCloseCurrentFile(unzipfile) == UNZ_OK) {
			IBufferDataView ds(buf.data(), file_info.uncompressed_size);
			palette= std::make_unique<Palette>();
			palette->Deserialize(ds);

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
			return read_saveinfo(&ds, details, party);
		}
	}

	return false;
}

// Level 2 Compression
bool GameDat::Restore_level2(
		unzFile& unzipfile, const char* dirname, int dirlen) {
	std::vector<char>       filebuf;
	std::unique_ptr<char[]> dynamicname;
	char                    fixedname[50];    // Set up name.
	const size_t            oname2offset = sizeof(GAMEDAT) + dirlen - 1;
	char*                   oname2;
	if (oname2offset + Flex_writer::MAX_FILENAME_SIZE > std::size(fixedname)) {
		dynamicname = std::make_unique<char[]>(
				oname2offset + Flex_writer::MAX_FILENAME_SIZE);
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
	U7remove(GPALETTE);

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
bool GameDat::Save_level1(
		zipFile& zipfile, const std::pmr::string& fname, bool required) {
	IFileDataSource ds;
	auto*           vec = get_memory_file(fname, false);

	if (!vec) {
		try {
			ds = IFileDataSource(U7open_in(fname));
		}
		catch (file_open_exception &)
		{
			if (Game::is_editing() || !required) {
				return true;    // Newly developed game. or file is not
								// required. Missing file is ok.
			}
			throw;
		}
		if (!ds.good()) {
			if (Game::is_editing() || !required) {
				return true;    // Newly developed game. or file is not
								// required. Missing file is ok.
			}
			throw file_read_exception(fname);
		}
		//std::cout << "Saving file " << fname << " size " << ds.getSize()
			//	  << " reading from disk."
				//  << std::endl;
	}

	const size_t size = vec ? vec->size() : ds.getSize();
	if (size > 0xFFFFFFFF) {
		throw file_read_exception(fname);    // File too large for zip file
	}

	if (zipOpenNewFileInZip(
				zipfile, remove_dir(fname.c_str()), nullptr, nullptr, 0, nullptr, 0,
				nullptr, Z_DEFLATED, Z_BEST_COMPRESSION)
		!= ZIP_OK) {
		return false;
	}

	if (vec) {
		zipWriteInFileInZip(zipfile, vec->data(), vec->size());
	} else {
		for (size_t readsofar = 0; readsofar < size;) {
			const size_t towrite = std::min(size - readsofar, MAX_SAVE_BUFFER);
			if (!towrite) {
				break;
			}
			ds.read(gamedat_in_memory.save_buffer, towrite);
			if (ds.fail()) {
				return false;
			}
			if (zipWriteInFileInZip(zipfile, gamedat_in_memory.save_buffer, towrite)
				!= ZIP_OK) {
				return false;
			}
			readsofar += towrite;
		}
	}

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

bool GameDat::Save_level2(
		zipFile& zipfile, const std::pmr::string& fname, bool required) {
	IFileDataSource ds;
	auto*           vec = get_memory_file(fname, false);

	if (!vec) {
		ds = IFileDataSource(U7open_in(fname));
		if (!ds.good()) {
			if (Game::is_editing() || !required) {
				return true;    // Newly developed game. or file is not
								// required. Missing file is ok.
			}
			throw file_read_exception(fname);
		}
	}

	const size_t size = vec ? vec->size() : ds.getSize();
	if (size > 0xFFFFFFFF) {
		throw file_read_exception(
				fname);    // File too large for level 2 compression
	}

	// Filename first
	std::memset(gamedat_in_memory.save_buffer, 0, 12);
	get_filename_from_path(fname).copy(gamedat_in_memory.save_buffer, 12);

	if (zipWriteInFileInZip(zipfile, gamedat_in_memory.save_buffer, 12) != ZIP_OK) {
		return false;
	}

	// Size of the file
	// Must be platform independent
	auto* ptr = gamedat_in_memory.save_buffer;
	little_endian::Write4(ptr, size);
	if (zipWriteInFileInZip(zipfile, gamedat_in_memory.save_buffer, 4) != ZIP_OK) {
		return false;
	}

	if (vec) {
		zipWriteInFileInZip(zipfile, vec->data(), vec->size());
	} else {
		for (size_t readsofar = 0; readsofar < size;) {
			const size_t towrite = std::min(size - readsofar, MAX_SAVE_BUFFER);
			if (!towrite) {
				break;
			}
			ds.read(gamedat_in_memory.save_buffer, towrite);
			if (ds.fail()) {
				return false;
			}
			if (zipWriteInFileInZip(zipfile, gamedat_in_memory.save_buffer, towrite)
				!= ZIP_OK) {
				return false;
			}
			readsofar += towrite;
		}
	}

	return true;
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
		const std::pmr::string& fname,      // File to create.
		const char*             savename    // User's savegame name.
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
	
		auto out = U7open_out(fname);
		if (out) {
			std::string title(savename);
			title.resize(0x50, '\0');
			out->write(title.data(), title.size());
			if (!out->good()) {
				throw file_write_exception(fname);
			}
		}
	
	zipFile zipfile = zipOpen(
				 std::allocate_shared<OFileDataSource>(
						std::pmr::polymorphic_allocator<char>(), std::move(out)));

	if (!zipfile) {
		throw file_write_exception(fname.c_str());
	}

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
				std::pmr::string iname_pmr(
						map->get_schunk_file_name(U7IREG, schunk, iname));
				if (fileExists(iname_pmr
							)) {
					if (!Save_level1(zipfile, iname_pmr)) {
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
				std::pmr::string iname_pmr(
						map->get_schunk_file_name(U7IREG, schunk, iname));
				if (fileExists(iname_pmr)) {
					if (!Save_level2(zipfile, iname_pmr)) {
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
	// Set default savegame name
	if (!savename) {
		savename = "";
	}
	std::cerr << "Trying to create an emergency save named \"" << savename
			  << "\"" << std::endl;

	// Write out current gamestate to gamedat in memory
	// Making the Screenshot necessaily uses unique_ptrs and can't allocate it
	// with the temporary allocator so as the process is unstable we skip making
	// the screenshot
	writetoMemory(false, true, false);

	save_gamedat(SaveInfo::Type::CRASHSAVE, savename);
}

GameDat::GameDat() {}

std::future<void> GameDat::writetoMemory(
		bool todisk, bool nopaint, bool screenshot) {
	// Promise to return if no waiting for disk writeneeded
	std::promise<void> p;
	p.set_value();
#ifdef DEBUG
	std::chrono::steady_clock::time_point start_time
			= std::chrono::steady_clock::now();
	#endif
	// Save it all to memory first
	if (cheat.in_map_editor()) {
		// In map editor, just do normal write
		gwin->write(true);
		GameDat::get()->write_saveinfo(screenshot);
		return p.get_future();
	}

	std::unique_lock memlock(gamedat_in_memory.get_mutex());
	// Starting save so clear 
	gamedat_in_memory.enable();

	gwin->write(nopaint);
	write_saveinfo(screenshot);

	#ifdef DEBUG
	std::chrono::steady_clock::time_point end_time
			= std::chrono::steady_clock::now();
	std::chrono::duration<double> elapsed_seconds = end_time - start_time;
	std::cerr << "Gamedat saved to memory in " << elapsed_seconds.count()
			  << " seconds\n";
	#endif
	if (todisk) {
		// Unlock so the async thread doesn't immediately block assming our caller doesn't also hold the lock
		// If caller holds the lock they will deadlock if they wait on the future before releasing the lock
		memlock.unlock();
		// Now write it all out to disk in a separate thread
		return std::async(std::launch::async, [this]() {
			std::unique_lock memlock(gamedat_in_memory.get_mutex());
			if (gamedat_in_memory.active) {
#ifdef DEBUG
				std::chrono::steady_clock::time_point start_time
						= std::chrono::steady_clock::now();
#endif
				// Write out all files to disk
				std::pmr::string fname_str;
				for (const auto& [fname, data] : *(gamedat_in_memory.files)) {
					fname_str = fname;
					auto out  = U7open_out(fname_str, false);
					if (out->good()) {
						out->write(
								reinterpret_cast<const char*>(data.data()),
								data.size());
					}
				}
#ifdef DEBUG
				std::chrono::steady_clock::time_point end_time
						= std::chrono::steady_clock::now();
				std::chrono::duration<double> elapsed_seconds
						= end_time - start_time;
				std::cerr << "Gamedat saved to disk in "
						  << elapsed_seconds.count() << " seconds\n";
#endif
			}
		});
	}
	return p.get_future();
}

std::pmr::vector<unsigned char>* GameDat::get_memory_file(
		std::pmr::string fname, bool create) {
	if (!gamedat_in_memory.mutex.try_lock()) {
		// Another thread accessing gamedat in memory; disallow access
		return nullptr;
	}

	std::lock_guard lock(gamedat_in_memory.mutex, std::adopt_lock);

	if (!gamedat_in_memory.active) {
		return nullptr;
	}
	// Must be a gamedat file
	if (Pentagram::strncasecmp(
				fname.c_str(), GAMEDAT, std::size(GAMEDAT) - 1)) {
		return nullptr;
	}
	auto             it = gamedat_in_memory.files->find(fname);
	if (it != gamedat_in_memory.files->end()) {
		return &it->second;
	}
	if (!create) {
		return nullptr;
	}

	auto vec = &(gamedat_in_memory.files->
						 emplace(
								 std::move(fname),
								 std::pmr::vector<unsigned char>())
						 .first->second);

	vec->reserve(gamedat_in_memory.initial_vcapacity);
	return vec;
}


template <size_t pool_size>
void GameDat::GamedatInMemory::BufferPoolResource<pool_size>::release() {
	offset               = 0;
	last_allocation_size = 0;
	free_blocks          = nullptr;
	high_water_mark      = 0;
	if (next_in_chain) {
		next_in_chain->release();
	}
	free_blocks_reentrancy_guard = true;
	free_blocks                  = new_object<std::pmr::deque<BlockInfo>>(this);

	// Preallocate some small blocks to avoid fragmentation
	for (int i = 0; i < 256; ++i) {
		current_block.offset = offset;
		current_block.size   = 64;
		offset += current_block.size;
		BlockInfo& ref = free_blocks->emplace_back();
		if (!current_block.size) {
			free_blocks->pop_back();
		} else {
			ref.offset           = current_block.offset;
			ref.size             = current_block.size;
			current_block.size   = 0;
			current_block.offset = 0;
		}
	}
	high_water_mark              = offset;
	free_blocks_reentrancy_guard = false;
}

template <size_t pool_size>
 bool GameDat::GamedatInMemory::BufferPoolResource<
		pool_size>::is_from_this_pool(const void* p) const noexcept {
	auto ptr = static_cast<const char*>(p);
	return ptr >= buffer.data() && ptr < buffer.data() + buffer.size();
}

template <size_t pool_size>
char* GameDat::GamedatInMemory::BufferPoolResource<pool_size>::strdup(
		const char* src) {
	if (!src) {
		return nullptr;
	}
	const size_t len = strlen(src);

	char* copy = static_cast<char *>(allocate(len + 1,1));
	strcpy(copy, src);
	return copy;
}

template <size_t pool_size>
void* GameDat::GamedatInMemory::BufferPoolResource<pool_size>::do_allocate(
		size_t bytes, size_t alignment) {
	if (current_block.size >= bytes) {
		size_t current = size_t(buffer.data()) + current_block.offset;
		size_t aligned = (current + alignment - 1) & ~(alignment - 1);
		size_t padding = aligned - current;
		if (current_block.size >= bytes + padding) {
			// Allocate from current block
			size_t new_offset    = current_block.offset + bytes + padding;
			last_allocation_size = bytes + padding;
			current_block.offset = new_offset;
			current_block.size -= bytes + padding;

			return reinterpret_cast<void*>(aligned);
		}
	}
	if (!free_blocks_reentrancy_guard && free_blocks && !free_blocks->empty()) {
		free_blocks_reentrancy_guard = true;
		typename std::pmr::deque<BlockInfo>::iterator smallest
				= free_blocks->begin();

		for (auto it = free_blocks->begin(); it != free_blocks->end(); ++it) {
			size_t current = size_t(buffer.data()) + it->offset;
			size_t aligned = (current + alignment - 1) & ~(alignment - 1);
			size_t padding = aligned - current;
			if (it->size >= bytes + padding) {
				if (it->size == bytes + padding) {
					// Perfect fit
					smallest = it;
					break;
				}
				if (it->size < smallest->size
					|| smallest->size < bytes + padding) {
					smallest = it;
				}
			}
		}
		size_t current = size_t(buffer.data()) + smallest->offset;
		size_t aligned = (current + alignment - 1) & ~(alignment - 1);
		size_t padding = aligned - current;
		if (smallest->size >= bytes + padding) {
			size_t remaining_size = smallest->size - (bytes + padding);
			if (remaining_size > 0) {
				// Add remaining block back to free blocks
				BlockInfo bi;
				bi.offset = smallest->offset + bytes + padding;
				bi.size   = remaining_size;

				*smallest = bi;
			} else {
				free_blocks->erase(smallest);
			}
			free_blocks_reentrancy_guard = false;
			return reinterpret_cast<void*>(aligned);
		}

		free_blocks_reentrancy_guard = false;
	}
	size_t current       = size_t(buffer.data()) + offset;
	size_t aligned       = (current + alignment - 1) & ~(alignment - 1);
	last_allocation_size = bytes + (aligned - current);
	size_t new_offset    = aligned + bytes - size_t(buffer.data());
	if (new_offset > pool_size) {
		if (bytes > next_size) {
			// Can't satisfy large allocation
			throw std::bad_alloc();
		}
		if (!next_in_chain) {
			// Create next in chain if this one is exhausted
			std::cout << "BufferPoolResource: Creating next in "
						 "chain of size "
					  << next_size << " bytes" << std::endl;
			next_in_chain = std::make_unique<BufferPoolResource<next_size>>();
		}
		return next_in_chain->allocate(bytes, alignment);
		throw std::bad_alloc();
	}
	offset          = new_offset;
	high_water_mark = std::max(high_water_mark, offset);
	return reinterpret_cast<void*>(aligned);
}


template <size_t pool_size>
void GameDat::GamedatInMemory::BufferPoolResource<pool_size>::do_deallocate(
		void* p, size_t bytes, size_t alignment) {
	ignore_unused_variable_warning(alignment);
	if (size_t(p) + bytes == size_t(buffer.data()) + offset) {
		// Last allocation, can deallocate
		offset -= last_allocation_size;
	} else if (
			!free_blocks_reentrancy_guard
			&& size_t(p) + bytes < size_t(buffer.data()) + offset
			&& size_t(p) >= offset) {
		free_blocks_reentrancy_guard = true;
		// pointer is in our buffer but not the last allocation
		// add to free blocks
		if (!free_blocks) {
			free_blocks = new_object<std::pmr::deque<BlockInfo>>(this);
		}
		current_block.offset = size_t(p) - size_t(buffer.data());
		current_block.size   = bytes;

		BlockInfo& ref = free_blocks->emplace_back();
		if (!current_block.size) {
			free_blocks->pop_back();
		} else {
			ref.offset           = current_block.offset;
			ref.size             = current_block.size;
			current_block.size   = 0;
			current_block.offset = 0;
		}

		free_blocks_reentrancy_guard = false;
	} else if (next_in_chain) {
		next_in_chain->deallocate(p, bytes, alignment);
	}
}

template <size_t pool_size>
GameDat::GamedatInMemory::BufferPoolResource<
		pool_size>::BufferPoolResource() {
	release();
}

// Explicit template instantiations
template class GameDat::GamedatInMemory::BufferPoolResource<
		GameDat::GamedatInMemory::pool_size>;
template class GameDat::GamedatInMemory::BufferPoolResource<
		GameDat::GamedatInMemory::BufferPoolResource<
				GameDat::GamedatInMemory::pool_size>::next_size>;

GameDat::GamedatInMemory::GamedatInMemory()
		: pool(), active(false), files(nullptr), save_buffer(nullptr)
		   {
	clear();
}

void GameDat::GamedatInMemory::clear() {
	std::unique_lock lock(mutex);
#ifdef DEBUG
	if (active) {
		std::cout << "GamedatInMemory::clear() high water mark was "
				  << pool.get_high_water_mark() << std::endl;
	}
#endif
	disable();

	// Release all the pool memory. No one should be using it at this point.
	pool.release();

	// Recreate the files map and save buffer

	files = pool.new_object<
			std::pmr::unordered_map<
					std::pmr::string, std::pmr::vector<unsigned char>>>(&pool);
	save_buffer = static_cast<char*>(pool.allocate(MAX_SAVE_BUFFER));
}

bool GameDat::GamedatInMemory::enable() {
		if (!mutex.try_lock()) {
		// Another thread accessing gamedat in memory; disallow access
		return false;
	}

	std::lock_guard lock(mutex, std::adopt_lock);
	// Already active do nothing return success
	if (active)
	{
		return true;
	}

	// Clear everything before enabling
	clear();
	std::pmr::set_default_resource(&pool);
	active = true;
	return true;
}

void GameDat::GamedatInMemory::disable() {
	std::unique_lock lock(mutex);

	// Reset the default resource back to the runitme default
	std::pmr::set_default_resource(nullptr);
	active = false;
}

void GameDat::Queue_Autosave(
		int gflag, int map_from, int map_to, int sc_from, int sc_to) {
	// No autosaves if haven't done first scene	
	auto usecode = gwin->get_usecode();
	if ((GAME_BG && !usecode->get_global_flag_bool(Usecode_machine::did_first_scene))
			|| (GAME_SI
				&& !usecode->get_global_flag_bool(
					Usecode_machine::si_did_first_scene)))
	{
		std::cout << "Skipping autosave: haven't done first scene" << std::endl;
		return;

	}

	auto tqueue = gwin->get_tqueue();
	auto lock   = tqueue->get_lock();
	if (autosave_event.in_queue()) {
		// An autosave is already queued
		return;
	}
	autosave_event.gflag    = gflag;
	autosave_event.map_from = map_from;
	autosave_event.map_to   = map_to;
	autosave_event.sc_from  = sc_from;
	autosave_event.sc_to    = sc_to;

	// Queue the autosave event to happen immediately at next opportunity
	tqueue->add(0, &autosave_event);
}

void GameDat::Autosave_Now(
		const char* savemessage, int gflag, int map_from, int map_to,
		int sc_from, int sc_to, bool wait, bool screenshot) {

	gamedat_in_memory.clear();
	SaveInfo::Type type = SaveInfo::Type::AUTOSAVE;

	char autosave_name[100] = "";

	// Decrement save count so the Autosave does not count when write_saveinfo
	// incremets it
	save_count--;

	if (savemessage) {
		// caller defined autosave message
		cout << "Want to Autosave with message: " << savemessage << std::endl;
		if (Settings::get().disk.autosave_count == 0) {
			std::cout << "Autosaves disabled, skipping autosave." << std::endl;
			return;
		}
		snprintf(
				autosave_name, sizeof(autosave_name), "%s%s",
				Strings::AutoSave(),savemessage);
	} else if (gflag != -1) {
		std::cout << "Want to Autosave for gflag " << gflag << std::endl;
		if (Settings::get().disk.flagautosave_count == 0) {
			
			std::cout << "Flag autosaves disabled, skipping autosave."
					  << std::endl;
			return;
		}
		snprintf(
				autosave_name, sizeof(autosave_name), "%s%02d",
				Strings::AutosaveGF(), gflag);
		type = SaveInfo::Type::FLAG_AUTOSAVE;
	} else if (sc_from != -1 && sc_to != -1) {
		std::cout << "Want to Autosave moving from schunk " << sc_from
				  << " in map " << map_from << " to schunk " << sc_to << " in map " << map_to << std::endl;
		if (Settings::get().disk.autosave_count == 0) {
			std::cout << "Autosaves disabled, skipping autosave." << std::endl;
			return;
		}
		int from_x = sc_from % c_num_schunks;
		int from_y = sc_from / c_num_schunks;
		int to_x   = sc_to % c_num_schunks;
		int to_y   = sc_to / c_num_schunks;
		snprintf(
				autosave_name, sizeof(autosave_name),
				"%s%02d,%02d,%d->%02d,%02d,%d", Strings::AutoSave(), 
				from_x,	from_y,	map_from, 
				to_x, to_y, map_to);
	}

	auto wtm_f = gamedat->writetoMemory(
			Settings::get().disk.autosaves_write_to_gamedat, true, screenshot);

	auto &sg_f = save_gamedat_async(type, autosave_name);
	// If wait requested, wait on both futures
	if (wait) {
		sg_f.wait();
		wtm_f.wait();
	}
}

void GameDat::Quicksave() {
	gamedat_in_memory.clear();
	auto f = writetoMemory(true, false, true);
	
	save_gamedat_async(SaveInfo::Type::QUICKSAVE, "").wait();
	f.wait();
}

void GameDat::Savegame(
		const char* fname, const char* savename, bool no_paint,
		bool screenshot) {

	auto f = writetoMemory(true, no_paint, screenshot);
	save_gamedat_async(fname, savename).wait();
	f.wait();
}

void GameDat::Savegame(const char* savename, bool no_paint, bool screenshot) {
	auto f = writetoMemory(true, no_paint, screenshot);
	save_gamedat_async(SaveInfo::Type::REGULAR, savename).wait();
	f.wait();
}

void GameDat::Extractgame(const char* fname, bool doread) {
	gamedat_in_memory.clear();
	// Only restore if a filename is given
	if (fname && *fname) {
		restore_gamedat(fname);
	}

	if (doread) {
		gwin->read();
	}
	gamedat_in_memory.clear();
}

void GameDat::Autosave_Event::handle_event(
		unsigned long curtime, uintptr udata) {
	ignore_unused_variable_warning(curtime);
	ignore_unused_variable_warning(udata);
	
	// If don't move is set delay autosave till flag is cleared
	if (gwin->main_actor_dont_move())
	{
		auto tqueue = gwin->get_tqueue();
		auto lock   = tqueue->get_lock();
		// Queue the autosave event to happen again next frame
		tqueue->add(curtime+1, this);
		return;
	}
	gamedat->Autosave_Now(nullptr, gflag, map_from, map_to, sc_from, sc_to,false, true);
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
			// Flag Autosaves have 'f' at the end
		} else if (std::tolower(filename_[filename_.size() - 5]) == 'f') {
			type = Type::FLAG_AUTOSAVE;
			// crashsaves have 'c' at the end
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
	SaveInfo::Type t = type;
	SaveInfo::Type ot = other.type;

	// Treat flag autosaves the same as regular autosaves
	if (t == SaveInfo::Type::FLAG_AUTOSAVE) {
		t = SaveInfo::Type::AUTOSAVE;
	}
	if (ot == SaveInfo::Type::FLAG_AUTOSAVE) {
		ot = SaveInfo::Type::AUTOSAVE;
	}

	if (t != ot && Settings::get().disk.savegame_group_by_type) {
		return int(ot) - int(t);
	}

	if (Settings::get().disk.savegame_sort_by == Settings::Disk::SORTBY_NAME) {
		int namecomp = Pentagram::strcasecmp(
				this->savename.c_str(), other.savename.c_str());

		if (namecomp != 0) {
			return namecomp;
		}
	}

	if (details && other.details) {
		// Sort by time

		if (Settings::get().disk.savegame_sort_by
			== Settings::Disk::SORTBY_GAMETIME) {
			int datecomp = details.CompareGameTime(other.details);
			if (datecomp != 0) {
				return datecomp;
			}
		}

		int datecomp = details.CompareRealTime(other.details);
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
		return other.real_year - real_year;
	}

	if (real_month != other.real_month) {
		return other.real_month - real_month;
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
