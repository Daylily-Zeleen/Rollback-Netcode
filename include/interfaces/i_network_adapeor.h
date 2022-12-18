/*
 * @Author       : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @Date         : 2022-12-07 16:02:06
 * @LastEditors  : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @LastEditTime : 2022-12-15 22:51:31
 * @FilePath     : \rollback_netcode\include\core\interfaces\i_network_adapeor.h
 * @Description  :
 * Copyright (c) 2022 by Daylily-Zeleen email: daylily-zeleen@foxmail.com, All Rights Reserved.
 */

#pragma once

#include <core/event.h>
#include <interfaces/i_peer.h>
#include <interfaces/i_network_rollbackable.h>
#include <core/message.h>
#include <core/serialized_data.h>
#include <def.h>
#include <functional>


namespace rollback_netcode {
class INetworkAdaptor : virtual public IBase {
public:
	Event<std::function<void(const PeerId_t &p_peer_id, PingMessage &p_peer_msg)>> event_ping_received;
	Event<std::function<void(const PeerId_t &p_peer_id, PingMessage &p_peer_msg)>> event_ping_back_received;
	Event<std::function<void()>> event_start_remote_received;
	Event<std::function<void()>> event_start_stop_received;
	Event<std::function<void(const PeerId_t &p_sender_peer_id, const SerializedData &p_serialized_data)>> event_input_tick_received;

	virtual void attach_network_adaptor(const SharedPtr<IBase> &p_sync_manager) {}
	virtual void detach_network_adaptor(const SharedPtr<IBase> &p_sync_manager) {}
	virtual void start_network_adaptor(const SharedPtr<IBase> &p_sync_manager) {}
	virtual void stop_network_adaptor(const SharedPtr<IBase> &p_sync_manager) {}

	virtual void send_ping(const PeerId_t p_peer_id, const PingMessage &p_message) = 0;
	virtual void send_ping_back(const PeerId_t p_peer_id, const PingMessage &p_message) = 0;
	virtual void send_remote_start(const PeerId_t p_peer_id) = 0;
	virtual void send_remote_stop(const PeerId_t p_peer_id) = 0;
	virtual void send_input_tick(const PeerId_t p_peer_id, const SerializedData &p_serialized_data) = 0;

	virtual bool is_network_host() = 0;
	virtual bool is_network_master_for_obj(const INetworkRollbackable p_object) = 0;

	virtual SharedPtr<IPeer> get_network_peer() = 0;

	virtual void poll(){};

	PeerId_t get_network_peer_id() {
		ERR_THROW(get_network_peer() == nullptr, "Can't get local network peer.");
		return get_network_peer()->get_peer_id();
	}
};
} // namespace rollback_netcode