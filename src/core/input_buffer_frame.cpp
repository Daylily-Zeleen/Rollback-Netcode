/*
 * @Author       : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @Date         : 2022-12-07 15:37:28
 * @LastEditors  : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @LastEditTime : 2022-12-14 19:42:22
 * @FilePath     : \rollback_netcode\src\core\input_buffer_frame.cpp
 * @Description  :
 * Copyright (c) 2022 by Daylily-Zeleen email: daylily-zeleen@foxmail.com, All Rights Reserved.
 */
#include <core/input_buffer_frame.h>
#include <thirdparty/jsonxx/jsonxx.h>

namespace rollback_netcode {

SharedPtr<const IInput> InputBufferFrame::get_peer_input(const PeerId_t p_peer_id) const {
	auto it = peers_input.find(p_peer_id);
	return (it != peers_input.end()) ? (it->second.input) : nullptr;
}

bool InputBufferFrame::is_peer_input_predicated(const PeerId_t p_peer_id) const {
	auto it = peers_input.find(p_peer_id);
	return (it != peers_input.end()) ? it->second.predicted : true;
}

void InputBufferFrame::get_missing_peers(const std::map<PeerId_t, SharedPtr<IPeer>> &p_peers, std::vector<PeerId_t> &r_missing_peer_ids) const {
	r_missing_peer_ids.clear();
	for (auto &&kv : p_peers) {
		const PeerId_t peer_id = kv.first;
		auto it = peers_input.find(peer_id);
		if (it == peers_input.end() || it->second.predicted) {
			r_missing_peer_ids.emplace_back(peer_id);
		}
	}
}
std::vector<PeerId_t> InputBufferFrame::get_missing_peers(const std::map<PeerId_t, SharedPtr<IPeer>> &p_peers) const {
	std::vector<PeerId_t> ret(p_peers.size());
	get_missing_peers(p_peers, ret);
	return ret;
}

void InputBufferFrame::add_peer_input(const PeerId_t p_peer_id, const SharedPtr<const IInput> &p_input, const bool p_predict) {
	peers_input.emplace(p_peer_id, PeerInput(p_input, p_predict));
}

bool InputBufferFrame::is_complete(const std::map<PeerId_t, SharedPtr<IPeer>> &p_peers) const {
	for (auto &&kv : p_peers) {
		const PeerId_t peer_id = kv.first;
		auto it = peers_input.find(peer_id);
		if (it == peers_input.end() || it->second.predicted) {
			return false;
		}
	}
	return true;
}

JsonObj InputBufferFrame::to_json_obj() const {
	JsonObj input_frame_json, peers_input_json;

	for (auto &kv : peers_input) {
		const PeerId_t peer_id = kv.first;
		const PeerInput peer_input = kv.second;

		JsonObj peer_input_json("predict", peer_input.predicted);
		peer_input_json << "input" << peer_input.input->to_json_obj();

		peers_input_json << peer_id << peer_input_json;
	}

	input_frame_json << "tick" << tick;
	input_frame_json << "peers_input" << peers_input_json;

	return input_frame_json;
}
} // namespace rollback_netcode