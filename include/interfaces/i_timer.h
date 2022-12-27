/*
 * @Author       : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @Date         : 2022-12-07 15:42:18
 * @LastEditors  : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @LastEditTime : 2022-12-07 20:54:39
 * @FilePath     : \rollback_netcode\include\core\interfaces\i_timer.h
 * @Description  :
 * Copyright (c) 2022 by Daylily-Zeleen email: daylily-zeleen@foxmail.com, All Rights Reserved.
 */
#pragma once

#include <core/event.h>
#include <interfaces/base/i_base.h>
#include <functional>

namespace rollback_netcode {
class ITimer : virtual public IBase {
public:
	virtual void set_paused(const bool p_pause) = 0;
	virtual void set_wait_time(const float p_wait_time_in_second) = 0;
	virtual void set_loop(const bool loop) = 0;
	virtual void restart() = 0;

	Event<std::function<void()>> event_timeout;
};
} //namespace rollback_netcode