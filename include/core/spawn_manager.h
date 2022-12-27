/*
 * @Author       : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @Date         : 2022-12-08 11:13:47
 * @LastEditors  : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @LastEditTime : 2022-12-15 21:34:40
 * @FilePath     : \rollback_netcode\include\core\interfaces\i_spawn_manager.h
 * @Description  :
 * Copyright (c) 2022 by Daylily-Zeleen email: daylily-zeleen@foxmail.com, All Rights Reserved.
 */
#pragma once

#include <core/event.h>
#include <deque>
#include <hashfuncs.h>
#include <interfaces/i_network_rollbackable.h>
#include <interfaces/i_peer.h>

#include "interfaces/i_spawnable.h"
#include "interfaces/i_spawner.h"

#include <functional>
#include <utility>

namespace rollback_netcode {

class SpawnManager {
public:
    static const UUID_t UUID = INT64_MIN;

    Event<std::function<void(const SharedPtr<ISpawnable> &p_spawned_obj, const SharedPtr<const ISpawner> &p_spawner, const SharedPtr<const ISpawnData> &p_spawned_data)>> event_spawned;
    Event<std::function<void(const SharedPtr<ISpawnable> &p_despawned_obj)>> event_predespawn;

    void reset();

    template < typename TSpawner, typename TSpawnData>
        requires std::derived_from<TSpawner, ISpawner> && std::derived_from<TSpawnData, ISpawnData>
    FORCE_INLINE_ SharedPtr<ISpawnable> spawn(const SharedPtr<const TSpawner> &p_spawner, const SharedPtr<TSpawnData> &p_spawn_data) {
        SharedPtr<ISpawnable> ret = _do_spawn(p_spawner);
        ret->network_spawn_preprocess(p_spawn_data);
        ret->network_spawn(p_spawn_data);

        SpawnRecord spawn_record(p_spawn_data, p_spawner);
        spawned_records.insert_or_assign(ret->get_uuid(), spawn_record);
        spawned_objects.insert_or_assign(ret->get_uuid(), ret);

        event_spawned.invoke(ret, p_spawner, p_spawn_data);
        return ret;
    }

    // SharedPtr<ISpawnable> spawn(const SharedPtr<const ISpawner> &p_spawner, const SharedPtr<ISpawnData> &p_spawn_data);
    void despawn(const SharedPtr<ISpawnable> &p_to_despawn);

    void set_reuse_desoawned_objects(const bool p_reuse_desoawned_objects);

    SharedPtr<const IData> save_state();
    void load_state(const SharedPtr<const IData> &p_state);

    [[nodiscard]] bool is_reuse_desoawned_objects() const { return reuse_desoawned_objects; }
    [[nodiscard]] bool is_respawning() const { return respawning; }

protected:
    struct SpawnRecord {
        SharedPtr<const ISpawnData> spawn_data = nullptr;
        SharedPtr<const ISpawner> spawner = nullptr;
        SpawnRecord() = default;
        SpawnRecord(const SharedPtr<const ISpawnData> &p_spawn_data, const SharedPtr<const ISpawner> &spawner) : spawn_data(p_spawn_data), spawner(spawner) {}

        [[nodiscard]] JsonObj to_json_obj() const {
            JsonObj ret;
            ret << std::string("spawn_data") << spawn_data->to_json_obj();
            ret << std::string("spawner") << spawner->to_json_obj();
            return ret;
        }
        [[nodiscard]] Hash_t get_hash() const {
            Hash_t h = hash_murmur3_one_32(UINT32_MAX - 3);
            h = hash_murmur3_one_32(spawn_data->get_hash(), h);
            h = hash_murmur3_one_32(spawner->get_hash(), h);
            return hash_fmix32(h);
        }
    };

    class SpawnManagerState : public IData {
    public:
        std::map<UUID_t, SpawnRecord> spawned_records;

        void release_from_netcode() override {}

        bool is_managed_by_rollback_netcode() const override { return true; }

        JsonObj to_json_obj() const override {
            JsonObj j_spawn_records, ret;
            for (auto &kv : spawned_records) {
                j_spawn_records << kv.first << kv.second.to_json_obj();
            }
            ret << std::string("spawn_records") << j_spawn_records;
            return ret;
        }

        Hash_t get_hash() const override {
            Hash_t h = hash_murmur3_one_32(UINT32_MAX - 1);
            h = hash_murmur3_one_32(UINT32_MAX - 2, h);
            for (auto &kv : spawned_records) {
                h = hash_murmur3_one_32(kv.first, h);
                h = hash_murmur3_one_32(kv.second.get_hash(), h);
            }
            return hash_fmix32(h);
        }

        SpawnManagerState(std::map<UUID_t, SpawnRecord> p_spawned_records) : spawned_records(std::move(p_spawned_records)) {}
    };

    std::map<SharedPtr<const ISpawner>, std::deque<SharedPtr<ISpawnable>>> retired_objects;
    std::map<UUID_t, SharedPtr<ISpawnable>> spawned_objects;
    std::map<UUID_t, SpawnRecord> spawned_records;

    bool respawning = false;
    bool reuse_desoawned_objects = false;

    void _do_despawn(const SharedPtr<ISpawnable> &p_obj, const UUID_t &p_uuid);
    
    SharedPtr<ISpawnable> _do_spawn(const SharedPtr<const ISpawner> &p_spawner);
};

} // namespace rollback_netcode

namespace std {
inline std::string to_string(const rollback_netcode::ISpawnable *p) { return p->to_json_obj().json(); }
inline std::string to_string(const rollback_netcode::ISpawnData *p) { return p->to_json_obj().json(); }
} // namespace std