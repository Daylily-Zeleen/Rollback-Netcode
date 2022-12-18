/*
 * @Author       : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @Date         : 2022-12-07 14:33:45
 * @LastEditors  : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @LastEditTime : 2022-12-12 10:52:37
 * @FilePath     : \rollback_netcode\src\core\peer.cpp
 * @Description  :
 * Copyright (c) 2022 by Daylily-Zeleen email: daylily-zeleen@foxmail.com, All Rights Reserved.
 */
#include <interfaces/i_peer.h>

namespace rollback_netcode {
IPeer::IPeer(const PeerId_t peer_id) :
		peer_id(peer_id) {}

void IPeer::record_advantage(const uint8_t ticks_to_calculate_advantage) {
	advantage_list.emplace_back(local_lag - remote_lag);
	if (advantage_list.size() >= ticks_to_calculate_advantage) {
		float total = 0.0;
		for (unsigned int &e : advantage_list) {
			total += static_cast<float>(e);
		}
		calculated_advantage = static_cast<uint8_t>(std::round(total / float(advantage_list.size())));
		advantage_list.clear();
	}
}
void IPeer::clear_advantage() {
	calculated_advantage = 0.0f;
	advantage_list.clear();
}
void IPeer::clear() {
	rtt = 0;
	last_received_ping = 0;
	time_delta = 0;
	last_received_remote_input_tick = 0;
	next_requested_local_input_tick = 0;
	last_received_remote_hash_tick = 0;
	next_requested_local_hash_tick = 0;
	remote_lag = 0;
	local_lag = 0;
	clear_advantage();
}
} // namespace rollback_netcode