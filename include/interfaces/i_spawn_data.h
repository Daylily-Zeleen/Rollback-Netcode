#pragma once

#include "interfaces/base/i_base.h"
#include "../utils.h"

#include "base/i_base.h"
#include <string>

namespace rollback_netcode {

/// @brief A spawn data should be targeted to a specify uuid.
class ISpawnData : virtual public IBase {
public:
	virtual bool operator==(const ISpawnData &other) const = 0;

	virtual JsonObj to_json_obj() const = 0;
	virtual Hash_t get_hash() const = 0;
};

} //namespace rollback_netcode
