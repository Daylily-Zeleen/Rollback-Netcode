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

#include <concepts>
#include <def.h>

#include <chrono>

namespace rollback_netcode {
template <typename T>
concept json_value =
    requires {
        std::same_as<T, JsonObj> || std::same_as<T, JsonArr> || std::same_as<T, JsonStr> || std::integral<T> || std::floating_point<T> || std::same_as<T, bool> || std::same_as<T, jsonxx::Value>;
    };
class Utils {
public:
    static const std::chrono::steady_clock::time_point start_time ;
    static int64_t get_current_system_time_msec();
    static int64_t get_current_tick_usec();

    template <typename... Args> static std::string string_sprintf(const std::string &format, Args... args);

    template <typename TArr>
        requires requires(typename TArr::value_type e) {
                     requires std::ranges::forward_range<TArr>;
                     std::to_string(e);
                 }
    static std::string join_arr(const TArr &p_arr, const std::string &p_seperator = ", ");
    template <typename TArr>
        requires requires(typename TArr::value_type e) {
                     requires std::ranges::forward_range<TArr>;
                     std::to_string(e);
                 }
    static std::string arr_to_string(const TArr &p_arr);

    template <typename TArr>
        requires std::ranges::forward_range<TArr> && json_value<typename TArr::value_type>
    static JsonArr arr_to_json_arr(const TArr &p_arr);

    template <typename TArr>
        requires std::ranges::forward_range<TArr> && json_value<typename TArr::value_type>
    static std::string arr_to_json_string(const TArr &p_arr);

    template <typename TMap>
        requires std::ranges::forward_range<TMap> && std::same_as<typename TMap::key_type, std::string> && json_value<typename TMap::mapped_type>
    static JsonObj map_to_json_obj(const TMap &p_map);

    template <typename TMap>
        requires std::ranges::forward_range<TMap> && std::same_as<typename TMap::key_type, std::string> && json_value<typename TMap::mapped_type>
    static std::string map_to_json_string(const TMap &p_map);

    template <typename TMap>
        requires requires(typename TMap::key_type k, typename TMap::mapped_type v) {
                     requires std::ranges::forward_range<TMap>;
                     std::to_string(k);
                     std::to_string(v);
                 }
    static std::string map_to_string(const TMap &p_map);

    static JsonObj convert_json_arr_to_json_obj(const JsonArr &p_arr);
};
} // namespace rollback_netcode