/*
 * @Author       : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @Date         : 2022-12-07 15:48:51
 * @LastEditors  : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @LastEditTime : 2022-12-14 18:11:49
 * @FilePath     : \rollback_netcode\include\core\state_hash_frame.h
 * @Description  :
 * Copyright (c) 2022 by Daylily-Zeleen email: daylily-zeleen@foxmail.com, All Rights Reserved.
 */

#pragma once

#include "interfaces/base/i_base.h"
#include <interfaces/i_peer.h>

#include <map>
#include <memory>
namespace rollback_netcode {

struct StateHashFrame {
public:
	Tick_t tick;
	Hash_t state_hash;

	std::map<PeerId_t, Hash_t> peer_hashes;
	bool mismatch = false;

	StateHashFrame(const Tick_t p_tick, const Hash_t p_state_hash) :
			tick(p_tick), state_hash(p_state_hash) {}
	bool record_peer_hash(const PeerId_t p_peer_id, const Hash_t p_peer_hash);
	[[nodiscard]] bool has_peer_hash(const PeerId_t p_peer_id) const;
	[[nodiscard]] bool is_complete(const std::map<PeerId_t, SharedPtr<IPeer>> &p_peers) const;
	void get_missing_peers(const std::map<PeerId_t, SharedPtr<IPeer>> &p_peers, std::vector<PeerId_t> &r_missing_peer_ids) const;
	[[nodiscard]] std::vector<PeerId_t> get_missing_peers(const std::map<PeerId_t, SharedPtr<IPeer>> &p_peers) const;
};
} // namespace rollback_netcode