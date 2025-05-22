#include "AudioChannelMT.h"

#include <iostream>

namespace Pentagram {

	AudioChannelMT::AudioChannelMT(
			uint32 sample_rate_, bool stereo_, size_t Byte_limit)
			: AudioChannel(sample_rate_, stereo_),
			  max_bytes_to_allocate(Byte_limit),
			  mutex(std::make_unique<std::mutex>()),
			  thread_signal(std::make_unique<std::condition_variable>()),
			  thread_state(ThreadState::Waiting)

	{}

	std::ostream& operator<<(
			std::ostream& os, AudioChannelMT::ThreadState state) {
		if (state == AudioChannelMT::ThreadState::Running) {
			return os << "Running";
		}
		if (state == AudioChannelMT::ThreadState::Exiting) {
			return os << "Exiting";
		}
		if (state == AudioChannelMT::ThreadState::Waiting) {
			return os << "Waiting";
		}
		return os << (int)state;
	}

	std::ostream& operator<<(
			std::ostream& os, AudioChannelMT::ThreadCommand cmd) {
		if (cmd == AudioChannelMT::ThreadCommand::Exit) {
			return os << "Exit";
		}
		if (cmd == AudioChannelMT::ThreadCommand::Start) {
			return os << "Start";
		}
		if (cmd == AudioChannelMT::ThreadCommand::Restart) {
			return os << "Restart";
		}
		if (cmd == AudioChannelMT::ThreadCommand::Stop) {
			return os << "Stop";
		}
		if (cmd == AudioChannelMT::ThreadCommand::None) {
			return os << "None";
		}

		return os << (int)cmd;
	}

	AudioChannelMT::~AudioChannelMT() {
		ExitThread();
	}

	void AudioChannelMT::ExitThread() {
		stop();
		if (thread) {
			COUT("AudioChannelMT::ExitThread sending Exit");
			SendCommand(ThreadCommand::Exit);
			WaitForThreadState(ThreadState::Exiting);
			thread->join();
			thread.reset();
		}
	}

	void AudioChannelMT::SendCommand(ThreadCommand cmd) {
		{
			std::cout << "AudioChannelMT::SendCommand " << cmd << std::endl;
			auto l = lock();
			std::cout << "AudioChannelMT::SendCommand pushed" << std::endl;
			commands.emplace(cmd);
		}
		std::cout << "AudioChannelMT::SendCommand notify" << std::endl;
		thread_signal->notify_one();
		std::cout << "AudioChannelMT::SendCommand notified" << std::endl;
	}

	void AudioChannelMT::WaitForThreadState(ThreadState check) {
		// Wait till thread is waiting
		if (thread) {
			for (;;) {
				auto l = lock();
				if (!l) {
					continue;
				}
				if (thread_state == check) {
					break;
				}
				l.unlock();
				std::this_thread::yield();
			}
		}
	}

	void AudioChannelMT::ThreadFunc_static(AudioChannelMT* self) {
		self->ThreadFunc();
	}

