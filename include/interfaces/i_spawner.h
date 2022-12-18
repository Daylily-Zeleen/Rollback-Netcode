#pragma onces
#include "utils.h"

#include "i_spawnable.h"

namespace rollback_netcode {
/// @brief A spawner is to spawn a specify obj.
///        You need to setup it's uuid before return.
class ISpawner : virtual public IBase {
public:
	virtual SharedPtr<ISpawnable> spawn() const = 0;
	virtual bool operator==(const ISpawner &other) const = 0;

	virtual Hash_t get_hash() const = 0;
	virtual JsonObj to_json_obj() const = 0;
};
} //namespace rollback_netcode
