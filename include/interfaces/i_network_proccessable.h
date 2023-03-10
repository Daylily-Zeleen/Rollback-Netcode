/*
 * @Author       : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @Date         : 2022-12-09 20:00:33
 * @LastEditors  : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @LastEditTime : 2022-12-14 15:20:23
 * @FilePath     : \rollback_netcode\include\core\interfaces\i_network_proccessable.h
 * @Description  :
 * Copyright (c) 2022 by Daylily-Zeleen email: daylily-zeleen@foxmail.com, All Rights Reserved.
 */
#pragma once

#include "interfaces/i_data.h"
#include "interfaces/base/i_base.h"

namespace rollback_netcode {
class INetwokProcessable : public virtual ISyncObject {
public:
    virtual void network_preprocess(const SharedPtr<const IData> &input) = 0;
    virtual void network_process(const SharedPtr<const IData> &input) = 0;
    virtual void network_postprocess(const SharedPtr<const IData> &input) = 0;

    virtual PeerId_t get_network_master_peer_id() const { return -1; }
    virtual void network_preprocess_without_input() {}
    virtual void network_process_without_input() {}
    virtual void network_postprocess_without_input() {}

    /**
     * @brief You must call this method after you setup this object,
     * 		  So that can be process by the rollback network.
     */
    void enable_network_process();

    /**
     * @brief You should call this method to stop process by rollback network.
     * At most time, you should stop process by rollback network when this object despawn.
     */
    void disable_network_process();

    virtual ~INetwokProcessable() { disable_network_process(); }

private:
    bool _network_process_started = false;
};
} // namespace rollback_netcode