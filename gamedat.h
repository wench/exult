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
#include <cstddef>
#include <deque>
#include <future>
#include <memory>
#include <memory_resource>
#include <mutex>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

class IDataSource;
class Flex_writer;
class unzFile;
class zipFile;
class zipFile;
class Shape_file;

#define MAX_SAVEGAME_NAME_LEN 0x50

class GameDat : protected Game_singletons {
public:
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

		short save_count;        // 12
		char  old_party_size;    // 13 Party size is stored on disk here but this
								 // field is no longer used

		bool cheated;    // 14 has the player used a cheat

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
		SaveInfo(const SaveInfo& other, std::unique_ptr<Shape_file>&& newscreenshot)
				: filename_(other.filename_), num(other.num), savename(other.savename), readable(other.readable),
				  details(other.details), party(other.party), screenshot(std::move(newscreenshot)) {
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

	constexpr static size_t MAX_SAVE_BUFFER = 64 * 1024;    // 64 KB

	struct GamedatInMemory {
		std::recursive_mutex mutex = {};

		// A simple memory resource that hands out memory from a fixed buffer
		// It will reuse memory blocks when they are deallocated
		// If the buffer is exhausted it will chain to another
		// used for temporary allocations when loading/saving games
		// Zero byte allocations are allowed and will use up a Block of memory so are not free
		template <size_t pool_size>
		class BlockPoolResource : public std::pmr::memory_resource {
			// friend to all instantiations of us
			template <size_t>
			friend class GameDat::GamedatInMemory::BlockPoolResource;

			struct Block {
				constexpr static size_t minsplit = 16;    // Minimum size of new blocks that can be created by ShrinkAndSplit
				uintptr                 check;
				size_t                  size = 0;
				Block*                  prev = nullptr;
				Block*                  next = nullptr;

				Block() = default;

				Block(size_t size) : size(size) {}

				~Block() {
					RemoveThis();
					size = 0;
				}

				uintptr calculate_check() {
					return uintptr(this) ^ size ^ uintptr(prev) ^ uintptr(next);
				}

				bool validate_check() {
					return check == calculate_check();
				}

				bool RemoveThis() {
					// Must be in the list and be a valid block
					if (!prev || !next || !size) {
						return false;
					}
					next->prev = prev;
					prev->next = next;
					next       = nullptr;
					prev       = nullptr;
					check      = calculate_check();
					return true;
				}

				bool Insert(Block* toinsert, bool before = false) {
					// toinsert must have a size, this must be in the list
					if (!next || !prev || !toinsert || !toinsert->size) {
						return false;
					}

					// If to insert is in the list, remove it
					if (toinsert->next || toinsert->prev) {
						if (!toinsert->RemoveThis()) {
							return false;
						}
					}

					if (before) {
						prev->next     = toinsert;
						toinsert->prev = prev;
						toinsert->next = this;
						prev           = toinsert;
					} else {
						next->prev     = toinsert;
						toinsert->next = next;
						toinsert->prev = this;
						next           = toinsert;
					}
					check = calculate_check();
					return true;
				}

				// Shrink the current block and split off the remaining space innto a new block if possible
				// returns the address of the resized buffer
				// throws invalid_argument if new_size is bigger than the blocks current size or alingment is not power of 2
				uintptr ShrinkAndSplit(size_t new_size, size_t new_alignment);

				static bool TryMergeBlocks(Block* block1, Block* block2);

				static Block* FromAddress(uintptr addr, size_t size, size_t alignment);

				uintptr GetAlignedAddressForBlockBuffer(size_t size, size_t alignment);

			} free_list;

			alignas(Block) std::byte buffer[pool_size];
			size_t             high_water_mark = 0;
			size_t             allocated       = 0;
			mutable std::mutex mutex           = {};    // Just a plain mutex because no methods are reenterant

			constexpr static size_t                       next_size     = 1024 * 1024;    // 1 MB
			std::unique_ptr<BlockPoolResource<next_size>> next_in_chain = nullptr;

		public:
			BlockPoolResource();

			void release();

			size_t get_high_water_mark() const {
				size_t high = high_water_mark;
				if (next_in_chain) {
					high += next_in_chain->get_high_water_mark();
				}
				return high;
			}

			static bool is0orPow2(size_t val) {
				return val ? (val & (val - 1)) == 0 : true;
			}

			static uintptr AlignAddress(uintptr addr, uintptr alignment, bool greater) {
				if (!is0orPow2(alignment)) {
					throw std::invalid_argument("alignment is not a power of 2");
				}
				alignment--;
				if (greater) {
					addr += alignment;
				}
				return addr & ~(alignment);
			}

			template <typename T, typename... Args>
			T* new_object(Args&&... args) {
				void* p = allocate(sizeof(T), alignof(T));
				return new (p) T(std::forward<Args>(args)...);
			}

			/// Create an array of type T
			/// \param value_initialize If set value initialize all elements otherwise all elements are default initialized
			template <typename T>
			T* new_array(size_t count, bool value_initialize) {
				// If T is CV qualified get rid of CV
				using Base_T = typename std::remove_cv<T>::type;

				Base_T* p = reinterpret_cast<Base_T*>(allocate(sizeof(Base_T) * count, alignof(Base_T)));

				if (value_initialize) {
					for (size_t i = 0; i < count; i++) {
						new (reinterpret_cast<void*>(p + i)) Base_T();
					}
				} else {
					// Default initialize
					// for trvival types this should get optimized away
					for (size_t i = 0; i < count; i++) {
						new (reinterpret_cast<void*>(p + i)) Base_T;
					}
				}

				return p;
			}

			template <typename T>
			void delete_object(T*& obj) {
				if (is_from_this_pool(obj)) {
					obj->~T();
					deallocate(obj, sizeof(T), alignof(T));
					obj = nullptr;
				}
			}

			// Deallocate array created using new_array destructing every element in reverse order as if deleted using delete[]
			template <typename T>
			void delete_array(T*& array, size_t count) {
				;
				if (is_from_this_pool(array)) {
					// Only call destructors if it's not trivial
					if constexpr (!std::is_trivially_destructible_v<T>) {
						auto i = count;

						while (i--) {
							array[i].~T();
						}
					}
					deallocate(array, sizeof(T) * count, alignof(T));
					array = nullptr;
				}
			}

			// Public version of is_from_this that holds a lock
			bool is_from_this_pool(const void* p) {
				if (!p) {
					return false;
				}
				auto lock = std::lock_guard(mutex);

				return is_from_this(p, true);
			}

		protected:
			bool is_from_this(const void* p, bool inc_next) const noexcept {
				if (!p) {
					return false;
				}
				// Just make sure that p is in buffer or if inc_next is true
				// recursively call on next_in_chain
				auto ptr = reinterpret_cast<const std::byte*>(p);
				return (ptr >= buffer && ptr < (buffer + pool_size))
					   || (inc_next && next_in_chain && next_in_chain->is_from_this(p, true));
			}

			void* do_allocate(size_t bytes, size_t alignment) override;

			void do_deallocate(void* p, size_t bytes, size_t alignment) override;

			bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
				return this == &other;
			}

		public:
			template <typename T>
			class Deleter {
				using Base_T                  = typename std::remove_all_extents<T>::type;
				BlockPoolResource* bpr        = nullptr;
				size_t             array_size = 0;

			public:
				using pointer = Base_T*;

				Deleter() {}

				// Constructor for array types
				Deleter(BlockPoolResource* bpr, size_t array_size) : bpr(bpr), array_size(array_size) {				}

				// Constructor for Non array types
				Deleter(BlockPoolResource* bpr) : bpr(bpr) { 
					assert(std::is_array<T>::value == false);
				}

				void operator()(pointer ptr) const {
					if (!bpr || !ptr) {
						return;
					}
					if constexpr (std::is_array<T>::value) {
						bpr->delete_array<>(ptr, array_size);
					} else {
						bpr->delete_object(ptr);
					}
				}
			};

			// Make_unique for array type
			template <typename T, std::enable_if_t<std::is_array<T>::value,bool> =true>
			auto make_unique(std::size_t size) {
				using Base_T = typename std::remove_all_extents<T>::type;
				return std::unique_ptr<T, Deleter<T>>(new_array<Base_T>(size, true), {this, size});
			}

			template <typename T,  std::enable_if_t<std::is_array<T>::value,bool> =true>
			auto make_unique_for_overwrite(std::size_t size) {
				using Base_T = typename std::remove_all_extents<T>::type;
				return std::unique_ptr<T, Deleter<T>>(new_array<Base_T>(size, false), {this, size});
			}

			// Make_unique for non array type
			template <typename T, typename... Args, std::enable_if_t<!std::is_array<T>::value,bool> =true>
			auto make_unique(Args&&... args) {
				return std::unique_ptr<T, Deleter<T>>(new_object<T>(std::forward<Args>(args)...), {this});
			}

			template <typename T, typename... Args>
			auto make_shared(Args&&... args) {
				return std::allocate_shared<T>(std::pmr::polymorphic_allocator<T>(this), std::forward<Args>(args)...);
			}

			char* strdup(const char* src, Deleter<char[]>& deleter);
		};

		// Initial Pool with 5 MB buffer
		static const size_t pool_size = 5 * 1024 * 1024;
		using pool_t                  = BlockPoolResource<pool_size>;
		pool_t pool;
		bool   active = false;

		template <typename T>
		using unique_ptr = std::unique_ptr<T, BlockPoolResource<pool_size>::Deleter<T>>;

		template <typename T = char>
		std::pmr::polymorphic_allocator<T> get_allocator() {
			return &pool;
		}

		unique_ptr<std::pmr::unordered_map<std::pmr::string, std::pmr::vector<unsigned char>>> files;

		// Initial capacity for new savegame memory files
		// 512 bytes seems like a good initial size in testing as the IREG
		// files are usually at least this big and there are hundreds of those
		// so preallocating this much isn't very wasteful
		static const size_t initial_vcapacity = 512;

		unique_ptr<char[]> save_buffer;

		std::recursive_mutex& get_mutex() {
			return mutex;
		}

		GamedatInMemory();
		void clear();
		bool enable();
		void disable();
		void mt_test();
	}

