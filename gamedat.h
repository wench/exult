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

#include "databuf.h"
#include "items.h"
#include "palette.h"
#include "singles.h"
#include "tqueue.h"
#include "vgafile.h"

#include <array>
#include <chrono>
#include <deque>
#include <future>
#include <memory>
#include <memory_resource>
#include <mutex>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

class IDataSource;
class Flex_writer;
class unzFile;
class zipFile;
class zipFile;
class Shape_file;


class GameDat : protected Game_singletons {
public:
	static constexpr int MAX_SAVEGAME_NAME_SIZE = 0x50;

	struct Strings {
		static auto AutoSave() {
			return get_text_msg(0x6EA - msg_file_start);
		}

		static auto QuickSave() {
			return get_text_msg(0x6EB - msg_file_start);
		}

		static auto CrashSave() {
			return get_text_msg(0x6EC - msg_file_start);
		}

		static auto Save() {
			return get_text_msg(0x6ED - msg_file_start);
		}

		static auto LostFocus() {
			return get_text_msg(0x6EE - msg_file_start);
		}

		static auto AutosaveGF() {
			return get_text_msg(0x6EF - msg_file_start);
		}
	};

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

		short save_count;       // 12
		char old_party_size;    // 13 Party size is stored on disk here but this
								// field is no longer used

		bool cheated;   // 14 has the player used a cheat

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
		std::unique_ptr<Shape_file> screenshot = nullptr;
		std::unique_ptr<Palette>    palette    = nullptr;

		// Default constructor is allowed
		SaveInfo() {}

		// Move Constructor from a std::string filename
		SaveInfo(std::string&& filename);

		// No copy constructor as screenshot can't be copied because Shape_file
		// has no copy constructor

		SaveInfo(const SaveInfo&) = delete;
		SaveInfo(SaveInfo&&)      = default;

		// Copy from exising object but with move for a Screenshot
		SaveInfo(
				const SaveInfo&               other,
				std::unique_ptr<Shape_file>&& newscreenshot)
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
			FLAG_AUTOSAVE,
			QUICKSAVE,
			CRASHSAVE,
			NUM_TYPES
		} type = Type::UNKNOWN;

	constexpr static int NUM_TYPES = static_cast<int>(Type::NUM_TYPES);

		// const getter for filename
		const std::string& filename() const {
			return filename_;
		}

		int compare(const SaveInfo& other) const noexcept;

		bool operator<(const SaveInfo& other) const noexcept {
			return compare(other) < 0;
		}
	};

	int save_count = 0;

	std::vector<SaveInfo>                           save_infos      = {};
	std::array<int, int(SaveInfo::Type::NUM_TYPES)> first_free      = {};
	std::array<int, SaveInfo::NUM_TYPES>            oldest          = {};
	std::string                                     save_mask       = "";
	std::shared_future<void>                        saveinfo_future = {};
	std::recursive_mutex                            save_info_mutex = {};
	std::atomic_bool                                save_info_cancel = {};

	static constexpr size_t MAX_SAVE_BUFFER = 64 * 1024;    // 64 KB

	struct GamedatInMemory {
		std::recursive_mutex     mutex  = {};

		// A simple memory resource that hands out memory from a fixed buffer
		// It will reuse memory blocks when they are deallocated
		// If the buffer is exhausted it will chain to another
		// used for temporary allocations when loading/saving games
		template <size_t pool_size>
		class BufferPoolResource : public std::pmr::memory_resource {
			struct BlockInfo {
				size_t offset;
				size_t size;
			};

			volatile BlockInfo          current_block                = {0, 0};
			std::pmr::deque<BlockInfo>* free_blocks                  = nullptr;
			bool                        free_blocks_reentrancy_guard = false;
			std::array<char, pool_size> buffer;
			size_t                      size                 = pool_size;
			size_t                      offset               = 0;
			size_t                      last_allocation_size = 0;
			size_t                      high_water_mark      = 0;

			constexpr static size_t next_size = 1024 * 1024;    // 1 MB
			std::unique_ptr<BufferPoolResource<next_size>> next_in_chain
					= nullptr;

		public:
			BufferPoolResource();

			void release();

			size_t get_high_water_mark() const {
				size_t high = high_water_mark;
				if (next_in_chain) {
					high += next_in_chain->get_high_water_mark();
				}
				return high;
			}

			template <typename T, typename... Args>
			T* new_object(Args&&... args) {
				void* p = allocate(sizeof(T), alignof(T));
				return new (p) T(std::forward<Args>(args)...);
			}

			bool is_from_this_pool(const void* p) const noexcept;

			char* strdup(const char* src);

		protected:
			void* do_allocate(size_t bytes, size_t alignment) override;

			void do_deallocate(
					void* p, size_t bytes, size_t alignment) override;

			bool do_is_equal(const std::pmr::memory_resource& other)
					const noexcept override {
				return this == &other;
			}
		};

		// Initial Pool with 5 MB buffer
		static const size_t           pool_size = 5 * 1024 * 1024;
		BufferPoolResource<pool_size> pool;
		bool                          active = false;

		template<typename T=char>
		std::pmr::polymorphic_allocator<T> get_allocator() {
			return &pool;
		}

		std::pmr::unordered_map<
				std::pmr::string, std::pmr::vector<unsigned char>>* files;

		// Initial capacity for new savegame memory files
		// 512 bytes seems like a good initial size in testing as the IREG
		// files are usually at least this big and there are hundreds of those
		// so preallocating this much isn't very wasteful
		static const size_t initial_vcapacity = 512;

		char* save_buffer = nullptr;

		std::recursive_mutex& get_mutex() {
			return mutex;
		}

		GamedatInMemory();
		void clear();
		bool enable();
		void disable();

	} gamedat_in_memory;

	static void init() {
		gamedat = new GameDat();
	}