	void AudioChannelMT::ThreadFunc() {
		bool decompressing   = false;
		thread_state         = ThreadState::Waiting;
		size_t cur_framesize = 0;
		// sample->getFrameSize();
		size_t bytes_unallocated = max_bytes_to_allocate;
		frames_free.clear();
		frames_ready.clear();

		while (1) {
			std::unique_ptr<uint8[]> frame;
			uint32                   framesize;
			{
				auto l = lock();

				// If no commands and no frames to decompress, wait till
				// signalled

				if (!commands.size()
					&& !((frames_free.size()
						  || bytes_unallocated >= cur_framesize)
						 && (decompressing))) {
					std::cout << "want to wait cmds " << commands.size()
							  << " frames " << frames_free.size()
							  << " bytes_unallocated " << bytes_unallocated
							  << " decompressing " << decompressing
							  << std::endl;
					auto res = thread_signal->wait_for(
							l, std::chrono::milliseconds(100));

					std::cout
							<< "thread signal "
							<< (res == std::cv_status::no_timeout ? "" : "not ")
							<< "recieved. command "

							<< (commands.size() ? commands.back()
												: ThreadCommand::None)
							<< " bytes available "
							<< bytes_unallocated
									   + frames_free.size() * cur_framesize
							<< " current state " << thread_state << std::endl;
				} else {
					std::cout << "skipping waiting for thread signal. command: "
							  << (commands.size() ? commands.back()
												  : ThreadCommand::None)
							  << " bytes available "
							  << bytes_unallocated
										 + frames_free.size() * cur_framesize
							  << " current state " << thread_state << std::endl;
				}

				while (commands.size()) {
					auto front = commands.front();
					// If back is exit, immediately respond to that instead
					if (commands.back() == ThreadCommand::Exit) {
						front = commands.back();
					}
					commands.pop();
					std::cout << "AudioChannelMT::ThreadFunc cmd " << front
							  << std::endl;
					switch (front) {
					case ThreadCommand::Stop:
						COUT("AudioChannelMT thread recieved stop message ");

						thread_state  = ThreadState::Waiting;
						decompressing = false;
						break;
					case ThreadCommand::Start: {
						COUT("AudioChannelMT thread recieved start message ");
						if (!sample || !decomp) {
							thread_state = ThreadState::Waiting;
							continue;
						}
						// if the new framesize is larger than the old
						// discard all frames
						auto new_framesize = sample->getFrameSize();
						if (new_framesize < cur_framesize) {
							frames_free.clear();
							frames_ready.clear();
							bytes_unallocated = max_bytes_to_allocate;
						}
						cur_framesize = new_framesize;

						// Move all frames to free
						while (frames_ready.size()) {
							auto& back = frames_ready.back();
							if (back.second) {
								frames_free.push_back(std::move(back.second));
							}
							frames_ready.pop_back();
						}

						// Fill the free list till it has at least 20 frames
						while (0 && frames_free.size() < 20) {
							// Decrement bytes_unallocated by framesize
							if (bytes_unallocated < cur_framesize) {
								bytes_unallocated = 0;
							} else {
								bytes_unallocated -= cur_framesize;
							}
							frames_free.push_back(
									std::make_unique<uint8[]>(cur_framesize));
						}
					}
						[[fallthrough]];
					case ThreadCommand::Restart:
						if (!sample || !decomp) {
							thread_state = ThreadState::Waiting;
							continue;
						}

						COUT("AudioChannelMT thread doing restart ");
						sample->rewind(decomp);
						thread_state  = ThreadState::Running;
						decompressing = true;
						break;
					case ThreadCommand::Exit:
						COUT("AudioChannelMT thread Exiting ");
						thread_state = ThreadState::Exiting;
						return;
						break;
					}
				}
				if (!decompressing) {
					continue;
				}
				// Get a freeframe
				if (frames_free.size()) {
					frame.swap(frames_free.front());
					frames_free.pop_front();
				} else if (bytes_unallocated >= cur_framesize) {
					// Allocate a new frame
					frame = std::make_unique<uint8[]>(cur_framesize);
					bytes_unallocated -= cur_framesize;
				} else {
					// No free frames and we'd exceed our limit if we try to
					// allocate more So continue loop until there is a free
					// frame
					COUT("AudioChannelMT thread has no free frames for "
						 "decompression ");
					continue;
				}
			}

			// Decompress a frame
			framesize = sample->decompressFrame(decomp, frame.get());

			if (framesize == 0) {
				COUT("AudioChannelMT thread reached end of stream ");
				// push an empty frame
				{
					auto l = lock();
					frames_ready.push_back({});
				}
				// if we should be looping
				// automatically restart
				if (loop) {
					COUT("AudioChannelMT thread restarting decompression "
						 "because channel is looping ");

					sample->rewind(decomp);
					// frame1_size = sample->decompressFrame(decomp,
					// frame.get());

				} else {
					thread_state  = ThreadState::Waiting;
					decompressing = false;
					continue;
				}
			} else {
				// push the frame
				auto l = lock();
				frames_ready.push_back({framesize, std::move(frame)});
			}
		}
	}

