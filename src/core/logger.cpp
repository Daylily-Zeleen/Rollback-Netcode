#include "utils.h"
#include <core/logger.h>

namespace rollback_netcode {

Logger::Logger() = default;

bool Logger::start(const std::string &p_log_file_path, const PeerId_t &p_peer_id, const SharedPtr<IMatchInfo> &p_match_info) {
	if (!_started) {
		_log_file.open(p_log_file_path, std::ios::out);

		JsonObj header_json;
		header_json << "log_type" << LogType::HEADER;
		header_json << "peer_id" << p_peer_id;
		header_json << "match_info" << p_match_info->to_json_obj();

		std::string json_str = header_json.json();
		_log_file.write(json_str.c_str(), std::streamsize(json_str.length()));

		_started = true;
		_writer_thread = std::thread([this] { _writer_thread_function(); });
	}
	return true;
}

void Logger::stop() {
	_writer_thread_mutex.lock();
	bool is_running = _started;
	_writer_thread_mutex.unlock();

	if (is_running) {
		if (data.size() > 0) {
			write_current_data();

			_writer_thread_mutex.lock();
			_started = false;
			_writer_thread_mutex.unlock();

			_writer_thread_semaphore.post();
			_writer_thread.join();

			_log_file.close();
			_write_queue.clear();
			data.clear();
			_start_times.clear();
		}
	}
}

void Logger::_writer_thread_function() {
	while (true) {
		_writer_thread_semaphore.wait();

		std::string data_to_write;
		bool should_exit;

		_writer_thread_mutex.lock();
		if (_write_queue.size() > 0) {
			data_to_write = _write_queue.front();
			_write_queue.pop_front();
		}
		should_exit = !_started;
		_writer_thread_mutex.unlock();

		if (data_to_write.length() > 0) {
			_log_file.write(data_to_write.c_str(), std::streamsize(data_to_write.length()));
		} else if (should_exit) {
			break;
		}
	}
}
void Logger::write_current_data() {
	if (data.size() == 0)
		return;

	auto copy = data.duplicate();
	copy << "log_type" << uint8_t(LogType::FRAME);

	if (!copy.has<jsonxx::Number>("frame_type"))
		copy << "frame_type" << uint8_t(FrameType::INTERFRAME);

	_writer_thread_mutex.lock();
	_write_queue.emplace_back(copy.json());
	_writer_thread_mutex.unlock();

	_writer_thread_semaphore.post();

	data.clear();
}

void Logger::write_state(const StateBufferFrame &p_state_buffer_frame) {
	JsonObj data_to_write;
	data_to_write << "log_type" << LogType::STATE;
	data_to_write << "tick" << p_state_buffer_frame.tick;
	data_to_write << "state" << p_state_buffer_frame.to_json_obj();

	_writer_thread_mutex.lock();
	_write_queue.emplace_back(data_to_write.json());
	_writer_thread_mutex.unlock();

	_writer_thread_semaphore.post();
}

void Logger::write_input(const InputBufferFrame &p_input_buffer_frame) {
	JsonObj data_to_write;
	data_to_write << "log_type" << uint8_t(LogType::INPUT);
	data_to_write << "tick" << p_input_buffer_frame.tick;
	data_to_write << "input" << p_input_buffer_frame.to_json_obj();

	_writer_thread_mutex.lock();
	_write_queue.emplace_back(data_to_write.json());
	_writer_thread_mutex.unlock();

	_writer_thread_semaphore.post();
}

void Logger::begin_interframe() {
	if (!data.has<jsonxx::Number>("frame_type")) {
		data << "frame_type" << uint8_t(FrameType::INTERFRAME);
	}
	if (!data.has<jsonxx::Number>("start_time")) {
		data << "start_time" << get_current_system_time_msec();
	}
}

void Logger::end_interframe() {
	if (!data.has<jsonxx::Number>("frame_type")) {
		data << "frame_type" << uint8_t(FrameType::INTERFRAME);
	}
	if (!data.has<jsonxx::Number>("start_time")) {
		data << "start_time" << (get_current_system_time_msec() - 1);
	}

	data << "end_time" << get_current_system_time_msec();
	write_current_data();
}

void Logger::begin_tick(const Tick_t p_tick) {
	if (data.size() > 0)
		end_interframe();

	data << "frame_type" << uint8_t(FrameType::TICK);
	data << "tick" << p_tick;
	data << "start_time" << get_current_system_time_msec();
}

void Logger::end_tick(const int64_t p_start_ticks_usecs) {
	data << "end_time" << get_current_system_time_msec();
	data << "duration" << (float(get_current_tick_usec() - p_start_ticks_usecs) / 1000.0);
	write_current_data();
}

void Logger::skip_tick(const SkipReason p_skip_reson, const uint64_t &p_start_ticks_usecs) {
	data << "skipped" << true;
	data << "skip_reason" << p_skip_reson;
	end_tick(int64_t(p_start_ticks_usecs));
}

void Logger::begin_interpolation_frame(const Tick_t p_tick) {
	if (data.size() > 0)
		end_interframe();

	data << "frame_type" << uint8_t(FrameType::INTERPOLATION_FRAME);
	data << "tick" << p_tick;
	data << "start_time" << get_current_system_time_msec();
}
void Logger::end_interpolation_frame(const uint64_t &p_start_ticks_usecs) {
	data << "end_time" << get_current_system_time_msec();
	data << "duration" << (float(get_current_tick_usec() - p_start_ticks_usecs) / 1000.0);
	write_current_data();
}

void Logger::log_fatal_error(const std::string &p_error_msg) {
	if (!data.has<jsonxx::Number>("end_time"))
		data << "end_time" << get_current_system_time_msec();
	data << "fatal_error" << true;
	data << "fatal_error_message" << p_error_msg;
	write_current_data();
}

void Logger::add_value(const std::string &p_key, const std::string &p_value) {
	if (!data.has<jsonxx::Array>(p_key))
		data << p_key << jsonxx::Array();
	data.get<jsonxx::Array>(p_key) << p_value;
}

void Logger::merge_array_value(const std::string &p_key, const jsonxx::Array &p_arr) {
	if (!data.has<jsonxx::Array>(p_key))
		data << p_key << p_arr;
	else
		data << p_key << (data.get<jsonxx::Array>(p_key) << p_arr);
}

void Logger::increment_value(const std::string &p_key, const uint64_t p_amount) {
	if (!data.has<jsonxx::Number>(p_key))
		data << p_key << p_amount;
	else
		data << p_key << (uint64_t(data.get<jsonxx::Number>(p_key)) + p_amount);
}

void Logger::start_timing(const std::string &p_timer) {
	ERR_THROW(_start_times.has<JsonObj>(p_timer), string_sprintf("Timer already exists: %s", p_timer));
	_start_times << p_timer << get_current_tick_usec();
}

void Logger::stop_timing(const std::string &p_timer, const bool p_accumulate) {
	ERR_THROW(!_start_times.has<JsonObj>(p_timer), string_sprintf("No such ping_timer: %s", p_timer));
	if (_start_times.has<JsonObj>(p_timer)) {
		add_timing(p_timer, float(get_current_tick_usec() - _start_times.get<int64_t>(p_timer)) / 1000.0f, p_accumulate);
		_start_times.erase(p_timer);
	}
}

void Logger::add_timing(const std::string &p_timer, const float p_msecs, const bool p_accumulate) {
	if (!data.has<JsonObj>("timings"))
		data << "timings" << JsonObj();
	auto timings = data.get<JsonObj>("timings");
	if (timings.has<jsonxx::Number>(p_timer) && p_accumulate) {
		auto old_average = timings.get<jsonxx::Number>(p_timer + ".average");
		auto old_count = static_cast<uint32_t>(timings.get<jsonxx::Number>(p_timer + ".count"));

		timings << p_timer << p_msecs;
		timings << (p_timer + ".max") << std::max(float(timings.get<jsonxx::Number>(p_timer + ".max")), p_msecs);
		timings << (p_timer + ".average") << ((old_average * old_count) + p_msecs) / (old_count + 1);
		timings << (p_timer + ".count") << (old_count + 1);
	} else {
		timings << p_timer << p_msecs;
		if (p_accumulate) {
			timings << (p_timer + ".max") << p_msecs;
			timings << (p_timer + ".average") << 0.0;
			timings << p_timer + ".count" << 1;
		}
	}
}
void Logger::add_message(const std::string &p_msg_key, const std::string &p_msg) { data << p_msg_key << p_msg; }
void Logger::add_peer_statues(const SharedPtr<IPeer> &p_peer) {
	JsonObj statuse;
	statuse << "local_lag" << p_peer->get_local_lag();
	statuse << "remote_lag" << p_peer->get_remote_lag();
	statuse << "advantege" << p_peer->get_local_lag() - p_peer->get_remote_lag();
	statuse << "calculated_advantage" << p_peer->get_calculated_advantage();
	add_value(string_sprintf("peer_%d", p_peer->get_peer_id()), statuse.json());
}

} // namespace rollback_netcode