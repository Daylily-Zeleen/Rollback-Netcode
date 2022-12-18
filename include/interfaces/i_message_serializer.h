/*
 * @Author       : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @Date         : 2022-12-07 16:02:06
 * @LastEditors  : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @LastEditTime : 2022-12-14 17:46:18
 * @FilePath     : \rollback_netcode\include\core\interfaces\i_message_serializer.h
 * @Description  :
 * Copyright (c) 2022 by Daylily-Zeleen email: daylily-zeleen@foxmail.com, All Rights Reserved.
 */

#pragma once

#include <interfaces/i_input.h>
#include <core/serialized_data.h>
#include <core/message.h>
#include <def.h>

namespace rollback_netcode {
class IMessageSerializer : virtual public IBase{
public:
	virtual void serialize_input(const SharedPtr<const IInput> &p_input, SerializedData &r_serialized_data) = 0;
	virtual SharedPtr<const IInput> deserialize_input(const SerializedData &p_serialized_data) = 0;

	virtual void serialize_message(const PeerFrameMessage &p_peer_frame_message, SerializedData &r_serialized_data) = 0;
	virtual PeerFrameMessage deserialize_message(const SerializedData &p_serialized_data) = 0;
};
} // namespace rollback_netcode