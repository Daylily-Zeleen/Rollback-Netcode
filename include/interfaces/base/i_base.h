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

class IBase : virtual public std::enable_shared_from_this<IBase> {
public:
    virtual void release_from_netcode() = 0;
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

template <typename TBase>
    requires std::derived_from<TBase, IBase> || std::derived_from<TBase, const IBase>
class SharedPtr : public std::shared_ptr<TBase> {
public:
    ~SharedPtr() { // release resource
        if (this->use_count() == 1) {
            if ((this->get())->is_managed_by_rollback_netcode()) {
                this->_Decref();
            } else {
                auto ptr = static_cast<const IBase *>(this->get());
                const_cast<IBase *>(ptr)->release_from_netcode();
            }
        } else {
            this->_Decref();
        }
    }
    // template <typename TDerived>
    //     requires std::derived_from<TDerived, TBase>
    // SharedPtr(std::shared_ptr<TDerived> &p) {
    //     // auto ptr = const_cast<TDerived *>(p.get());
    //     // (*this) = reinterpret_cast<TBase *>(ptr);
    //     (*this) = p.get();
    // }
    template <typename TDerived>
        requires std::derived_from<TDerived, TBase>
    SharedPtr(const std::shared_ptr<TDerived> &p) {
        (*this) = SharedPtr<TBase>(p);
    }

    SharedPtr(std::shared_ptr<TBase> &p) { (*this) = p; }
    // SharedPtr(const std::shared_ptr<TBase> &p) const { (*this) = p; }
    SharedPtr(TBase *p) { (*this) = p; }

    SharedPtr(decltype(nullptr)) : std::shared_ptr<TBase>(nullptr) {}
    SharedPtr() = default;

    operator SharedPtr<const TBase>() const { return SharedPtr<const TBase>(*this); }
};

class ISyncObject : virtual public IBase {
public:
    virtual UUID_t get_uuid() const = 0;
    virtual bool is_valid() const = 0;
};

} // namespace rollback_netcode