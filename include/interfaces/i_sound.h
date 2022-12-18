#pragma once

#include <interfaces/base/i_base.h>

namespace rollback_netcode {

class ISound : virtual public IBase {
public:
	virtual void play() = 0;
};

}