public:
	friend class Game_window;
	GameDat();

	static GameDat* get() {
		return gamedat;
	}

	void read_save_infos_async(bool force);
	void get_saveinfo(
			std::unique_ptr<Shape_file>& map, SaveGame_Details& details,
			std::vector<SaveGame_Party>& party, bool current);

	// Get Vector of all savegame info
	const std::vector<SaveInfo>* GetSaveGameInfos(bool force);

	bool are_save_infos_loaded();

	void wait_for_saveinfo_read();

	// Get the filename for savegame num of specified SaveInfo:Type

	std::pmr::string get_save_filename(
			int num, SaveInfo::Type type);

	// Emergency save Creates a new save in the next available index
	// It preserves the existing GAMEDAT
	// and it does not paint the saving game message on screen or create the
	// miniscreenshot
	void MakeEmergencySave(const char* savename = nullptr);

	// Wite gamedat files to memory in order to save the game. Do not hold
	// gamedat_in_memory lock before waiting on returned future returned future
	// waits for disk writing to complete if todisk argument is true

	void writetoMemory(bool nopaint, bool screenshot);

	bool writeMemorytoDisk();

	
	ODataSourceFileOrVector<std::pmr::polymorphic_allocator> Open_ODataSource(
			const std::pmr::string& fname) {
		return ODataSourceFileOrVector<std::pmr::polymorphic_allocator>(
				get_memory_file(fname, true), fname);
	}	
	auto Open_ODataSource(const char* fname) {
		return Open_ODataSource(
				std::pmr::string(fname));
	}

	std::shared_ptr<std::ostream> Open_ostream(const std::pmr::string &fname) {
		auto memfile = get_memory_file(fname, true);
		if (!memfile) {
			return U7open_out(
					std::pmr::string(fname),
					false);
		}
		std::shared_ptr<ODataSource> ds
				= std::allocate_shared<OVectorDataSource<
						std::pmr::polymorphic_allocator<unsigned char>>>(
						gamedat_in_memory.get_allocator<unsigned char>(), memfile);

		return std::allocate_shared<ODataSource_ostream>(
				gamedat_in_memory.get_allocator<ODataSource_ostream>(), ds);
	}

