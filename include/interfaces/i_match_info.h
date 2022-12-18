/*
 * @Author       : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @Date         : 2022-12-07 15:42:18
 * @LastEditors  : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @LastEditTime : 2022-12-14 15:43:47
 * @FilePath     : \rollback_netcode\include\core\interfaces\i_match_info.h
 * @Description  :
 * Copyright (c) 2022 by Daylily-Zeleen email: daylily-zeleen@foxmail.com, All Rights Reserved.
 */
#pragma once

#include <interfaces/base/i_base.h>
#include <def.h>

namespace rollback_netcode {
class IMatchInfo : virtual public IBase {
public:
	virtual JsonObj to_json_obj() const = 0;
};
} //namespace rollback_netcode
namespace std {
inline std::string to_string(const rollback_netcode::IMatchInfo *p) { return p->to_json_obj().json(); }
} // namespace std