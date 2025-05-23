/*
Copyright (C) 2003-2005  The Pentagram Team
Copyright (C) 2007-2025  The Exult team

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

#ifndef LOWLEVELMIDIDRIVER_H_INCLUDED
#define LOWLEVELMIDIDRIVER_H_INCLUDED

#include "MidiDriver.h"
#include "XMidiSequenceHandler.h"
#include "common_types.h"
#include "ignore_unused_variable_warning.h"

#include <atomic>
#include <condition_variable>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

class XMidiEventList;
class XMidiSequence;

//! Specifies the max number of simultaneous playing sequences supported
//! \note Only 2 simultaneous playing sequences required for Ultima 8
#define LLMD_NUM_SEQ 4

#define NEW_PRODUCESAMPLES_TIMING 1

//! An Abstract implementation of MidiDriver for Simple Low Level support of
//! Midi playback
//!
//! \note An implementation of LowLevelMidiDriver needs to implement the open(), close()
//!  and send() functions. Implementing increaseThreadPriority() is optional,
//!  however it is strongly recommended that it is implemented. If it's not
//!  implemented, the main Pentagram thread MAY use too much CPU time and cause
//!  timing problems. Similar, implemeting yield() is a good idea too.
class LowLevelMidiDriver : public MidiDriver, public XMidiSequenceHandler {
public:
	~LowLevelMidiDriver() override;

	// MidiDriver Implementation
	int  initMidiDriver(uint32 samp_rate, bool stereo) override;
	void destroyMidiDriver() override;
	int  maxSequences() override;

	void startSequence(
			int seq_num, XMidiEventList* list, bool repeat, int vol,
			int branch = -1) override;
	void   finishSequence(int seq_num) override;
	void   pauseSequence(int seq_num) override;
	void   unpauseSequence(int seq_num) override;
	void   setSequenceVolume(int seq_num, int vol) override;
	void   setSequenceSpeed(int seq_num, int speed) override;
	bool   isSequencePlaying(int seq_num) override;
	void   setSequenceRepeat(int seq_num, bool newrepeat) override;
	uint32 getSequenceCallbackData(int seq_num) override;

	uint32 getPlaybackLength(int seq_num) override {
		if (seq_num < 0 || seq_num >= LLMD_NUM_SEQ) {
			return UINT32_MAX;
		}
		// Length is in 120 Hz ticks need toconvert to ms
		return (length[seq_num] * 25) / 3;
	}

	uint32 getPlaybackPosition(int seq_num) override {
		if (seq_num < 0 || seq_num >= LLMD_NUM_SEQ) {
			return UINT32_MAX;
		}
		// Position is in 120 Hz ticks need to convert to ms
		return (position[seq_num] * 25) / 3;
	}

	void produceSamples(sint16* samples, uint32 bytes) override;

	void loadTimbreLibrary(IDataSource*, TimbreLibraryType type) override;

	static bool precacheTimbresOnStartup;
	static bool precacheTimbresOnPlay;

protected:
	LowLevelMidiDriver(std::string&& name) : MidiDriver(std::move(name)) {}

	// Will be wanted by software drivers
	uint32 sample_rate;
	bool   stereo;

	//! Open the Midi Device
	//! \return 0 on sucess. Non zero on failure.
	virtual int open() = 0;

	//! Close the Midi Device
	virtual void close() = 0;

	//! Send a message to the Midi Device
	virtual void send(uint32 message) = 0;

	//! Send a SysEX message to the Midi Device
	//
	// Note that this is slightly different to the API used in ScummVM.
	// The 0xF0 status isn't assumed, and the final 0xF7 also isn't assumed, and
	// is the final byte of the msg buffer. length includes the final byte. The
	// reason for the differences is because the midi specifications can have
	// SysEx messages that does start with 0xF0 and don't end with 0xF7. Chances
	// are though they will never be encountered.
	virtual void send_sysex(uint8 status, const uint8* msg, uint16 length) {
		ignore_unused_variable_warning(status, msg, length);
	}

	//! Increate the Thread Priority of the Play (current) thread
	virtual void increaseThreadPriority() {}

	//! Allows LowLevelMidiDrivers to produce samples
	virtual void lowLevelProduceSamples(sint16* samples, uint32 num_samples) {
		ignore_unused_variable_warning(samples, num_samples);
	}

	//! Yield execution of the current thread
	void yield() {
		std::this_thread::yield();
	}

private:
	enum Messages {
		LLMD_MSG_PLAY              = 1,
		LLMD_MSG_FINISH            = 2,
		LLMD_MSG_PAUSE             = 3,
		LLMD_MSG_SET_VOLUME        = 4,
		LLMD_MSG_SET_SPEED         = 5,
		LLMD_MSG_PRECACHE_TIMBRES  = 6,
		LLMD_MSG_SET_REPEAT        = 7,
		LLMD_MSG_SET_GLOBAL_VOLUME = 8,
		// These are only used by thread
		LLMD_MSG_THREAD_INIT        = -1,
		LLMD_MSG_THREAD_INIT_FAILED = -2,
		LLMD_MSG_THREAD_EXIT        = -3
	};

	struct ComMessage {
		ComMessage(const ComMessage& other) {
			type     = other.type;
			sequence = other.sequence;
			std::memcpy(&data, &other.data, sizeof(data));
		}

		ComMessage& operator=(const ComMessage& other) {
			type     = other.type;
			sequence = other.sequence;
			std::memcpy(&data, &other.data, sizeof(data));
			return *this;
		}

		Messages type;
		int      sequence;

		union {
			struct {
				XMidiEventList* list;
				bool            repeat;
				int             volume;
				int             branch;
			} play;

			struct {
				bool paused;
			} pause;

			struct {
				int level;
			} volume;

			struct {
				int percentage;
			} speed;

			struct {
				int code;
			} init_failed;

			struct {
				XMidiEventList* list;
			} precache;

			struct {
				bool newrepeat;
			} set_repeat;
		} data;

		ComMessage(
				Messages T, int seq,
				std::function<void(decltype(data)&)> data_setter
				= std::function<void(decltype(data)&)>())
				: type(T), sequence(seq) {
			std::memset(&data, 0, sizeof(data));
			if (data_setter) {
				data_setter(data);
			}
		}
	};

	bool uploading_timbres;    // Set in 'uploading' timbres mode

	// Communications
	std::queue<ComMessage>                       messages;
	std::unique_ptr<std::recursive_mutex>        mutex;
	std::unique_ptr<std::condition_variable_any> cond;
	sint32                                       peekComMessageType();
	void sendComMessage(const ComMessage& message);
	void waitTillNoComMessages();

	// State Readable by main game thread
	std::atomic_bool     playing[LLMD_NUM_SEQ];          // Only set by thread
	std::atomic_int32_t  callback_data[LLMD_NUM_SEQ];    // Only set by thread
	uint32               length[LLMD_NUM_SEQ];           // Not used by thread
	std::atomic_uint32_t position[LLMD_NUM_SEQ];

	// anyone can use our lock if needed
public:
	std::unique_lock<std::recursive_mutex> LockMutex(bool trylock = false) {
		// create mutex if it doesn't yet exist.
		// Shouldn't happen
		// No one should be calling this before initialization of a midi driver
		// Could in theory lead to a race condition but that would require
		// someone to call this from 2 different threads This cannot lead to a
		// race condition with the midi playback thread as the mutex will
		// already exist

		if (!mutex) {
			mutex = std::make_unique<std::recursive_mutex>();
		}
		if (trylock) {
			return std::unique_lock(*mutex, std::try_to_lock);
		}

		return std::unique_lock(*mutex);
	}

private:
	// Shared Data
	std::atomic_uchar global_volume = 100;

	// Xmidi clock 1/6000th second granuality. Will overflow after 198 hours,
	// good luck to anyone who plays Exult for 8 straight days continuously An
	// overflow here may stop playback of any tracks currently playing when the
	// oveflow occurs, but new tracks should play normally
	std::chrono::duration<uint32, std::ratio<1, 6000>> xmidi_clock
			= decltype(xmidi_clock)(0);
	// Any current delay we must observe before starting a new track
	decltype(xmidi_clock) start_track_delay_until = decltype(xmidi_clock)(0);
	// The delay to use between stopping and starting tracks. 1500 ticks is
	// 1/4sec
	const decltype(xmidi_clock) after_stop_delay = decltype(xmidi_clock)(1500);

	// Which seq a chan has been locked by
	int chan_locks[16];
	// Maps from locked logical chan to phyiscal
	int            chan_map[LLMD_NUM_SEQ][16];
	XMidiSequence* sequences[LLMD_NUM_SEQ];
	// Time we can next send sysex at
	std::chrono::milliseconds next_sysex;

	// Time lastevent was sent at
	std::chrono::steady_clock::time_point time_last_send
			= std::chrono::steady_clock::time_point::min();

	// If no events were sent in the last threshold time, sample production
	//    will be skipped
	std::chrono::milliseconds no_produce_threshold
			= std::chrono::milliseconds(2000);
	// Software Synth only Data
	uint32 total_seconds;          // xmidi_clock = total_seconds*6000
	uint32 samples_this_second;    //		+
								   // samples_this_second*6000/sample_rate;
#if !NEW_PRODUCESAMPLES_TIMING
	uint32 samples_per_iteration;
#endif

	// Thread Based Only Data
	std::unique_ptr<std::thread> thread;

	// Timbre Banks
	struct MT32Timbre {
		uint32 time_uploaded;
		int    index;
		bool   protect;
		uint8  timbre[246];
	};

	struct MT32Patch {
		sint8 timbre_bank;      // 0-3	(group A, group B, Memory, Rhythm)
		sint8 timbre_num;       // 0-63
		uint8 key_shift;        // 0-48
		uint8 fine_tune;        // 0-100 (-50 - +50)
		uint8 bender_range;     // 0-24
		uint8 assign_mode;      // 0-3 (POLY1, POLY2, POLY3, POLY4)
		uint8 reverb_switch;    // 0-1 (off,on)
		uint8 dummy;
	};

	static const MT32Patch mt32_patch_template;

	struct MT32Rhythm {
		uint8 timbre;           // 0-94 (M1-M64,R1-30,OFF)
		uint8 output_level;     // 0-100
		uint8 panpot;           // 0-14 (L-R)
		uint8 reverb_switch;    // 0-1 (off,on)
	};

	struct MT32RhythmSpec {
		int        note;
		MT32Rhythm rhythm;
	};

	MT32Patch**  mt32_patch_banks[128];     // 128 banks, of 128 Patches
	MT32Timbre** mt32_timbre_banks[128];    // 128 banks, of 128 Timbres
	MT32Rhythm*  mt32_rhythm_bank[128];     // 1 bank of rhythm
	int          mt32_timbre_used[64][2];
	int          mt32_bank_sel[LLMD_NUM_SEQ][16];
	int          mt32_patch_bank_sel[128];

	void loadXMidiTimbreLibrary(IDataSource* ds);
	void extractTimbreLibrary(XMidiEventList* eventlist);
	void uploadTimbre(int bank, int patch);
	void setPatchBank(int bank, int patch);
	void loadRhythm(const MT32Rhythm& rhythm, int note);
	void loadRhythmTemp(int temp);
	void sendMT32SystemMessage(
			uint32 address_base, uint16 address_offset, uint32 len,
			const void* data);

	// Shared Methods

	enum class PlaySeqResult {
		terminate = 0,
		normal    = 1,
		delayreq  = 2,
	};
	//! Play all sequences, handle communications requests
	//! /return one of PlaySeqResult
	PlaySeqResult playSequences();

	// Thread Methods
	int        initThreadedSynth();
	void       destroyThreadedSynth();
	static int threadMain_Static(std::shared_ptr<LowLevelMidiDriver> data);
	int        threadMain();
	// Thread flag -- set to true when ready to quit
	std::atomic_bool quit_thread;

	// Software methods
	int  initSoftwareSynth();
	void destroySoftwareSynth();

	// XMidiSequenceHandler implementation
	void sequenceSendEvent(uint16 sequence_id, uint32 message) override;
	void sequenceSendSysEx(
			uint16 sequence_id, uint8 status, const uint8* msg,
			uint16 length) override;
	uint32 getTickCount(uint16 sequence_id) override;
	void   handleCallbackTrigger(uint16 sequence_id, uint8 data) override;

	int protectChannel(uint16 sequence_id, int chan, bool protect);
	int lockChannel(uint16 sequence_id, int chan, bool lock);

	int unlockAndUnprotectChannel(uint16 sequence_id);

	//! Set global volume of this driver
	//! \param vol The new volume level for the sequence (0-100)
	void setGlobalVolume(int vol) override {
		sendComMessage(ComMessage(
				LLMD_MSG_SET_GLOBAL_VOLUME, -1, [vol](auto& data) noexcept {
					data.volume.level = vol;
				}));
	}

	//! Get global volume of this driver
	//! \returns Current Volume [0-100]
	int getGlobalVolume() override {
		return global_volume;
	}
};

#endif    // LOWLEVELMIDIDRIVER_H_INCLUDED
