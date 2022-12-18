#pragma once

#include "base/i_base.h"
#include "i_spawn_data.h"
#include "utils.h"

namespace rollback_netcode {
class ISpawnData;

/// @brief A obj can be sapwned by a ISpawner with spawn data
class ISpawnable : virtual public ISyncObject {
public:
	virtual void network_spawn_preprocess(const SharedPtr<ISpawnData> &p_data) = 0;
	virtual void network_spawn(const SharedPtr<const ISpawnData> &p_data) = 0;
	virtual void network_despawn() = 0;

	virtual void destroy() = 0;

	// virtual void despawn(const ISpawnable &p_to_despawn) = 0;
	virtual bool is_valid() = 0;
	virtual JsonObj to_json_obj() const = 0;
	virtual Hash_t get_hash() = 0;
};
} //namespace rollback_netcode