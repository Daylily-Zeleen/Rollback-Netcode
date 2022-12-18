/*
 * @Author       : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @Date         : 2022-12-07 15:40:26
 * @LastEditors  : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @LastEditTime : 2022-12-14 15:39:57
 * @FilePath     : \rollback_netcode\include\core\state_buffer_frame.h
 * @Description  :
 * Copyright (c) 2022 by Daylily-Zeleen email: daylily-zeleen@foxmail.com, All Rights Reserved.
 */

#pragma once

#include <interfaces/i_state.h>
#include <def.h>
#include <thirdparty/hashfuncs.h>
#include <thirdparty/jsonxx/jsonxx.h>

#include <memory>
#include <utility>

namespace rollback_netcode {

struct StateBufferFrame {
	Tick_t tick;
	const std::map<UUID_t, SharedPtr<const IState>> data;

	StateBufferFrame(const Tick_t p_tick, std ::map<UUID_t, SharedPtr<const IState>> p_data) :
			tick(p_tick), data(std::move(p_data)) {}

	[[nodiscard]] Hash_t get_data_hash() const {
		Hash_t h = hash_murmur3_one_32(10);
		for (auto &kv : data) {
			h = hash_murmur3_one_32(kv.first, h);
			h = hash_murmur3_one_32(kv.second->get_hash(), h);
		}
		return hash_fmix32(h);
	}

	[[nodiscard]] JsonObj to_json_obj() const {
		JsonObj ret, data_json;
		for (auto &kv : data) {
			data_json << uuid_to_text(kv.first) << kv.second->to_json_obj();
		}
		ret << "tick" << tick;
		ret << "data" << data_json;
		return ret;
	}
};
} // namespace rollback_netcode