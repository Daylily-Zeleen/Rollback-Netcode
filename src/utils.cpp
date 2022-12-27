/*
 * @Author       : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @Date         : 2022-12-13 11:58:03
 * @LastEditors  : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @LastEditTime : 2022-12-14 16:29:22
 * @FilePath     : \rollback_netcode\include\utils.h
 * @Description  :
 * Copyright (c) 2022 by Daylily-Zeleen email: daylily-zeleen@foxmail.com, All Rights Reserved.
 */
#pragma once

#include "../include/utils.h"
#include "def.h"

#include <iterator>
#include <memory.h>
#include <stdint.h>
#include <string>

namespace rollback_netcode {

const std::chrono::steady_clock::time_point Utils::start_time = std::chrono::steady_clock::now();

// const auto start_time = std::chrono::steady_clock::now();
int64_t Utils::get_current_system_time_msec() { return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count(); }
int64_t Utils::get_current_tick_usec() { return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start_time).count(); }

template <typename... Args> std::string Utils::string_sprintf(const std::string &format, Args... args) {
    const auto size = std::snprintf(nullptr, 0, format.c_str(), args...) + 1;
    const auto buffer = std::make_unique<char[]>(size);

    std::snprintf(buffer.get(), size, format.c_str(), args...);

    return std::string(buffer.get(), buffer.get() + size - 1);
}

template <typename TArr>
    requires requires(typename TArr::value_type e) {
                 requires std::ranges::forward_range<TArr>;
                 std::to_string(e);
             }
std::string Utils::join_arr(const TArr &p_arr, const std::string &p_seperator) {
    std::string ret = "";
    auto count = p_arr.size();
    for (auto it = p_arr.begin(); it != p_arr.end(); it++) {
        if (it != p_arr.begin())
            ret += p_seperator;
        ret += std::to_string(*it);
    }
    return ret;
}

template <typename TArr>
    requires requires(typename TArr::value_type e) {
                 requires std::ranges::forward_range<TArr>;
                 std::to_string(e);
             }
std::string Utils::arr_to_string(const TArr &p_arr) {
    return "[" + join_arr(p_arr) + "]";
}

template <typename TArr>
    requires std::ranges::forward_range<TArr> && json_value<typename TArr::value_type>
JsonArr Utils::arr_to_json_arr(const TArr &p_arr) {
    JsonArr ret;
    for (auto it = p_arr.begin(); it != p_arr.end(); ++it) {
        ret << *it;
    }
    return ret;
}

template <typename TArr>
    requires std::ranges::forward_range<TArr> && json_value<typename TArr::value_type>
std::string Utils::arr_to_json_string(const TArr &p_arr) {
    return arr_to_json_arr(p_arr).json();
}

template <typename TMap>
    requires std::ranges::forward_range<TMap> && std::same_as<typename TMap::key_type, std::string> && json_value<typename TMap::mapped_type>
JsonObj Utils::map_to_json_obj(const TMap &p_map) {
    JsonObj ret;
    for (auto &kv : p_map) {
        ret << std::to_string(kv.first) << kv.second;
    }
    return ret;
}

template <typename TMap>
    requires std::ranges::forward_range<TMap> && std::same_as<typename TMap::key_type, std::string> && json_value<typename TMap::mapped_type>
std::string Utils::map_to_json_string(const TMap &p_map) {
    return map_to_json_obj(p_map).json();
}

template <typename TMap>
    requires requires(typename TMap::key_type k, typename TMap::mapped_type v) {
                 requires std::ranges::forward_range<TMap>;
                 std::to_string(k);
                 std::to_string(v);
             }
std::string Utils::map_to_string(const TMap &p_map) {
    std::string ret = "{";
    for (auto it = p_map.begin(); it < p_map.end(); it++) {
        if (it != p_map.begin()) {
            ret += ", ";
        }
        ret += std::to_string(it->first) + ": " + std::to_string(it->second);
    }
    return ret;
}

JsonObj Utils::convert_json_arr_to_json_obj(const JsonArr &p_arr) {
    JsonObj ret;
    for (uint32_t i = 0; i < p_arr.size(); i++) {
        ret << std::to_string(i) << p_arr.get(i);
    }
    return ret;
}

} // namespace rollback_netcode