	gamedat_in_memory;

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
			std::unique_ptr<Shape_file>& map, SaveGame_Details& details, std::vector<SaveGame_Party>& party, bool current);

	// Get Vector of all savegame info
	const std::vector<SaveInfo>* GetSaveGameInfos(bool force);

	bool are_save_infos_loaded();

	void wait_for_saveinfo_read();

	// Get the filename for savegame num of specified SaveInfo:Type

	std::pmr::string get_save_filename(int num, SaveInfo::Type type);

	// Emergency save Creates a new save in the next available index
	// It preserves the existing GAMEDAT
	// and it does not paint the saving game message on screen or create the
	// miniscreenshot
	void MakeEmergencySave(const char* savename = nullptr);

	// Wite gamedat files to memory in order to save the game. Do not hold
	// gamedat_in_memory lock before waiting on returned future returned future
	// waits for disk writing to complete if todisk argument is true

	std::future<void> writetoMemory(bool todisk, bool nopaint, bool screenshot);

	auto Open_ODataSource(const char* fname) {
		return ODataSourceFileOrVector<std::pmr::polymorphic_allocator>(get_memory_file(fname, true), std::pmr::string(fname));
	}

	std::shared_ptr<std::ostream> Open_ostream(const char* fname) {
		auto memfile = get_memory_file(fname, true);
		if (!memfile) {
			return U7open_out(std::pmr::string(fname), false);
		}
		std::shared_ptr<ODataSource> ds
				= gamedat_in_memory.pool.make_shared<OVectorDataSource<std::pmr::polymorphic_allocator<unsigned char>>>(memfile);

		return gamedat_in_memory.pool.make_shared<ODataSource_ostream>(ds);
	}

