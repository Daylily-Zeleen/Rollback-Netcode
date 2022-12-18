/*
 * @Author       : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @Date         : 2022-12-10 14:44:21
 * @LastEditors  : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @LastEditTime : 2022-12-11 16:40:10
 * @FilePath     : \rollback_netcode\include\debug\debug.h
 * @Description  :
 * Copyright (c) 2022 by Daylily-Zeleen email: daylily-zeleen@foxmail.com, All Rights Reserved.
 */
#pragma once

#ifdef DEBUG
#define ERR_THROW(cond, msg) \
    if (cond)                \
        throw msg;
#else
#define ERR_THROW(cond, msg)
#endif

#ifdef DEBUG
#define WARNING(msg) throw msg;
#else
#define WARNING(msg)
#endif

#ifdef DEBUG 
#define HINT(msg) throw msg;
#else
#define HINT(msg)
#endif

#ifdef DEBUG 
#define PUSH_ERR(msg) throw msg;
#else
#define PUSH_ERR(msg)
#endif

#ifdef DEBUG

#endif