/*
 * @Author       : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @Date         : 2022-12-07 15:48:51
 * @LastEditors  : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @LastEditTime : 2022-12-14 19:43:07
 * @FilePath     : \rollback_netcode\src\core\state_hash_frame.cpp
 * @Description  :
 * Copyright (c) 2022 by Daylily-Zeleen email: daylily-zeleen@foxmail.com, All Rights Reserved.
 */

#include <core/state_hash_frame.h>

namespace rollback_netcode {
bool StateHashFrame::record_peer_hash(const PeerId_t p_peer_id, const Hash_t p_peer_hash) {
	peer_hashes.emplace(p_peer_id, p_peer_hash);
	if (p_peer_hash != state_hash) {
		mismatch = true;
		return false;
	}
	return true;
}
bool StateHashFrame::has_peer_hash(const PeerId_t p_peer_id) const { return peer_hashes.count(p_peer_id) > 0; }
bool StateHashFrame::is_complete(const std::map<PeerId_t, SharedPtr<IPeer>> &p_peers) const {
	for (auto &&kv : p_peers) {
		const auto peer_id = kv.first;
		if (peer_hashes.count(peer_id) == 0) {
			return false;
		}
	}
	return true;
}
void StateHashFrame::get_missing_peers(const std::map<PeerId_t, SharedPtr<IPeer>> &p_peers, std::vector<PeerId_t> &r_missing_peer_ids) const {
	r_missing_peer_ids.clear();
	for (auto &&kv : p_peers) {
		const PeerId_t peer_id = kv.first;
		auto it = peer_hashes.find(peer_id);
		if (it == peer_hashes.end()) {
			r_missing_peer_ids.emplace_back(peer_id);
		}
	}
}

std::vector<PeerId_t> StateHashFrame::get_missing_peers(const std::map<PeerId_t, SharedPtr<IPeer>> &p_peers) const {
	std::vector<PeerId_t> ret(p_peers.size());
	get_missing_peers(p_peers, ret);
	return ret;
}
} // namespace rollback_netcode