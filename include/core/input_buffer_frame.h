/*
 * @Author       : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @Date         : 2022-12-07 14:51:38
 * @LastEditors  : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @LastEditTime : 2022-12-14 17:25:37
 * @FilePath     : \rollback_netcode\include\core\input_buffer_frame.h
 * @Description  :
 * Copyright (c) 2022 by Daylily-Zeleen email: daylily-zeleen@foxmail.com, All Rights Reserved.
 */
#pragma once
#include "interfaces/base/i_base.h"
#include <interfaces/i_peer.h>
#include <def.h>

#include <map>

#include <list>
#include <memory>
#include <vector>

namespace rollback_netcode {

struct InputBufferFrame {
public:
	struct PeerInput {
		const SharedPtr<const IInput> input;
		const bool predicted = false;

		PeerInput(const SharedPtr<const IInput> &p_input, const bool p_predicted) :
				input(p_input), predicted(p_predicted) {}
	};

	const Tick_t tick;

	std::map<PeerId_t, PeerInput> peers_input;

	InputBufferFrame(Tick_t tick) :
			tick(tick) {}

	[[nodiscard]] SharedPtr<const IInput> get_peer_input(const PeerId_t p_peer_id) const;

	[[nodiscard]] bool is_peer_input_predicated(const PeerId_t p_peer_id) const;

	void get_missing_peers(const std::map<PeerId_t, SharedPtr<IPeer>> &p_peers, std::vector<PeerId_t> &r_missing_peer_ids) const;
	[[nodiscard]] std::vector<PeerId_t> get_missing_peers(const std::map<PeerId_t, SharedPtr<IPeer>> &p_peers) const;

	void add_peer_input(const PeerId_t p_peer_id, const SharedPtr<const IInput> &p_input, const bool p_predict = false);

	[[nodiscard]] bool is_complete(const std::map<PeerId_t, SharedPtr<IPeer>> &p_peers) const;

	[[nodiscard]] JsonObj to_json_obj() const;
};

} // namespace rollback_netcode