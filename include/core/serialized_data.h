/*
 * @Author       : git config user.name && 忘忧の && git config user.email
 * @Date         : 2022-12-07 14:43:00
 * @LastEditors  : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @LastEditTime : 2022-12-13 11:05:09
 * @FilePath     : \rollback_netcode\include\core\serialized_data.h
 * @Description  :
 * Copyright (c) 2022 by Daylily-Zeleen email: daylily-zeleen@foxmail.com, All Rights Reserved.
 */
#pragma once

#include <def.h>

#define DEFAULT_BUFFER_CAPACITY UINT8_MAX // default 256 bytes

namespace rollback_netcode {
struct SerializedData {
    char *buffer;
    int64_t size;

    SerializedData(const int64_t &p_buffer_capacity = DEFAULT_BUFFER_CAPACITY) { buffer = MEMNEW(char[p_buffer_capacity]); }
    ~SerializedData() { MEMDELETE(buffer); }
};

} // namespace rollback_netcode