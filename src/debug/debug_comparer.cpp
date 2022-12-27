/*
 * @Author       : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @Date         : 2022-12-10 16:44:43
 * @LastEditors  : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @LastEditTime : 2022-12-14 16:56:44
 * @FilePath     : \rollback_netcode\src\debug\debug_comparer.cpp
 * @Description  :
 * Copyright (c) 2022 by Daylily-Zeleen email: daylily-zeleen@foxmail.com, All Rights Reserved.
 */
#include "../../include/utils.h"
#include <debug/debug_comparer.h>

#include <concepts>
// 状态比较迁移到 IState 接口
namespace rollback_netcode {

std::string join_arr(const std::vector<std::string> &p_arr, const std::string &p_seperator = ", ") {
    std::string ret = "";
    auto count = p_arr.size();
    for (auto it = p_arr.begin(); it != p_arr.end(); it++) {
        if (it != p_arr.begin())
            ret += p_seperator;
        ret += *it;
    }
    return ret;
}

std::string arr_to_string(const std::vector<std::string> &p_arr) { return "[" + join_arr(p_arr) + "]"; }

std::string _get_diff_path_string(const std::vector<std::string> &p_path, const std::string &p_key) {
    if (p_path.size() > 0) {
        return join_arr(p_path, std::string(" -> ")) + " -> " + p_key;
    }
    return p_key;
}

void DebugComparer::find_mismatches(const JsonObj &p_local, const JsonObj &p_remote) { _find_mismatches_recursive(p_local, p_remote); }

void DebugComparer::_find_mismatches_recursive(const JsonObj &p_local, const JsonObj &p_remote, const std::vector<std::string> &p_path) {
    bool missing_or_extra = false;

    auto local = p_local.kv_map();
    auto remote = p_remote.kv_map();

    for (auto &kv : local) {
        if (remote.find(kv.first) == remote.end()) {
            missing_or_extra = true;
            mismatches.emplace_back(MismatchType::MISSING, _get_diff_path_string(p_path, kv.first), std::to_string(kv.second), "null");
        }
    }

    for (auto &kv : remote) {
        if (local.find(kv.first) == remote.end()) {
            missing_or_extra = true;
            mismatches.emplace_back(MismatchType::EXTRA, _get_diff_path_string(p_path, kv.first), "null", std::to_string(kv.second));
        }
    }

    if (!missing_or_extra) {
        std::vector<std::string> local_keys(local.size());
        std::vector<std::string> remote_keys(remote.size());
        for (auto &kv : local) {
            local_keys.emplace_back(kv.first);
        }
        for (auto &kv : remote) {
            remote_keys.emplace_back(kv.first);
        }
        if (local_keys != remote_keys) {
            mismatches.emplace_back(MismatchType::REORDER, "KEYS", arr_to_string(local_keys), arr_to_string(remote_keys));
        }
    }

    for (auto &kv : local) {
        auto key = kv.first;
        auto remote_it = remote.find(key);
        if (remote_it == remote.end())
            continue;
        auto local_v = kv.second;
        auto remote_v = remote_it->second;
        if (local_v->type_ == jsonxx::Value::OBJECT_) {
            if (remote_v->type_ == jsonxx::Value::OBJECT_) {
                std::hash<std::string> hasher;
                if (hasher(local_v->object_value_->json()) != hasher(remote_v->object_value_->json())) {
                    std::vector<std::string> new_path(p_path);
                    new_path.emplace_back(key);
                    _find_mismatches_recursive(*(local_v->object_value_), *(remote_v->object_value_), new_path);
                }
            } else {
                mismatches.emplace_back(MismatchType::DIFFERENCE, _get_diff_path_string(p_path, key), std::to_string(local_v), std::to_string(remote_v));
            }
        } else if (local_v->type_ == jsonxx::Value::ARRAY_) {
            if (remote_v->type_ == jsonxx::Value::ARRAY_) {
                std::hash<std::string> hasher;
                if (hasher(local_v->object_value_->json()) != hasher(remote_v->object_value_->json())) {
                    std::vector<std::string> new_path(p_path);
                    new_path.emplace_back(key);
                    _find_mismatches_recursive(Utils::convert_json_arr_to_json_obj(*(local_v->array_value_)), Utils::convert_json_arr_to_json_obj(*(remote_v->array_value_)), new_path);
                }
            } else {
                mismatches.emplace_back(MismatchType::DIFFERENCE, _get_diff_path_string(p_path, key), std::to_string(local_v), std::to_string(remote_v));
            }
        } else if (local_v->type_ != remote_v->type_) {
            mismatches.emplace_back(MismatchType::DIFFERENCE, _get_diff_path_string(p_path, key), std::to_string(local_v), std::to_string(remote_v));
        } else {
            switch (local_v->type_) {
            case jsonxx::Value::NUMBER_: {
                if (local_v->number_value_ != remote_v->number_value_) {
                    mismatches.emplace_back(MismatchType::DIFFERENCE, _get_diff_path_string(p_path, key), std::to_string(local_v->number_value_), std::to_string(remote_v->number_value_));
                }
            } break;
            case jsonxx::Value::STRING_: {
                if (*(local_v->string_value_) != *(remote_v->string_value_)) {
                    mismatches.emplace_back(MismatchType::DIFFERENCE, _get_diff_path_string(p_path, key), *(local_v->string_value_), *(remote_v->string_value_));
                }
            } break;
            case jsonxx::Value::BOOL_: {
                if ((local_v->bool_value_) != (remote_v->bool_value_)) {
                    mismatches.emplace_back(MismatchType::DIFFERENCE, _get_diff_path_string(p_path, key), std::to_string(local_v->bool_value_), std::to_string(remote_v->bool_value_));
                }
            } break;
            case jsonxx::Value::NULL_: {
                // both null, do nothing.
            } break;
            default:
                ERR_THROW(true, "Should not be happened");
                break;
            }
        }
    }
}

std::string DebugComparer::get_mismatches_string() const {
    std::vector<std::string> data(mismatches.size() * 4);
    for (auto &mismatch : mismatches) {
        switch (mismatch.type) {
        case MismatchType::MISSING: {
            data.emplace_back(" => [MISSING] " + mismatch.path);
            data.emplace_back(mismatch.local_text);
            data.emplace_back("");
        } break;
        case MismatchType::EXTRA: {
            data.emplace_back(" => [EXTRA] " + mismatch.path);
            data.emplace_back(mismatch.remote_text);
            data.emplace_back("");
        } break;
        case MismatchType::REORDER: {
            data.emplace_back(" => [REORDER] " + mismatch.path);
            data.emplace_back("LOCAL:  %s" + mismatch.local_text);
            data.emplace_back("REMOTE: %s" + mismatch.remote_text);
            data.emplace_back("");
        } break;
        case MismatchType::DIFFERENCE: {
            data.emplace_back(" => [DIFF] " + mismatch.path);
            data.emplace_back("LOCAL:  %s" + mismatch.local_text);
            data.emplace_back("REMOTE: %s" + mismatch.remote_text);
            data.emplace_back("");
        } break;
        }
    }
    return join_arr(data, "\n");
}

} // namespace rollback_netcode