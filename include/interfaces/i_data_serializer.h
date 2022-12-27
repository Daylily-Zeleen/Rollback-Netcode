/*
 * @Author       : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @Date         : 2022-12-07 16:02:06
 * @LastEditors  : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @LastEditTime : 2022-12-14 17:46:18
 * @FilePath     : \rollback_netcode\include\core\interfaces\i_data_serializer.h
 * @Description  :
 * Copyright (c) 2022 by Daylily-Zeleen email: daylily-zeleen@foxmail.com, All Rights Reserved.
 */

#pragma once

#include "interfaces/i_data.h"
#include <core/message.h>
#include <core/serialized_data.h>
#include <def.h>
#include <string>


namespace rollback_netcode {
class IDataSerializer : virtual public IBase {
public:
    virtual SerializedData serialize_match_info(const SharedPtr<const IData> &p_input) = 0;
    virtual SharedPtr<const IData> deserialize_match_info(const SerializedData &p_serialized_data) = 0;

    virtual SerializedData serialize_state(const SharedPtr<const IData> &p_state) = 0;
    virtual SharedPtr<const IData> deserialize_state(const SerializedData &p_serialized_data) = 0;

    virtual SerializedData serialize_input(const SharedPtr<const IData> &p_input) = 0;
    virtual SharedPtr<const IData> deserialize_input(const SerializedData &p_serialized_data) = 0;

    virtual SerializedData serialize_message(const PeerFrameMessage &p_peer_frame_message) = 0;
    virtual void deserialize_message(const SerializedData &p_serialized_data, PeerFrameMessage &r_peer_frame_msg) = 0;
};
} // namespace rollback_netcode