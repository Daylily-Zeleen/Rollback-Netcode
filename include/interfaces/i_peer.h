/*
 * @Author       : git config user.name && 忘忧の && git config user.email
 * @Date         : 2022-12-07 13:57:35
 * @LastEditors  : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @LastEditTime : 2022-12-14 17:27:43
 * @FilePath     : \rollback_netcode\include\core\interfaces\i_peer.h
 * @Description  :
 * Copyright (c) 2022 by caizebin email: caizebin@oasis.com, All Rights Reserved.
 */
#pragma once

#include "interfaces/i_data.h"
#include <interfaces/base/i_base.h>


#include <list>

namespace rollback_netcode {

class IPeer : virtual public IBase {
public:
    /**
     * @brief Construct a new IPeer object
     *
     * @param peer_id negetive peer_id means that is a invalid peer.
     */
    IPeer(const PeerId_t peer_id = -1);

    PeerId_t get_peer_id() const { return peer_id; }
    uint32_t get_rtt() const { return rtt; }
    float get_time_delta() const { return time_delta; }
    uint32_t get_last_received_ping() const { return last_received_ping; }

    Tick_t get_last_received_remote_input_tick() const { return last_received_remote_input_tick; }
    Tick_t get_next_requested_local_input_tick() const { return next_requested_local_input_tick; }
    Tick_t get_last_received_remote_hash_tick() const { return last_received_remote_hash_tick; }
    Tick_t get_next_requested_local_hash_tick() const { return next_requested_local_hash_tick; }

    uint32_t get_remote_lag() const { return remote_lag; }
    uint32_t get_local_lag() const { return local_lag; }

    uint8_t get_calculated_advantage() const { return calculated_advantage; }

public:
    virtual SharedPtr<const IData> get_local_input() = 0;
    virtual SharedPtr<const IData> predict_remote_input(const SharedPtr<const IData> &p_previous_input, const Tick_t p_ticks_since_real_input) = 0;

protected:
    PeerId_t peer_id;

private:
    uint32_t rtt = 0;
    float time_delta = 0;
    uint32_t last_received_ping = 0;

    Tick_t last_received_remote_input_tick = 0;
    Tick_t next_requested_local_input_tick = 0;
    Tick_t last_received_remote_hash_tick = 0;
    Tick_t next_requested_local_hash_tick = 0;

    uint32_t remote_lag = 0;
    uint32_t local_lag = 0;

    uint8_t calculated_advantage = 0.0f;
    std::vector<uint32_t> advantage_list;

    void record_advantage(const uint8_t ticks_to_calculate_advantage);
    void clear_advantage();
    void clear();

    friend class ISyncManager;
};

} // namespace rollback_netcode