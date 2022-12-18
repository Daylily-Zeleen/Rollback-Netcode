/*
 * @Author       : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @Date         : 2022-12-12 11:21:27
 * @LastEditors  : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @LastEditTime : 2022-12-15 21:45:11
 * @FilePath     : \rollback_netcode\include\core\interfaces\base/i_base.h
 * @Description  :
 * Copyright (c) 2022 by Daylily-Zeleen email: daylily-zeleen@foxmail.com, All Rights Reserved.
 */
#pragma once

#include <concepts>
#include <memory>

#include <def.h>

namespace rollback_netcode {
class IBase;
template <typename TBase>
	requires std::derived_from<TBase, IBase> || std::derived_from<TBase, const IBase>
class SharedPtr;

template <class Ty, class... Types>
_NODISCARD_SMART_PTR_ALLOC
#if _HAS_CXX20
		std::enable_if_t<!std::is_array_v<Ty>, SharedPtr<Ty>>
#else // _HAS_CXX20
		SharedPtr<Ty>
#endif // _HAS_CXX20
		generate_shared(Types &&...Args) { // make a shared_ptr to non-array object
	const auto Rx = new std::_Ref_count_obj2<Ty>(_STD forward<Types>(Args)...);
	SharedPtr<Ty> Ret;
	Ret._Set_ptr_rep_and_enable_shared(_STD addressof(Rx->_Storage._Value), Rx);
	return Ret;
}

template <typename TBase>
	requires std::derived_from<TBase, IBase> || std::derived_from<TBase, const IBase>
class SharedPtr : public std::shared_ptr<TBase> {
public:
	~SharedPtr() noexcept { // release resource
		if (use_count() == 1) {
			if ((*this)->is_managed_by_rollback_netcode()) {
				this->_Decref();
			} else {
				(*this)->release_from_netcode();
			}
		} else {
			this->_Decref();
		}
	}

	SharedPtr(std::shared_ptr<TBase> &p) {
		(*this) = p;
	}

	SharedPtr(decltype(nullptr)) :
			std::shared_ptr<TBase>(nullptr) {}

	operator SharedPtr<const TBase>() const {
		return SharedPtr<const TBase>(*this);
	}
};

class IBase : virtual public std::enable_shared_from_this<IBase> {
public:
	virtual bool release_from_netcode() = 0;
	virtual bool is_managed_by_rollback_netcode() const = 0;

	template <typename TDerived>
		requires std::derived_from<TDerived, IBase> || std::convertible_to<TDerived, IBase> || std::same_as<TDerived, IBase>
	SharedPtr<TDerived> get_shared_prt() {
		return SharedPtr<TDerived>(shared_from_this());
	}

	template <typename TDerived>
		requires std::derived_from<TDerived, IBase> || std::convertible_to<TDerived, IBase> || std::same_as<TDerived, IBase>
	SharedPtr<const TDerived> get_shared_prt() const {
		return SharedPtr<const TDerived>(shared_from_this());
	}
};

class ISyncObject : virtual public IBase {
public:
	virtual UUID_t get_uuid() const = 0;
    virtual bool is_valid() const = 0;
};

} // namespace rollback_netcode