private:
	void write_saveinfo(bool screenshot = true);    // Write the save info to gamedat

	std::pmr::vector<unsigned char>* get_memory_file(std::pmr::string fname, bool create);

	std::shared_ptr<IDataSource> Open_IDataSource(std::pmr::string& fname) {
		auto memfile = get_memory_file(fname, false);
		if (memfile) {
			return gamedat_in_memory.pool.make_shared<IBufferDataView>(memfile->data(), memfile->size());
		} else {
			return gamedat_in_memory.pool.make_shared<IFileDataSource>(U7open_in(fname, false));
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
	std::shared_future<void>& save_gamedat_async(std::variant<SaveInfo::Type, const char*> type_or_filename, const char* savename);
	void                      read_saveinfo(bool newgame);    // Read the save info from gamedat

	void restore_flex_files(IDataSource& in, const char* basepath);

	bool get_saveinfo(
			const std::string& filename, std::string& name, std::unique_ptr<Shape_file>& map, SaveGame_Details& details,
			std::vector<SaveGame_Party>& party, std::unique_ptr<Palette>& palette);

	void clear_saveinfos();

	void SaveToFlex(
			Flex_writer&      flex,
			std::pmr::string& fname    // Name of file to save.
	);
	void save_chunks_to_flex(Game_map* map, Flex_writer& flex);

	bool read_saveinfo(IDataSource* in, SaveGame_Details& details, std::vector<SaveGame_Party>& party);
	bool fileExists(const std::pmr::string& fname);

	bool fileExists(const char* fname) {
		return fileExists(std::pmr::string(fname, gamedat_in_memory.get_allocator()));
	}
#ifdef HAVE_ZIP_SUPPORT
	bool get_saveinfo_zip(
			const char* fname, std::string& name, std::unique_ptr<Shape_file>& map, SaveGame_Details& details,
			std::vector<SaveGame_Party>& party, std::unique_ptr<Palette>& palette);
	bool save_gamedat_zip(const std::pmr::string& fname, const char* savename);
	bool Restore_level2(unzFile& unzipfile, const char* dirname, int dirlen);
	bool restore_gamedat_zip(const char* fname);
	bool Save_level2(zipFile& zipfile, const std::pmr::string& fname, bool required = true);

	bool Save_level2(zipFile& zipfile, const char* fname, bool required = true) {
		return Save_level2(zipfile, std::pmr::string(fname, gamedat_in_memory.get_allocator()), required);
	}

	bool Save_level1(zipFile& zipfile, const std::pmr::string& fname, bool required = true);

	bool Save_level1(zipFile& zipfile, const char* fname, bool required = true) {
		return Save_level1(zipfile, std::pmr::string(fname, gamedat_in_memory.get_allocator()), required);
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

public:
	// Queue an autosave to occur at the next possible time. Safe to call at any
	// time and from other threads.
	// Only one Autosave can be queued at a time
	void Queue_Autosave(int gflag = -1, int map_from = -1, int map_to = -1, int sc_from = -1, int sc_to = -1);

	// Do an immediate Autosave. Should Only be called from main thread
	void Autosave_Now(
			const char* savemessage = nullptr, int gflag = -1, int map_from = -1, int map_to = -1, int sc_from = -1, int sc_to = -1,
			bool wait = true, bool screenshot = false);

	void Quicksave();

	void Savegame(const char* fname, const char* savename, bool no_paint, bool screenshot);
	void Savegame(const char* savename, bool no_paint, bool screenshot);

	void Extractgame(const char* fname, bool doread);

	void Load() {
		Extractgame(nullptr, true);
	}

	void ResortSaveInfos();

	// Get temporary memory allocator for use while saving/loading
	// Once saving/loading is complete, all memory allocated with this allocator
	// is freed so it should only be used for temporary data
	template <typename T = char>
	auto getTempAlloc() {
		return gamedat_in_memory.get_allocator<T>();
	}

	// Delete a savegame file. This will invalidate any existing SaveInfo
	// vectors
	void DeleteSaveGame(const std::string& fname);
};
#endif    // SAVEINFO_H_INCLUDED