	void AudioChannelMT::DecompressFirstFrame() {
		// Start thread decompressor
		if (thread_state != ThreadState::Running) {
			COUT("AudioChannelMT::DecompressFirstFrame sending start, state "
				 "was "
				 << thread_state);
			SendCommand(ThreadCommand::Start);
			WaitForThreadState(ThreadState::Running);
		}
		AudioChannel::DecompressFirstFrame();
	}

	void AudioChannelMT::DecompressNextFrame() {
		// if want looping and the thread is not running restart it
		if (loop && thread_state != ThreadState::Running) {
			COUT("AudioChannelMT::DecompressNextFrame sending Restart, state "
				 "was "
				 << thread_state);
			SendCommand(ThreadCommand::Restart);
			WaitForThreadState(ThreadState::Running);
		}
		AudioChannel::DecompressNextFrame();
	}

	uint32 AudioChannelMT::DecompressFrame(int dest) {
		// wait till there is a frame available and pop it
		std::unique_ptr<uint8[]> frame;
		uint32                   framesize;
		int                      message_count = 0;
		for (;;) {
			std::this_thread::yield();
			auto l = lock();

			if (l.owns_lock()) {
				if (frames_ready.size()) {
					auto& front = frames_ready.front();
					frame.swap(front.second);
					framesize = front.first;
					frames_ready.pop_front();
					break;
				} else if (up_frames[dest]) {
					frames_free.push_back(std::move(up_frames[dest]));
					thread_signal->notify_one();
				}
			}
			if (message_count++ == 0) {
				COUT("AudioChannelMT::DecompressFrame waiting for frame ");
			}
		}
		// exchange the existing frame unique pointer with the incoming frame
		// and update the raw pointer
		up_frames[dest].swap(frame);
		frames[dest] = up_frames[dest].get();

		// Send the old frame to the free list
		if (frame) {
			auto l = lock();
			if (l.owns_lock()) {
				frames_free.push_back(std::move(frame));
			}
		}
		thread_signal->notify_one();
		return framesize;
	}

	void AudioChannelMT::rewind() {
		// If rewind is requested issue a start
		if (loop && thread_state != ThreadState::Running) {
			COUT("AudioChannelMT::rewind sending start, state was "
				 << thread_state);
			sample->rewind(decomp);
			SendCommand(ThreadCommand::Start);
		}
		overall_position = 0;
	}

	void AudioChannelMT::playSample(
			AudioSample* sample, int loop, int priority, bool paused,
			uint32 pitch_shift, int lvol, int rvol, sint32 instance_id) {
		// Move frame unique pointers back to free list
		if (up_frames[0]) {
			frames_free.push_back(std::move(up_frames[0]));
		}
		if (up_frames[0]) {
			frames_free.push_back(std::move(up_frames[1]));
		}
		if (!thread) {
			thread = std::make_unique<std::thread>(
					AudioChannelMT::ThreadFunc_static, this);

			// Stop any existing decompressor thread
		} else if (thread_state != ThreadState::Waiting) {
			SendCommand(ThreadCommand::Stop);
			COUT("AudioChannelMT::playSample sending stop, state was "
				 << thread_state);
			// Wait till thread is waiting
			WaitForThreadState(ThreadState::Waiting);
		}

		// Clear frame pointers just to be sure
		frames[0] = nullptr;
		frames[1] = nullptr;

		AudioChannel::playSample(
				sample, loop, priority, paused, pitch_shift, lvol, rvol,
				instance_id);
	}

	void AudioChannelMT::stop() {
		if (thread) {
			COUT("stop sending stop, state was " << thread_state);
			SendCommand(ThreadCommand::Stop);
			WaitForThreadState(ThreadState::Waiting);
		}
		AudioChannel::stop();
	}
}    // namespace Pentagram