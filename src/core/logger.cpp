#include "core/serialized_data.h"
#include "def.h"
#include <core/logger.h>
#include <interfaces/i_sync_manager.h>
#include <numeric>
#include <stdint.h>
#include <utility>
namespace rollback_netcode {

Logger::Logger() = default;

bool Logger::start(const std::string &p_log_file_path, const PeerId_t &p_peer_id, const SharedPtr<IData> &p_match_info) {
    if (!_started) {
        _log_file.open(p_log_file_path, std::ios::out);

        auto data_adaptor = ISyncManager::get_singleton()->data_adaptor;
        ERR_THROW(data_adaptor == nullptr, "Logger function should be used after setting data_adaptor for RollbackNetworkSyncManager.");

        SerializedData match_info_data;
        HeaderLog header_log(p_peer_id, data_adaptor->serialize_match_info(p_match_info));

        auto data = header_log.serialize();

        _log_file.write(data.buffer, data.size);

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

        SerializedData data_to_write;
        bool should_exit;

        _writer_thread_mutex.lock();
        if (_write_queue.size() > 0) {
            data_to_write = _write_queue.front();
            _write_queue.pop_front();
        }
        should_exit = !_started;
        _writer_thread_mutex.unlock();

        if (data_to_write.size > 0) {
            _log_file.write(data_to_write.buffer, data_to_write.size);
        } else if (should_exit) {
            break;
        }
    }
}

void Logger::write_state(const StateBufferFrame &p_state_buffer_frame) {
    auto data_adaptor = ISyncManager::get_singleton()->data_adaptor;
    ERR_THROW(data_adaptor == nullptr, "Logger function should be used after setting data_adaptor for RollbackNetworkSyncManager.");

    std::map<UUID_t, SerializedData> state_data;
    for (auto &kv : p_state_buffer_frame.data) {
        state_data.emplace(kv.first, data_adaptor->serialize_state(kv.second));
    }

    StateLog state_log(p_state_buffer_frame.tick, state_data);

    _writer_thread_mutex.lock();
    _write_queue.emplace_back(state_log.serialize());
    _writer_thread_mutex.unlock();

    _writer_thread_semaphore.post();
}

void Logger::write_input(const InputBufferFrame &p_input_buffer_frame) {
    auto data_adaptor = ISyncManager::get_singleton()->data_adaptor;
    ERR_THROW(data_adaptor == nullptr, "Logger function should be used after setting data_adaptor for RollbackNetworkSyncManager.");

    std::map<PeerId_t, InputLog::InputData> input_data;
    for (auto &kv : p_input_buffer_frame.peers_input) {
        input_data.emplace(kv.first, InputLog::InputData(data_adaptor->serialize_input(kv.second.input), kv.second.predicted));
    }

    InputLog input_log(p_input_buffer_frame.tick, input_data);

    _writer_thread_mutex.lock();
    _write_queue.emplace_back(input_log.serialize());
    _writer_thread_mutex.unlock();

    _writer_thread_semaphore.post();
}

void Logger::write_current_data() {
    if (data.size() == 0)
        return;

    auto copy = data.duplicate();

    if (!copy.has<jsonxx::Number>("frame_type"))
        copy << std::string("frame_type") << uint8_t(FrameLog::FrameType::INTERFRAME);

    _writer_thread_mutex.lock();
    _write_queue.emplace_back(FrameLog(copy.json()).serialize());
    _writer_thread_mutex.unlock();

    _writer_thread_semaphore.post();

    data.clear();
}

void Logger::begin_interframe() {
    if (!data.has<jsonxx::Number>("frame_type")) {
        data << std::string("frame_type") << uint8_t(FrameLog::FrameType::INTERFRAME);
    }
    if (!data.has<jsonxx::Number>("start_time")) {
        data << std::string("start_time") << Utils::get_current_system_time_msec();
    }
}

void Logger::end_interframe() {
    if (!data.has<jsonxx::Number>("frame_type")) {
        data << std::string("frame_type") << uint8_t(FrameLog::FrameType::INTERFRAME);
    }
    if (!data.has<jsonxx::Number>("start_time")) {
        data << std::string("start_time") << (Utils::get_current_system_time_msec() - 1);
    }

    data << std::string("end_time") << Utils::get_current_system_time_msec();
    write_current_data();
}

void Logger::begin_tick(const Tick_t p_tick) {
    if (data.size() > 0)
        end_interframe();

    data << std::string("frame_type") << uint8_t(FrameLog::FrameType::TICK);
    data << std::string("tick") << p_tick;
    data << std::string("start_time") << Utils::get_current_system_time_msec();
}

void Logger::end_tick(const int64_t p_start_ticks_usecs) {
    data << std::string("end_time") << Utils::get_current_system_time_msec();
    data << std::string("duration") << (float(Utils::get_current_tick_usec() - p_start_ticks_usecs) / 1000.0);
    write_current_data();
}

void Logger::skip_tick(const SkipReason p_skip_reson, const uint64_t &p_start_ticks_usecs) {
    data << std::string("skipped") << true;
    data << std::string("skip_reason") << p_skip_reson;
    end_tick(int64_t(p_start_ticks_usecs));
}

void Logger::begin_interpolation_frame(const Tick_t p_tick) {
    if (data.size() > 0)
        end_interframe();

    data << std::string("frame_type") << uint8_t(FrameLog::FrameType::INTERPOLATION_FRAME);
    data << std::string("tick") << p_tick;
    data << std::string("start_time") << Utils::get_current_system_time_msec();
}
void Logger::end_interpolation_frame(const uint64_t &p_start_ticks_usecs) {
    data << std::string("end_time") << Utils::get_current_system_time_msec();
    data << std::string("duration") << (float(Utils::get_current_tick_usec() - p_start_ticks_usecs) / 1000.0);
    write_current_data();
}

void Logger::log_fatal_error(const std::string &p_error_msg) {
    if (!data.has<jsonxx::Number>("end_time"))
        data << std::string("end_time") << Utils::get_current_system_time_msec();
    data << std::string("fatal_error") << true;
    data << std::string("fatal_error_message") << p_error_msg;
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
    if (!data.has(p_key))
        data << p_key << p_amount;
    else
        data << p_key << (uint32_t(data.get<jsonxx::Number>(p_key)) + p_amount);
}

void Logger::start_timing(const std::string &p_timer) {
    ERR_THROW(_start_times.has<JsonObj>(p_timer), std::format("Timer already exists: {}", p_timer));
    _start_times << p_timer << Utils::get_current_tick_usec();
}

void Logger::stop_timing(const std::string &p_timer, const bool p_accumulate) {
    ERR_THROW(!_start_times.has<JsonObj>(p_timer), std::format("No such ping_timer: {}", p_timer));
    if (_start_times.has<JsonObj>(p_timer)) {
        add_timing(p_timer, float(Utils::get_current_tick_usec() - static_cast<int64_t>(_start_times.get<jsonxx::Number>(p_timer))) / 1000.0f, p_accumulate);
        _start_times.erase(p_timer);
    }
}

void Logger::add_timing(const std::string &p_timer, const float p_msecs, const bool p_accumulate) {
    if (!data.has<JsonObj>("timings"))
        data << std::string("timings") << JsonObj();
    auto timings = data.get<JsonObj>("timings");
    if (timings.has<jsonxx::Number>(p_timer) && p_accumulate) {
        auto old_average = timings.get<jsonxx::Number>(p_timer + ".average");
        auto old_count = static_cast<uint32_t>(timings.get<jsonxx::Number>(p_timer + ".count"));

        timings << p_timer << static_cast<JsonNumber>(p_msecs);
        timings << (p_timer + ".max") << static_cast<JsonNumber>(std::max(float(timings.get<jsonxx::Number>(p_timer + ".max")), p_msecs));
        timings << (p_timer + ".average") << static_cast<JsonNumber>(((old_average * old_count) + p_msecs) / (old_count + 1));
        timings << (p_timer + ".count") << static_cast<JsonNumber>(old_count + 1);
    } else {
        timings << p_timer << static_cast<JsonNumber>(p_msecs);
        if (p_accumulate) {
            timings << (p_timer + ".max") << static_cast<JsonNumber>(p_msecs);
            timings << (p_timer + ".average") << static_cast<JsonNumber>(0.0);
            timings << (p_timer + ".count") << static_cast<JsonNumber>(1);
        }
    }
}
void Logger::add_message(const std::string &p_msg_key, const std::string &p_msg) { data << p_msg_key << p_msg; }
void Logger::add_peer_statues(const SharedPtr<IPeer> &p_peer) {
    JsonObj statuse;
    statuse << std::string("local_lag") << p_peer->get_local_lag();
    statuse << std::string("remote_lag") << p_peer->get_remote_lag();
    statuse << std::string("advantege") << p_peer->get_local_lag() - p_peer->get_remote_lag();
    statuse << std::string("calculated_advantage") << p_peer->get_calculated_advantage();
    add_value(std::format("peer_{}", std::to_string(p_peer->get_peer_id())), statuse.json());
}

} // namespace rollback_netcode