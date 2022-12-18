/*
 * @Author       : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @Date         : 2022-12-10 16:44:43
 * @LastEditors  : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @LastEditTime : 2022-12-14 16:39:52
 * @FilePath     : \rollback_netcode\include\debug\debug_comparer.h
 * @Description  :
 * Copyright (c) 2022 by Daylily-Zeleen email: daylily-zeleen@foxmail.com, All Rights Reserved.
 */
#pragma once

#include <interfaces/i_state.h>
#include <def.h>

#include <utility>
namespace rollback_netcode {
// For debug
enum class MismatchType {
	MISSING,
	EXTRA,
	REORDER,
	DIFFERENCE,
};

struct Mismatch {
	MismatchType type;
	std::string path;
	std::string local_text;
	std::string remote_text;
	Mismatch(const MismatchType p_type, std::string p_path, std::string p_local_text, std::string p_remote_text) :
			type(p_type), path(std::move(p_path)), local_text(std::move(p_local_text)), remote_text(std::move(p_remote_text)) {}
};

class DebugComparer {
public:
	std::vector<Mismatch> mismatches;

	void find_mismatches(const JsonObj &p_local, const JsonObj &p_remote);
	[[nodiscard]] std::string get_mismatches_string() const;

private:
	void _find_mismatches_recursive(const JsonObj &p_local, const JsonObj &p_remote, const std::vector<std::string> &p_path = {});
};
} // namespace rollback_netcode
