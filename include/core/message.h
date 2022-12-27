/*
 * @Author       : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @Date         : 2022-12-07 19:53:36
 * @LastEditors  : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @LastEditTime : 2022-12-11 02:10:09
 * @FilePath     : \rollback_netcode\include\core\message.h
 * @Description  :
 * Copyright (c) 2022 by Daylily-Zeleen email: daylily-zeleen@foxmail.com, All Rights Reserved.
 */
#pragma once

#include <core/serialized_data.h>
#include "../utils.h"

namespace rollback_netcode {
struct PingMessage {
public:
    int64_t local_time;
    int64_t remote_time = 0;

    PingMessage() { local_time = Utils::get_current_system_time_msec(); }

    PingMessage(const int64_t p_local_time, const int64_t p_remote_time) : local_time(p_local_time), remote_time(p_remote_time) {}

    void set_remote_time() { remote_time = Utils::get_current_system_time_msec(); }
};

struct PeerFrameMessage {
public:
    Tick_t next_requested_input_tick;
    Tick_t next_hash_input_tick;
    std::map<Tick_t, SerializedData> inputs;
    std::map<Tick_t, Hash_t> state_hashs;

    PeerFrameMessage(const Tick_t &p_next_requested_input_tick, std::map<Tick_t, SerializedData> &p_inputs, const Tick_t &p_next_hash_input_tick, std::map<Tick_t, Hash_t> &p_state_hashs) {
        next_requested_input_tick = p_next_requested_input_tick;
        next_hash_input_tick = p_next_hash_input_tick;
        inputs = std::move(p_inputs);
        state_hashs = std::move(p_state_hashs);
    }
    PeerFrameMessage() = default;
};
} // namespace rollback_netcode