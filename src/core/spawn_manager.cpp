/*
 * @Author       : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @Date         : 2022-12-08 11:13:47
 * @LastEditors  : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @LastEditTime : 2022-12-15 22:47:58
 * @FilePath     : \rollback_netcode\src\core\i_spawn_manager.cpp
 * @Description  :
 * Copyright (c) 2022 by Daylily-Zeleen email: daylily-zeleen@foxmail.com, All Rights Reserved.
 */
#include "interfaces/base/i_base.h"
#include "interfaces/i_spawnable.h"
#include <core/spawn_manager.h>
#include <deque>

namespace rollback_netcode {

void SpawnManager::reset() {
    spawned_records.clear();
    spawned_objects.clear();
    retired_objects.clear();
}

void SpawnManager::set_reuse_desoawned_objects(const bool p_reuse_desoawned_objects) {
    reuse_desoawned_objects = p_reuse_desoawned_objects;
    if (!reuse_desoawned_objects) {
        retired_objects.clear();
    }
}

SharedPtr<ISpawnable> SpawnManager::_do_spawn(const SharedPtr<const ISpawner> &p_spawner) {
    auto it = retired_objects.find(p_spawner);
    if (it != retired_objects.end()) {
        std::deque<SharedPtr<ISpawnable>> &objs = it->second;
        SharedPtr<ISpawnable> obj(nullptr);
        while (objs.size() > 0) {
            obj = objs.front();
            objs.pop_front();
            if (obj && obj->is_valid())
                break;
            else
                obj = nullptr;
        }
        if (objs.size() == 0) {
            retired_objects.erase(it);
        }
        if (obj) {
            // print ("Reusing %s" % resource_path)
            return obj;
        }
    }
    return p_spawner->spawn();
}
void SpawnManager::despawn(const SharedPtr<ISpawnable> &p_to_despawn) { _do_despawn(p_to_despawn, p_to_despawn->get_uuid()); }

void SpawnManager::_do_despawn(const SharedPtr<ISpawnable> &p_obj, const UUID_t &p_uuid) {
    event_predespawn.invoke(p_obj);
    p_obj->network_despawn();

    if (is_reuse_desoawned_objects() && spawned_records.find(p_uuid) != spawned_records.end() && p_obj->is_valid()) {
        auto record = spawned_records.at(p_uuid);
        if (retired_objects.find(record.spawner) == retired_objects.end()) {
            retired_objects.emplace(record.spawner, std::deque<SharedPtr<ISpawnable>>({p_obj}));
        } else {
            retired_objects[record.spawner].emplace_back(p_obj);
        }
    } else {
        // Do nothing, if not reuse, it will be erase from all containers and be deleted.
    }

    spawned_records.erase(p_uuid);
    spawned_objects.erase(p_uuid);
}

SharedPtr<const IData> SpawnManager::save_state() {
    for (auto it = spawned_objects.begin();;) {
        if (it != spawned_objects.end()) {
            if (it->second == nullptr) {
                ++it;
                auto to_rease = it;
                --to_rease;
                spawned_objects.erase(to_rease);
            }
        }
        if (it == spawned_objects.end()) {
            break;
        }
        auto obj = it->second;

        if (!obj->is_valid()) {
            spawned_records.erase(it->first);
            spawned_objects.erase(it->first);
        }
    }
    return std::make_shared<const SpawnManagerState>(spawned_records);
}

void SpawnManager::load_state(const SharedPtr<const IData> &p_state) {
    ERR_THROW(dynamic_cast<const SpawnManagerState *>(p_state.get()) == nullptr, "ERR type");
    auto state = static_cast<const SpawnManagerState *>(p_state.get());
    // copy
    spawned_records = state->spawned_records;

    std::vector<UUID_t> spawned_objects_uuid(spawned_objects.size());
    for (auto &kv : spawned_objects) {
        spawned_objects_uuid.emplace_back(kv.first);
    }
    // Remove nodes that aren't in the state we are loading.
    for (UUID_t uuid : spawned_objects_uuid) {
        if (spawned_records.find(uuid) == spawned_records.end()) {
            auto ptr = spawned_objects[uuid];
            _do_despawn(ptr, uuid);
            // print ("[LOAD %s] de-spawned: %s" % [RollbackNetworkSyncManager.current_tick, node_path])
        }
    }

    //  Spawn nodes that don't already exist.
    for (auto &kv : spawned_records) {
        auto uuid = kv.first;
        auto it = spawned_objects.find(uuid);
        if (it != spawned_objects.end()) {
            auto old_obj = it->second;
            if (old_obj->is_valid()) {
                spawned_objects.erase(uuid);
                spawned_records.erase(uuid);
            }
        }

        respawning = true;

        it = spawned_objects.find(uuid);
        if (it == spawned_objects.end()) {
            auto record = spawned_records[uuid];
            auto spawned_obj = _do_spawn(record.spawner);
            spawned_obj->network_spawn(record.spawn_data);
            event_spawned.invoke(spawned_obj, record.spawner, record.spawn_data);
        }

        respawning = false;
    }
}

} // namespace rollback_netcode