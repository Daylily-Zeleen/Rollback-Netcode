/*
 * @Author       : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @Date         : 2022-12-07 15:42:18
 * @LastEditors  : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @LastEditTime : 2022-12-14 15:38:01
 * @FilePath     : \rollback_netcode\include\core\interfaces\i_state.h
 * @Description  :
 * Copyright (c) 2022 by Daylily-Zeleen email: daylily-zeleen@foxmail.com, All Rights Reserved.
 */
#pragma once

#include <interfaces/base/i_base.h>
#include <def.h>

namespace rollback_netcode {

class IState : virtual public IBase {
public:
	virtual std::vector<std::string> get_keys() = 0;
	virtual bool has_key(const std::string &p_key) = 0;
	virtual const std::string get_value_text(const std::string &p_key) = 0;
	virtual JsonObj to_json_obj() const = 0;
	virtual Hash_t get_hash() const = 0;
};
} // namespace rollback_netcode

namespace std {
inline std::string to_string(const rollback_netcode::IState *p) { return p->to_json_obj().json(); }
} // namespace std