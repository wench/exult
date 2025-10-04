#pragma once
#include "AudioChannel.h"

#include <mutex>
#include <thread>
#include <condition_variable>
#include <queue>
#include <list>
#include <chrono>
#include <utility>
#include <memory>
		namespace Pentagram {

	// MultiThreaded Audio Channel
	// Pre Decompresses Frames on a separate thread
	// The decode thread takes free frames from frames_free, fills them then adds to frames_ready
	// THe mixer thread take decoded frames from frames_ready, and once used adds them  to frames_free for reuse
	// The decode thread will add an empty frame to frames_ready when it reaches end of stream and then restart decoding if the channel is looping
	class AudioChannelMT : public AudioChannel {
		// The maximum frames allowed to be allocated
		size_t max_bytes_to_allocate;		
		// Decoded frames waiting for mixing
		std::list<std::pair<uint32, std::unique_ptr<uint8[]>>> frames_ready;
		// Free frames waiting tio be used
		std::list<std::unique_ptr<uint8[]>> frames_free;
		
		// Unique pointers for the current frames pointers in AudioChannel::frames
		std::unique_ptr<uint8[]> up_frames[2];

		std::unique_ptr<std::thread> thread;

		// Mutex for reading and writing to frame stacks and Commands
		std::unique_ptr < std::mutex> mutex;

		// Condition Variable for decode thread to wait on till it has something to do
		// signalled when Mixer thread returns a frame and when command are sent to the decode thread
		std::unique_ptr <std::condition_variable> thread_signal;

		public:
		enum class ThreadCommand
		{
			None,
			// Stop decompressing
			Stop,

			// Set all frames clear and Start decompressing from the Beginning
			Start,

			// Rewind the decompressor and start decompressing from the Beginning
			// Leaving existing decompressed frames as is
			Restart,


			// Immediately exit the thread
			Exit
			
		};


		enum class ThreadState
		{
			// Set when the thread is idling as it has nothing to decompress
			// While waiting decomp can be modified
			Waiting,

			// While running thread is decompressing. decomp must not be changed while in this state
			Running,

			//Thread has recieved an Exit command and is trying to exit
			Exiting,
		};

	protected:
		std::queue<ThreadCommand> commands;
		std::atomic<ThreadState> thread_state;


		std::unique_lock<std::mutex> lock() {
			return std::unique_lock<std::mutex>(*mutex);
		}

		void SendCommand(ThreadCommand cmd);


		void WaitForThreadState(ThreadState check);

		static void ThreadFunc_static(AudioChannelMT* self);
		void        ThreadFunc();

public:
		AudioChannelMT(
				uint32 sample_rate_, bool stereo_,
				size_t byte_limit = 10 << 20);

		virtual ~AudioChannelMT();
		AudioChannelMT(const AudioChannelMT&)            = delete;
		AudioChannelMT(AudioChannelMT&&)                 = delete;
		AudioChannelMT& operator=(const AudioChannelMT&) = delete;
		AudioChannelMT& operator=(AudioChannelMT&&)      = delete;

		void ExitThread();

		bool isMT() const override {
			return true;
		}

		void DecompressFirstFrame() override;
		void DecompressNextFrame() override;
		uint32 DecompressFrame(int dest) override;

		void rewind() override;

		bool looping_needs_rewind() const override{
			return false;
		} 

		void playSample(
				AudioSample* sample, int loop, int priority, bool paused,
				uint32 pitch_shift, int lvol, int rvol, sint32 instance_id) override;

		void stop() override;
	};

}    // namespace Pentagram
