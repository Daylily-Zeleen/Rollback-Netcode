﻿#include <interfaces/i_network_proccessable.h>
#include <interfaces/i_sync_manager.h>

namespace rollback_netcode {
/**
 * @brief You must call this method after you setup this object,
 * 		  So that can be process by the rollback network.
 */
void INetwokProcessable::start_network_process() {
	ERR_THROW(ISyncManager::get_singleton() == nullptr, "Should instantia a SyncManager before start network process.");
	ISyncManager::get_singleton()->network_processables.emplace(ISyncObject::get_uuid(), this);
	_network_process_started = true;
}

/**
 * @brief You should call this method to stop process by rollback network.
 * At most time, you should stop process by rollback network when this object despawn.
 */
void INetwokProcessable::stop_network_process() {
	if (_network_process_started) {
		ERR_THROW(ISyncManager::get_singleton() == nullptr, "Should instantia a SyncManager before start network process.");
		ISyncManager::get_singleton()->network_processables.erase(ISyncObject::get_uuid());
		_network_process_started = false;
	}
}
} //namespace rollback_netcode