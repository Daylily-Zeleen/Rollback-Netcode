#include <interfaces/i_network_rollbackable.h>
#include <interfaces/i_sync_manager.h>

namespace rollback_netcode {

void INetworkRollbackable::enable_network_rollback() {
	ERR_THROW(ISyncManager::get_singleton() == nullptr, "Should instantia a RollbackNetworkSyncManager before start network process.");
	ISyncManager::get_singleton()->network_rollbackables.emplace(this->get_uuid(), this);
	_network_rollback_enabled = true;
}
void INetworkRollbackable::disable_network_rollback() {
	if (_network_rollback_enabled) {
		ERR_THROW(ISyncManager::get_singleton() == nullptr, "Should instantia a RollbackNetworkSyncManager before start network process.");
		ISyncManager::get_singleton()->network_rollbackables.erase(this->get_uuid());
		_network_rollback_enabled = false;
	}
}
} //namespace rollback_netcode