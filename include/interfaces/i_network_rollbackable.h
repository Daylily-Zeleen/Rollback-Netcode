/*
 * @Author       : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @Date         : 2022-12-07 19:42:56
 * @LastEditors  : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @LastEditTime : 2022-12-14 17:33:21
 * @FilePath     : \rollback_netcode\include\core\interfaces\i_network_rollbackable.h
 * @Description  :
 * Copyright (c) 2022 by Daylily-Zeleen email: daylily-zeleen@foxmail.com, All Rights Reserved.
 */
#pragma once

#include <interfaces/base/i_base.h>
#include <interfaces/i_state.h>
#include <def.h>

namespace rollback_netcode {

class INetworkRollbackable : public virtual ISyncObject {
public:
	virtual SharedPtr<const IState> save_state() = 0;
	virtual void load_state(const SharedPtr<const IState> &p_state) = 0;
	virtual void interpolate_state(const SharedPtr<const IState> &p_old_state, const SharedPtr<const IState> &p_new_state, const float weight) = 0;

	/**
	 * @brief You must call this method after you setup this object,
	 * 		  So that can be rollback by the rollback network.
	 */
	void enable_network_rollback();

	/**
	 * @brief You should call this method to disable network rollback.
	 * At most time, you should stop process by rollback network when this object despawn.
	 */
	void disable_network_rollback();

	virtual ~INetworkRollbackable() { disable_network_rollback(); }
private:
	bool _network_rollback_enabled = false;
};
} //namespace rollback_netcode