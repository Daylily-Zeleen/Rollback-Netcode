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

#include "interfaces/i_sync_manager.h"
#include <core/state_buffer_frame.h>
#include <numeric>

namespace rollback_netcode {

[[nodiscard]] Hash_t StateBufferFrame::get_data_hash() const {
    static const auto data_adaptor = ISyncManager::get_singleton()->data_adaptor;
    ERR_THROW(data_adaptor == nullptr, "DataAdaptor is not setted.");
    Hash_t h = hash_murmur3_one_32(10);
    for (auto &kv : data) {
        h = hash_murmur3_one_32(kv.first, h);
        h = hash_murmur3_one_32(kv.second->get_hash(), h);
    }
    return hash_fmix32(h);
}

[[nodiscard]] JsonObj StateBufferFrame::to_json_obj() const {
    JsonObj ret, data_json;
    static const auto data_adaptor = ISyncManager::get_singleton()->data_adaptor;
    ERR_THROW(data_adaptor == nullptr, "DataAdaptor is not setted.");
    for (auto &kv : data) {
        data_json << uuid_to_text(kv.first) << kv.second->to_json_obj();
    }
    ret << std::string("tick") << tick;
    ret << std::string("data") << data_json;
    return ret;
}

} // namespace rollback_netcode