/*
 * @Author       : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @Date         : 2022-12-07 14:38:51
 * @LastEditors  : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @LastEditTime : 2022-12-14 15:43:23
 * @FilePath     : \rollback_netcode\include\core\interfaces\i_input.h
 * @Description  :
 * Copyright (c) 2022 by Daylily-Zeleen email: daylily-zeleen@foxmail.com, All Rights Reserved.
 */
#pragma once
#include <interfaces/base/i_base.h>

#include <core/serialized_data.h>

namespace rollback_netcode {

/// @brief 输入接口(待实现)
class IInput : virtual public IBase {
public:
    virtual bool is_valid() const = 0;
    virtual uint32_t serialize(uint8_t *&r_buffer) const = 0;

    virtual Hash_t get_hash() const = 0;
    virtual void serialize(SerializedData &r_serialized_data) = 0;
    virtual void deserialize(const SerializedData &p_serialized_data) = 0;
    virtual JsonObj to_json_obj() const = 0;
};

} // namespace rollback_netcode

namespace std {
inline std::string to_string(const rollback_netcode::IInput *p_input) { return p_input->to_json_obj().json(); }
} // namespace std