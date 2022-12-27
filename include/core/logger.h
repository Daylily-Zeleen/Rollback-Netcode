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

#include <deque>
#include <fstream>
#include <mutex>
#include <semaphore>
#include <string>
#include <utility>
#include <vcruntime.h>
#include <vcruntime_string.h>

#include "core/serialized_data.h"
#include "input_buffer_frame.h"
#include "interfaces/i_peer.h"
#include "state_buffer_frame.h"

namespace rollback_netcode {

class Semaphore {
public:
    explicit Semaphore(int count = 0) : count_(count) {}

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

#pragma region Log
class Log {
public:
    enum LogType {
        HEADER,
        FRAME,
        STATE,
        INPUT,
    };

    LogType log_type;

    [[nodiscard]] virtual SerializedData serialize() const = 0;
};

class HeaderLog : public Log {
public:
    PeerId_t peer_id;
    SerializedData match_info_data;

    HeaderLog(PeerId_t p_peer_id, const SerializedData &p_match_info_data) : peer_id(p_peer_id), match_info_data(p_match_info_data) { log_type = HEADER; }

    [[nodiscard]] SerializedData serialize() const override {
        SerializedData ret(match_info_data.size + sizeof(PeerId_t));
        memcpy(ret.buffer, &peer_id, sizeof(PeerId_t));
        memcpy(ret.buffer + 1, &match_info_data.size, sizeof(decltype(match_info_data.size)));
        memcpy(ret.buffer + 1 + sizeof(decltype(match_info_data.size)), match_info_data.buffer, match_info_data.size);
        return ret;
    }
};
class FrameLog : public Log {
public:
    enum FrameType {
        INTERFRAME,
        TICK,
        INTERPOLATION_FRAME,
    };

    std::string frame_json_text;

    FrameLog(std::string p_frame_json_text) : frame_json_text(std::move(p_frame_json_text)) { log_type = FRAME; }

    [[nodiscard]] SerializedData serialize() const override {
        auto frame_json_text_lenth = frame_json_text.length();
        SerializedData ret(frame_json_text_lenth);
        memcpy(ret.buffer, &frame_json_text_lenth, sizeof(decltype(frame_json_text_lenth)));
        memcpy(ret.buffer + sizeof(decltype(frame_json_text_lenth)), frame_json_text.c_str(), frame_json_text_lenth);
        return ret;
    }
};

class StateLog : public Log {
public:
    Tick_t tick;
    std::map<UUID_t, SerializedData> state_data;

    StateLog(Tick_t p_tick, std::map<UUID_t, SerializedData> p_state_data) : tick(p_tick), state_data(std::move(p_state_data)) { log_type = STATE; }

    [[nodiscard]] SerializedData serialize() const override {
        size_t size = sizeof(Tick_t);
        for (auto &kv : state_data) {
            size += sizeof(UUID_t);
            size += sizeof(decltype(kv.second.size));
            size += state_data.size();
        }
        SerializedData ret(size);

        auto pos = ret.buffer;
        memcpy(pos, &tick, sizeof(Tick_t));
        pos += sizeof(Tick_t);
        for (auto &kv : state_data) {
            memcpy(pos, &kv.first, sizeof(UUID_t));
            pos += sizeof(UUID_t);
            memcpy(pos, &kv.second.size, sizeof(decltype(kv.second.size)));
            pos += sizeof(decltype(kv.second.size));
            memcpy(pos, kv.second.buffer, kv.second.size);
            pos += kv.second.size;
        }
        return ret;
    }
};

class InputLog : public Log {
public:
    Tick_t tick;
    class InputData {
    public:
        SerializedData input_data;
        bool predict;
        InputData(const SerializedData &p_input_data, bool p_predict) : input_data(p_input_data), predict(p_predict) {}
    };

    std::map<PeerId_t, InputData> peers_input_data;

    InputLog(Tick_t p_tick, std::map<PeerId_t, InputData> p_peers_input_data) : tick(p_tick), peers_input_data(std::move(p_peers_input_data)) { log_type = INPUT; }

    [[nodiscard]] SerializedData serialize() const override {
        int64_t size = sizeof(Tick_t);

        for (auto &kv : peers_input_data) {
            size += sizeof(PeerId_t);

            size += sizeof(decltype(kv.second.input_data.size));
            size += kv.second.input_data.size;

            size += sizeof(bool);
        }
        SerializedData ret(size);

        auto pos = ret.buffer;
        memcpy(pos, &tick, sizeof(Tick_t));
        pos += sizeof(Tick_t);

        for (auto &kv : peers_input_data) {
            memcpy(pos, &kv.first, sizeof(PeerId_t));
            pos += sizeof(PeerId_t);

            memcpy(pos, &kv.second.input_data.size, sizeof(decltype(kv.second.input_data.size)));
            pos += sizeof(decltype(kv.second.input_data.size));
            memcpy(pos, kv.second.input_data.buffer, kv.second.input_data.size);
            pos += kv.second.input_data.size;

            memcpy(pos, &kv.second.predict, sizeof(bool));
            pos += sizeof(bool);
        }
        return ret;
    }
};
#pragma endregion

class Logger {
public:
    enum SkipReason {
        ADVANTAGE_ADJUSTMENT,
        INPUT_BUFFER_UNDERRUN,
        WAITING_TO_REGAIN_SYNC,
    };

    bool start(const std::string &p_log_file_path, const PeerId_t &p_network_peer_id, const SharedPtr<IData> &p_match_info);
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
    std::deque<SerializedData> _write_queue;
    std::fstream _log_file;
    bool _started = false;

    jsonxx::Object data;
    jsonxx::Object _start_times;

    void _writer_thread_function();
    void write_current_data();
};
} // namespace rollback_netcode