private:
	void write_saveinfo(
			bool screenshot = true);    // Write the save info to gamedat

	std::pmr::vector<unsigned char>* get_memory_file(
			const std::pmr::string &fname, bool create);


	std::shared_ptr<IDataSource> Open_IDataSource(std::pmr::string& fname) {
		auto memfile = get_memory_file(fname, false);
		if (memfile) {
			return std::allocate_shared<IBufferDataView>(
					gamedat_in_memory.get_allocator<IBufferDataView>(), memfile->data(),
					memfile->size());
		} else {
			return std::allocate_shared<IFileDataSource>(
					gamedat_in_memory.get_allocator<IFileDataSource>(),
					U7open_in(fname, false));
		}
	}

	// Explode a savegame into "gamedat".
	void restore_gamedat(const char* fname);

	// Save "gamedat" to a new savegame with the given filename
	void save_gamedat(const std::pmr::string& fname, const char* savename);
	// Save gamedat to a new savegame of the specified SaveInfo:Type with the
	// given name
	void save_gamedat(SaveInfo::Type type, const char* savename);

	// Save Gamedat to a savegame asyncronously
	// Savegame thread will block while gamedat_in_memory is locked so do not
	// wait on the returned future if holding the gamedat_in_memory lock
	std::shared_future<void>& save_gamedat_async(
			std::variant<SaveInfo::Type, const char*> type_or_filename,
			const char*                               savename);
	void init(bool newgame);    // Read the save info from gamedat

	void restore_flex_files(IDataSource& in, const char* basepath);

	bool get_saveinfo(
			const std::string& filename, std::string& name,
			std::unique_ptr<Shape_file>& map, SaveGame_Details& details,
			std::vector<SaveGame_Party>& party,
			std::unique_ptr<Palette>&    palette);

	void clear_saveinfos();

	void SaveToFlex(
			Flex_writer&      flex,
			std::pmr::string& fname    // Name of file to save.
	);
	void save_chunks_to_flex(Game_map* map, Flex_writer& flex);

	bool read_saveinfo(
			IDataSource* in, SaveGame_Details& details,
			std::vector<SaveGame_Party>& party);
	bool fileExists(const std::pmr::string& fname);

	bool fileExists(const char* fname) {
		return fileExists(std::pmr::string(fname, gamedat_in_memory.get_allocator()));
	}
#ifdef HAVE_ZIP_SUPPORT
	bool get_saveinfo_zip(
			const char* fname, std::string& name,
			std::unique_ptr<Shape_file>& map, SaveGame_Details& details,
			std::vector<SaveGame_Party>& party,
			std::unique_ptr<Palette>&    palette);
	bool save_gamedat_zip(const std::pmr::string& fname, const char* savename);
	bool Restore_level2(unzFile& unzipfile, const char* dirname, int dirlen);
	bool restore_gamedat_zip(const char* fname);
	bool Save_level2(
			zipFile& zipfile, const std::pmr::string& fname,
			bool required = true);

	bool Save_level2(
			zipFile& zipfile, const char* fname, bool required = true) {
		return Save_level2(
				zipfile, std::pmr::string(fname, gamedat_in_memory.get_allocator()),
				required);
	}

	bool Save_level1(
			zipFile& zipfile, const std::pmr::string& fname,
			bool required = true);

	bool Save_level1(
			zipFile& zipfile, const char* fname, bool required = true) {
		return Save_level1(
				zipfile, std::pmr::string(fname, gamedat_in_memory.get_allocator()),
				required);
	}

	bool Begin_level2(zipFile& zipfile, int mapnum);
	bool End_level2(zipFile& zipfile);
#endif

	void read_save_infos();

	class Autosave_Event : public Time_sensitive {
	public:
		int map_from = -1;
		int map_to   = -1;
		int sc_from  = -1;
		int sc_to    = -1;
		int gflag    = -1;

		void handle_event(unsigned long curtime, uintptr udata) override;

	} autosave_event;

std::chrono::steady_clock::time_point last_autosave = {};

public:
	// Queue an autosave to occur at the next possible time. Safe to call at any
	// time and from other threads.
	// Only one Autosave can be queued at a time
	void Queue_Autosave(
			int gflag = -1, int map_from = -1, int map_to = -1,
			int sc_from = -1, int sc_to = -1);

	// Do an immediate Autosave. Should Only be called from main thread
	void Autosave_Now(
			const char* savemessage = nullptr, int gflag = -1,
			int map_from = -1, int map_to = -1, int sc_from = -1,
			int sc_to = -1, bool noasync = true, bool screenshot = false);

	void Quicksave();

	void Savegame(
			const char* fname, const char* savename, bool no_paint,
			bool screenshot);
	void Savegame(const char* savename, bool no_paint, bool screenshot);

	void Extractgame(const char* fname, bool doread);

	void Load() {
		Extractgame(nullptr, true);
	}

	void ResortSaveInfos();

	// Get temporary memory allocator for use while saving/loading
	// Once saving/loading is complete, all memory allocated with this allocator
	// is freed so it should only be used for temporary data
	template<typename T=char>
	auto getTempAlloc() {
		return gamedat_in_memory.get_allocator<T>();
	}

	// Delete a savegame file. This will invalidate any existing SaveInfo
	// vectors
	void DeleteSaveGame(const std::string& fname);
};
#endif    // SAVEINFO_H_INCLUDED
