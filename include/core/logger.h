/*
 * @Author       : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @Date         : 2022-12-07 15:42:18
 * @LastEditors  : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @LastEditTime : 2022-12-15 21:58:53
 * @FilePath     : \rollback_netcode\include\core\logger.h
 * @Description  :
 * Copyright (c) 2022 by Daylily-Zeleen email: daylily-zeleen@foxmail.com, All Rights Reserved.
 */
#pragma once

#include <def.h>
#include <deque>
#include <fstream>
#include <mutex>
#include <semaphore>

#include "interfaces/i_match_info.h"
#include "interfaces/i_peer.h"
#include "input_buffer_frame.h"
#include "state_buffer_frame.h"

namespace rollback_netcode {

class Semaphore {
public:
	explicit Semaphore(int count = 0) :
			count_(count) {}

	void post() {
		std::unique_lock<std::mutex> lock(mutex_);
		++count_;
		cv_.notify_one();
	}

	void wait() {
		std::unique_lock<std::mutex> lock(mutex_);
		cv_.wait(lock, [=] { return count_ > 0; });
		--count_;
	}

private:
	std::mutex mutex_;
	std::condition_variable cv_;
	int count_;
};

class Logger {
public:
	enum LogType {
		HEADER,
		FRAME,
		STATE,
		INPUT,
	};
	enum FrameType {
		INTERFRAME,
		TICK,
		INTERPOLATION_FRAME,
	};
	enum SkipReason {
		ADVANTAGE_ADJUSTMENT,
		INPUT_BUFFER_UNDERRUN,
		WAITING_TO_REGAIN_SYNC,
	};

	bool start(const std::string &p_log_file_path, const PeerId_t &p_network_peer_id, const SharedPtr<IMatchInfo> &p_match_info);
	void stop();
	void write_state(const StateBufferFrame &p_state_buffer_frame);
	void write_input(const InputBufferFrame &p_input_buffer_frame);

	void begin_interframe();
	void end_interframe();
	void begin_tick(const Tick_t p_tick);
	void end_tick(const int64_t p_start_ticks_usecs);
	void skip_tick(const SkipReason p_skip_reson, const uint64_t &p_start_ticks_usecs);
	void begin_interpolation_frame(const Tick_t p_tick);
	void end_interpolation_frame(const uint64_t &p_start_ticks_usecs);
	void log_fatal_error(const std::string &p_error_msg);

	void add_value(const std::string &p_key, const std::string &p_value);
	void merge_array_value(const std::string &p_key, const jsonxx::Array &p_arr);

	void increment_value(const std::string &p_key, const uint64_t p_amount = 1);
	void start_timing(const std::string &p_timer);
	void stop_timing(const std::string &p_timer, const bool p_accumulate = false);
	void add_timing(const std::string &p_timer, const float p_msecs, const bool p_accumulate = false);

	void add_message(const std::string &p_msg_key, const std::string &p_msg);
	void add_peer_statues(const SharedPtr<IPeer> &p_peer);

	Logger();

private:
	std::thread _writer_thread;
	std::mutex _writer_thread_mutex;
	Semaphore _writer_thread_semaphore;
	std::deque<std::string> _write_queue;
	std::fstream _log_file;
	bool _started = false;

	jsonxx::Object data;
	jsonxx::Object _start_times;

	void _writer_thread_function();
	void write_current_data();
};
} // namespace rollback_netcode