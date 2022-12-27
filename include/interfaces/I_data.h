#pragma once

#include "interfaces/base/i_base.h"

namespace rollback_netcode {

class IData : virtual public IBase {
public:
    virtual Hash_t get_hash() const = 0;
    virtual JsonObj to_json_obj() const = 0;
};

} // namespace rollback_netcode