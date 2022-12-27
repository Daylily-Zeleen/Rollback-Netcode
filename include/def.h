/*
 * @Author       : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @Date         : 2022-12-07 14:54:49
 * @LastEditors  : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @LastEditTime : 2022-12-14 13:49:18
 * @FilePath     : \rollback_netcode\include\def.h
 * @Description  :
 * Copyright (c) 2022 by Daylily-Zeleen email: daylily-zeleen@foxmail.com, All Rights Reserved.
 */
#ifndef DEF_H
#define DEF_H

#include "../thirdparty/jsonxx/jsonxx.h"
#include "debug/debug.h"
#include <string>

namespace rollback_netcode {

// Should always inline no matter what.
#ifndef ALWAYS_INLINE_
#if defined(__GNUC__)
#define ALWAYS_INLINE_ __attribute__((always_inline)) inline
#elif defined(_MSC_VER)
#define ALWAYS_INLINE_ __forceinline
#else
#define ALWAYS_INLINE_ inline
#endif
#endif

// Should always inline, except in debug builds because it makes debugging harder.
#ifndef FORCE_INLINE_
#ifdef DISABLE_FORCED_INLINE
#define FORCE_INLINE_ inline
#else
#define FORCE_INLINE_ ALWAYS_INLINE_
#endif
#endif

using JsonObj = jsonxx::Object;
using JsonArr = jsonxx::Array;
using JsonStr = jsonxx::String;
using JsonNumber = jsonxx::Number;

#define MEMNEW(mclass) new mclass
#define MEMDELETE(mptr) delete mptr

// 可能以字符串作为id
using PeerId_t = int32_t;
using UUID_t = int64_t; // 代替 NodePath
using Hash_t = uint32_t;
using Tick_t = int32_t;
using SoundIdentifier_t = std::string;

#define uuid_to_text(p_uuid) std::to_string(p_uuid)

// std::string default_uuid_to_text(const UUID_t &p_uuid) { return std::to_string(p_uuid); }
// std::string (*uuid_to_text_func)(const UUID_t &) = &default_uuid_to_text;

// std::string uuid_to_text(const UUID_t &p_uuid) { return (*uuid_to_text_func)(p_uuid); }

} // namespace rollback_netcode

#endif