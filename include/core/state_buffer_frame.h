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

#include "interfaces/i_data.h"
#include <hashfuncs.h>

#include <memory>
#include <utility>

namespace rollback_netcode {

struct StateBufferFrame {
	Tick_t tick;
	std::map<UUID_t, SharedPtr<const IData>> data;

	StateBufferFrame(const Tick_t p_tick, std ::map<UUID_t, SharedPtr<const IData>> &p_data) :
			tick(p_tick), data(std::move(p_data)) {}

	[[nodiscard]] Hash_t get_data_hash() const ;

	[[nodiscard]] JsonObj to_json_obj() const;
};
} // namespace rollback_netcode