/*
 * @Author       : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @Date         : 2022-12-07 16:00:30
 * @LastEditors  : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @LastEditTime : 2022-12-15 18:31:19
 * @FilePath     : \rollback_netcode\include\core\interfaces\i_sync_manager.h
 * @Description  :
 * Copyright (c) 2022 by Daylily-Zeleen email: daylily-zeleen@foxmail.com, All Rights Reserved.
 */

#pragma once

#include "core/sound_manager.h"
#include "core/spawn_manager.h"
#include "interfaces/i_network_adapeor.h"
#include "interfaces/i_sound.h"
#include "interfaces/i_spawn_data.h"
#include "interfaces/i_spawnable.h"
#include "interfaces/i_spawner.h"
#include <concepts>
#include <core/logger.h>
#include <core/state_hash_frame.h>
#include <deque>
#include <interfaces/i_data_serializer.h>
#include <interfaces/i_network_proccessable.h>
#include <interfaces/i_peer.h>
#include <interfaces/i_timer.h>

#include "../../thirdparty/deterministic_float/glacier_float.h"
#include <memory>
#include <vector>

namespace rollback_netcode {
class InputBufferFrame;
class INetwokProcessable;
class INetworkRollbackable;

class ISyncManager : virtual public IBase {
public:
    Event<std::function<void()>> event_sync_started;
    Event<std::function<void()>> event_sync_stopped;
    Event<std::function<void()>> event_sync_lost;
    Event<std::function<void()>> event_sync_regained;
    Event<std::function<void(const std::string &p_msg)>> event_sync_error;

    Event<std::function<void(const Tick_t p_count)>> event_skip_ticks_flagged;
    Event<std::function<void(const Tick_t p_tick)>> event_rollback_flagged;
    Event<std::function<void(const Tick_t p_tick, const PeerId_t &p_peer_id, const SharedPtr<const IData> &p_local_input, const SharedPtr<const IData> &p_remote_input)>> event_prediction_missed;
    Event<std::function<void(const Tick_t p_tick, const PeerId_t &p_peer_id, const Hash_t &local_hash, const Hash_t &remote_hash)>> event_remote_state_mismatch;

    Event<std::function<void(const PeerId_t &p_peer_id)>> event_peer_added;
    Event<std::function<void(const PeerId_t &p_peer_id)>> event_peer_removed;
    Event<std::function<void(const SharedPtr<IPeer> &p_peer)>> event_peer_pinged_back;

    Event<std::function<void(const Tick_t p_rollback_ticks)>> event_state_loaded;
    Event<std::function<void(const bool p_rollback)>> event_tick_finished;
    Event<std::function<void(const Tick_t p_tick)>> event_tick_retired;
    Event<std::function<void(const Tick_t p_tick)>> event_tick_input_completed;

    Event<std::function<void(const SharedPtr<ISpawnable> &p_spawned_obj, const SharedPtr<const ISpawner> &p_spawner, const SharedPtr<const ISpawnData> &p_spawned_data)>> event_spawned;
    Event<std::function<void(const SharedPtr<ISpawnable> &p_despawned_obj)>> event_predespawn;
    Event<std::function<void()>> event_interpolation_frame;

    ISyncManager();
    void setup(const SharedPtr<INetworkAdaptor> &p_network_adaptor, const SharedPtr<IDataSerializer> &p_data_adaptor, const SharedPtr<ITimer> &p_ping_timer);

    void set_mechanized(const bool p_mechanized);
    void set_input_delay(const uint8_t &p_input_delay);
    void set_ping_interval(const float &p_ping_interval);
    void set_tick_per_second(const uint8_t &p_tick_per_second) { tick_per_second = p_tick_per_second; }

    void set_max_messages_at_once(const uint8_t p_max_messages_at_once);

    Tick_t get_input_tick() const { return input_tick; }
    Tick_t get_current_tick() const { return current_tick; }
    uint32_t get_skip_ticks() const { return skip_ticks; }
    uint32_t get_rollback_ticks() const { return rollback_ticks; }
    Tick_t get_requested_input_complete_tick() const { return requested_input_complete_tick; }
    bool is_started() const { return started; }

    float get_tick_time_float() const { return tick_time_float; }
    GFloat get_tick_time_deterministic() const { return tick_time_deterministic; }

    virtual ~ISyncManager() { stop_logging(); }

    void start_logging(const std::string &p_log_file_path, const SharedPtr<const IData> &p_match_info);

    void stop_logging();

    void add_peer(const SharedPtr<IPeer> &p_peer);

    void remove_peer(const PeerId_t &p_peer_id);
    bool has_peer(const PeerId_t &p_peer_id) const { return peers.find(p_peer_id) != peers.end(); }
    SharedPtr<IPeer> get_peer(const PeerId_t &p_peer_id) const;

    void start(const SharedPtr<ITimer> &p_delay_timer);
    void stop();

    const SharedPtr<const IData> get_latest_input_from_peer(const PeerId_t &p_peer_id) const;

    void process_logic_tick();
    void process_frame(const float delta);

    void reset_mechanized_data();
    void execute_mechanized_tick();
    void execute_mechanized_interpolation_frame(float delta);
    void execute_mechanized_interframe();
    void process_mechanized_input();

    // FORCE_INLINE_ SharedPtr<ISpawnable> spawn(const SharedPtr<const ISpawner> &p_spawner, const SharedPtr<ISpawnData> &p_spawn_data);

    template <typename TSpawner, typename TSpawnData>
        requires std::derived_from<TSpawner, ISpawner> && std::derived_from<TSpawnData, ISpawnData>
    FORCE_INLINE_ SharedPtr<ISpawnable>
    spawn(const SharedPtr<const TSpawner> &p_spawner,
          const SharedPtr<TSpawnData> &p_spawn_data) // name: String, parent: Node, scene: PackedScene, data: Dictionary = {}, rename: bool = true, signal_name: String = '')
    {
        if (!started) {
            PUSH_ERR(std::format("Refusing to spawn {} before RollbackNetworkSyncManager has started", std::to_string(p_spawn_data).c_str()));
            return nullptr;
        }

        return spawn_manager.spawn<TSpawner, TSpawnData>(p_spawner, p_spawn_data);
    }
    FORCE_INLINE_ void despawn(const SharedPtr<ISpawnable> &p_despawnable) { spawn_manager.despawn(p_despawnable); }

    FORCE_INLINE_ bool is_in_rollback() const { return _in_rollback; }
    FORCE_INLINE_ bool is_respawning() const { return spawn_manager.is_respawning(); }

    FORCE_INLINE_ void play_sound(const SoundIdentifier_t &p_identifier, const SharedPtr<ISound> &p_sound) { sound_manager.play_sound(p_identifier, p_sound); }

    FORCE_INLINE_ bool ensure_current_tick_input_complete();

    template <typename TSyncManager>
        requires std::derived_from<TSyncManager, ISyncManager>
    static TSyncManager *get_singleton() {
        return reinterpret_cast<TSyncManager *>(singleton);
    }
    static ISyncManager *get_singleton() { return singleton; }

protected:
    FORCE_INLINE_ SharedPtr<const IData> call_get_local_input();
    FORCE_INLINE_ void call_network_process(const InputBufferFrame &p_input_buffer_frame);
    virtual void call_save_state(std::map<UUID_t, SharedPtr<const IData>> &r_saved_state_frame);
    virtual void call_load_state(const std::map<UUID_t, SharedPtr<const IData>> &p_state_frame);
    FORCE_INLINE_ void call_interpolate_state(const float p_weight);
    FORCE_INLINE_ void save_current_state();
    // TODO:: I think this method is worth to be inline.
    void update_input_complete_tick();
    FORCE_INLINE_ void update_state_hashes();
    FORCE_INLINE_ std::shared_ptr<InputBufferFrame> get_input_frame(const Tick_t &p_tick) const;
    FORCE_INLINE_ std::shared_ptr<StateBufferFrame> get_state_frame(const Tick_t &p_tick) const;

    void clear_peers();

    void handle_fatal_error(const std::string &msg);
    // events
    FORCE_INLINE_ void on_ping_timer_timeout();
    FORCE_INLINE_ void on_spawned(const SharedPtr<ISpawnable> &p_spawned_obj, const SharedPtr<const ISpawner> &p_spawner, const SharedPtr<const ISpawnData> &p_spawned_data);
    FORCE_INLINE_ void on_predespawn(const SharedPtr<ISpawnable> &p_despawned_obj);
    FORCE_INLINE_ void on_ping_received(const PeerId_t &p_peer_id, PingMessage &p_peer_msg);
    FORCE_INLINE_ void on_ping_back_received(const PeerId_t &p_peer_id, PingMessage &p_peer_msg);

    FORCE_INLINE_ void on_remote_start_received();
    FORCE_INLINE_ void on_remote_stop_received();
    FORCE_INLINE_ void on_input_tick_received(const PeerId_t &p_sender_peer_id, const SerializedData &p_serialized_data);

protected:
    SharedPtr<INetworkAdaptor> network_adaptor = nullptr;
    SharedPtr<IDataSerializer> data_adaptor = nullptr;
    friend class Logger;

    std::map<PeerId_t, SharedPtr<IPeer>> peers;

    std::deque<std::shared_ptr<InputBufferFrame>> input_buffer;
    std::deque<std::shared_ptr<StateBufferFrame>> state_buffer;
    std::deque<std::shared_ptr<StateHashFrame>> state_hashes;

    bool mechanized = false;
    std::map<PeerId_t, std::map<Tick_t, InputBufferFrame::PeerInput>> mechanized_input_received;
    uint8_t mechanized_rollback_ticks = 0;

    uint32_t max_buffer_size = 20;
    uint32_t ticks_to_calculate_advantage = 60;
    uint8_t input_delay = 2;
    uint8_t max_input_frames_per_message = 5;
    uint8_t max_messages_at_once = 2; //  通过 setter 设置
    uint32_t max_ticks_to_regain_sync = 300;
    uint32_t min_lag_to_regain_sync = 5;
    bool interpolation = false;
    uint8_t max_state_mismatch_count = 10;

    uint8_t tick_per_second = 10;
    float tick_time_float = 0.0f;
    GFloat tick_time_deterministic = GFloat::Zero();

    static ISyncManager *singleton;

#ifdef LOG_ENABLED
    std::unique_ptr<Logger> logger;
#endif

#ifdef DEBUG_ENABLED
    uint8_t debug_rollback_ticks = 0;
    uint8_t debug_random_rollback_ticks = 0;
    uint8_t debug_skip_nth_message = 0;
    float debug_physics_process_msecs = 10.0;
    float debug_process_msecs = 10.0;
    bool debug_check_message_serializer_roundtrip = false;
    bool debug_check_local_state_consistency = false;
    uint32_t debug_message_bytes = 700;
    std::deque<std::shared_ptr<rollback_netcode::StateBufferFrame>> debug_check_local_state_consistency_buffer;
#endif
    // In seconds, because we don't want it to be dependent on the network tick.
    float ping_interval = 1.0;

    // readonly
    Tick_t input_tick = 0;
    Tick_t current_tick = 0;
    uint32_t skip_ticks = 0;
    uint8_t rollback_ticks = 0;
    Tick_t requested_input_complete_tick = 0;
    bool started = false;

    // hidden
    bool _host_starting = false;
    SharedPtr<ITimer> ping_timer = nullptr;
    SharedPtr<ITimer> start_delay_timer = nullptr;
    int start_delay_timer_connect_key = 0;
    SpawnManager spawn_manager;
    SoundManager sound_manager;
    Tick_t _input_buffer_start_tick;
    Tick_t _state_buffer_start_tick;
    Tick_t _state_hashes_start_tick;
    std::deque<SerializedData> _input_send_queue;
    Tick_t _input_send_queue_start_tick;
    uint32_t _ticks_spent_regaining_sync;
    std::map<UUID_t, std::pair<SharedPtr<const IData>, SharedPtr<const IData>>> _interpolation_state;
    float _time_since_last_tick;
    uint32_t _debug_skip_nth_message_counter;
    Tick_t _input_complete_tick;
    Tick_t _state_complete_tick;
    Tick_t _last_state_hashed_tick;
    uint32_t _state_mismatch_count;
    bool _in_rollback;
    bool _ran_physics_process;
    uint32_t _ticks_since_last_interpolation_frame;

    std::map<UUID_t, INetwokProcessable *> network_processables;
    std::map<UUID_t, INetworkRollbackable *> network_rollbackables;

    std::vector<PeerId_t> _missing_peer_ids;

    uint8_t _new_messages = 2;
    uint8_t _old_messages = 2;

    void _reset();
    FORCE_INLINE_ std::shared_ptr<InputBufferFrame> predict_missing_input(std::shared_ptr<InputBufferFrame> &p_input_frame, std::shared_ptr<InputBufferFrame> &p_previous_frame);

    FORCE_INLINE_ bool do_tick(const bool rollback = false);

    std::shared_ptr<InputBufferFrame> get_or_create_input_frame(const Tick_t p_tick);

    bool cleanup_buffers();

    FORCE_INLINE_ std::shared_ptr<StateHashFrame> get_state_hash_frame(const Tick_t p_tick) const;

    FORCE_INLINE_ bool is_current_tick_input_complete() const { return current_tick <= _input_complete_tick; }

    void get_input_messages_from_send_queue_in_range(const Tick_t p_first_index, const Tick_t p_last_index, std::vector<std::map<Tick_t, SerializedData>> &r_input_messages_from_send_queue,
                                                     const bool p_reverse = false) const;

    FORCE_INLINE_ void get_input_messages_from_send_queue_for_peer(const IPeer &p_peer, std::vector<std::map<Tick_t, SerializedData>> &r_input_messages_from_send_queue) const;

    FORCE_INLINE_ void get_state_hashes_for_peer(const SharedPtr<IPeer> &p_peer, std::map<Tick_t, Hash_t> &r_state_hashes_for_peer) const;

    FORCE_INLINE_ void record_advantage(const bool force_calculate_advantage = false) const;

    FORCE_INLINE_ bool calculate_skip_ticks();

    FORCE_INLINE_ uint8_t calculate_max_local_lag() const;

    FORCE_INLINE_ Tick_t calculate_minimum_next_input_tick_requested() const;

    void send_input_messages_to_peer(const PeerId_t &peer_id);

    FORCE_INLINE_ void send_input_messages_to_all_peers();

#ifdef DEBUG_ENABLED
    void debug_check_consistent_local_state(const std::shared_ptr<StateBufferFrame> &p_state, const std::string &message = "Loaded");
#endif

    friend INetwokProcessable;
    friend INetworkRollbackable;
    friend StateBufferFrame;
    friend InputBufferFrame;
};

SharedPtr<const IData> ISyncManager::call_get_local_input() { return network_adaptor->get_network_peer()->get_local_input(); }

void ISyncManager::call_network_process(const InputBufferFrame &p_input_buffer_frame) {
    for (auto it = network_processables.begin(); it != network_processables.end(); it++) {
        auto obj = it->second;
        if (obj) {
            if (obj->is_valid()) {
                auto input = p_input_buffer_frame.get_peer_input(obj->get_network_master_peer_id());
                if (input) {
                    obj->network_preprocess(input);
                } else {
                    obj->network_preprocess_without_input();
                }
            }
        } else {
            auto to_remove = it;
            network_processables.erase(to_remove);
            it++;
            if (it == network_processables.end())
                break;
        }
    }
    for (auto &kv : network_processables) {
        if (kv.second->is_valid()) {
            auto input = p_input_buffer_frame.get_peer_input(kv.second->get_network_master_peer_id());
            if (input) {
                kv.second->network_process(input);
            } else {
                kv.second->network_process_without_input();
            }
        }
    }
    for (auto &kv : network_processables) {
        if (kv.second->is_valid()) {
            auto input = p_input_buffer_frame.get_peer_input(kv.second->get_network_master_peer_id());
            if (input) {
                kv.second->network_postprocess(input);
            } else {
                kv.second->network_postprocess_without_input();
            }
        }
    }
}

void ISyncManager::call_interpolate_state(const float p_weight) {
    for (auto &kv : _interpolation_state) {
        ERR_THROW(network_rollbackables.count(kv.first) <= 0, std::bad_exception());
        network_rollbackables[kv.first]->interpolate_state(kv.second.first, kv.second.second, p_weight);
    }
}

void ISyncManager::save_current_state() {
    ERR_THROW(current_tick < 0, "Attempting to store state for negative tick");
    std::map<UUID_t, SharedPtr<const IData>> current_state;
    this->call_save_state(current_state);
    state_buffer.emplace_back(std::make_shared<StateBufferFrame>(current_tick, current_state));

    // If the input for this state is complete, then update _state_complete_tick.
    if (_input_complete_tick > _state_complete_tick) {
        // Set to the current_tick so long as its less than or equal to the
        // _input_complete_tick, otherwise, cap it to the _input_complete_tick.
        _state_complete_tick = current_tick <= _input_complete_tick ? current_tick : _input_complete_tick;
    }
}

void ISyncManager::update_state_hashes() {
    while (_state_complete_tick > _last_state_hashed_tick) {
        auto state_frame = get_state_frame(_last_state_hashed_tick + 1);
        if (state_frame == nullptr) {
            handle_fatal_error("Unable to hash state");
            return;
        }

        _last_state_hashed_tick += 1;

        const Hash_t state_hash = state_frame->get_data_hash();
        state_hashes.emplace_back(std::make_shared<StateHashFrame>(_last_state_hashed_tick, state_hash));
#ifdef LOG_ENABLED
        if (logger)
            logger->write_state(*state_frame);
#endif
    }
}

std::shared_ptr<InputBufferFrame> ISyncManager::get_input_frame(const Tick_t &p_tick) const {
    if (p_tick < _input_buffer_start_tick) {
        return nullptr;
    }
    Tick_t index = p_tick - _input_buffer_start_tick;
    if (index >= input_buffer.size()) {
        return nullptr;
    }
    auto input_frame = input_buffer[index];
    ERR_THROW(input_frame->tick != p_tick, "Input frame retreived from input buffer has mismatched tick number");
    return input_frame;
}
std::shared_ptr<StateBufferFrame> ISyncManager::get_state_frame(const Tick_t &p_tick) const {
    if (p_tick < _state_buffer_start_tick)
        return nullptr;
    Tick_t index = p_tick - _state_buffer_start_tick;
    if (index >= state_buffer.size())
        return nullptr;
    auto state_frame = state_buffer[index];
    ERR_THROW(state_frame->tick != p_tick, "State frame retreived from state buffer has mismatched tick number");
    return state_frame;
}

std::shared_ptr<InputBufferFrame> ISyncManager::predict_missing_input(std::shared_ptr<InputBufferFrame> &p_input_frame, std::shared_ptr<InputBufferFrame> &p_previous_frame) {
    ERR_THROW(p_input_frame == nullptr, std::bad_exception());
    if (!p_input_frame->is_complete(peers)) {
        if (p_previous_frame == nullptr) {
            p_previous_frame = std::make_shared<InputBufferFrame>(-1);
        }

        p_input_frame->get_missing_peers(peers, _missing_peer_ids);
        for (const PeerId_t &peer_id : _missing_peer_ids) {
            auto peer = peers[peer_id];
            Tick_t missing_peers_ticks_since_real_input = peer->last_received_remote_input_tick == 0 ? -1 : current_tick - peer->last_received_remote_input_tick;
            auto predicted_input = peer->predict_remote_input(p_previous_frame->get_peer_input(peer_id), missing_peers_ticks_since_real_input);
            p_input_frame->add_peer_input(peer_id, predicted_input, true);
        }
    }
    return p_input_frame;
}

bool ISyncManager::do_tick(const bool rollback) {
    auto input_frame = get_input_frame(current_tick);
    auto previous_input_frame = get_input_frame(current_tick - 1);

    ERR_THROW(input_frame == nullptr, "Input frame for current_tick is null");
    input_frame = predict_missing_input(input_frame, previous_input_frame);

    call_network_process(*input_frame);
    // If the game was stopped during the last network process, then we return
    // false here, to indicate that a full tick didn't complete and we need to abort.
    if (!started)
        return false;

    save_current_state();

#ifdef DEBUG_ENABLED
    // Debug check that states computed multiple times with complete inputs are the same
    if (_last_state_hashed_tick >= current_tick) {
        debug_check_consistent_local_state(state_buffer.back(), "Recomputed");
    }
#endif

    event_tick_finished.invoke(rollback);
    return true;
}

std::shared_ptr<StateHashFrame> ISyncManager::get_state_hash_frame(const Tick_t p_tick) const {
    if (p_tick < _state_hashes_start_tick)
        return nullptr;
    Tick_t index = p_tick - _state_hashes_start_tick;
    if (index >= state_hashes.size())
        return nullptr;
    ERR_THROW(state_hashes[index]->tick != p_tick, "State hash frame retreived from state hashes has mismatched tick number");
    return state_hashes[index];
}

void ISyncManager::get_input_messages_from_send_queue_for_peer(const IPeer &p_peer, std::vector<std::map<Tick_t, SerializedData>> &r_input_messages_from_send_queue) const {
    Tick_t first_index = p_peer.next_requested_local_input_tick - _input_send_queue_start_tick;
    Tick_t last_index = Tick_t(_input_send_queue.size()) - 1;
    uint32_t max_messages = (max_input_frames_per_message * max_messages_at_once);
    if ((last_index + 1) - first_index <= max_messages) {
        get_input_messages_from_send_queue_in_range(first_index, last_index, r_input_messages_from_send_queue, true);
        return;
    }

    get_input_messages_from_send_queue_in_range(last_index - (_new_messages * max_input_frames_per_message) + 1, last_index, r_input_messages_from_send_queue, true);
    get_input_messages_from_send_queue_in_range(first_index, first_index + (_old_messages * max_input_frames_per_message) - 1, r_input_messages_from_send_queue);
}

void ISyncManager::get_state_hashes_for_peer(const SharedPtr<IPeer> &p_peer, std::map<Tick_t, Hash_t> &r_state_hashes_for_peer) const {
    if (p_peer->next_requested_local_hash_tick >= _state_hashes_start_tick) {
        Tick_t index = p_peer->next_requested_local_hash_tick - _state_hashes_start_tick;
        while (index < state_hashes.size()) {
            auto state_hash_frame = state_hashes[index];
            ERR_THROW(state_hash_frame == nullptr, std::bad_exception());
            r_state_hashes_for_peer.emplace(state_hash_frame->tick, state_hash_frame->state_hash);
            index++;
        }
    }
}

void ISyncManager::record_advantage(const bool force_calculate_advantage) const {
    for (auto &kv : peers) {
        auto peer = kv.second;
        // Number of frames we are predicting for this peer.
        peer->local_lag = (input_tick + 1) - peer->last_received_remote_input_tick;
        // Calculate the advantage the peer has over us.
        peer->record_advantage(!force_calculate_advantage ? ticks_to_calculate_advantage : 0);
#ifdef LOG_ENABLED
        if (logger)
            logger->add_peer_statues(peer);
#endif
    }
}
bool ISyncManager::ensure_current_tick_input_complete() {
    if (is_current_tick_input_complete())
        return true;
    if (requested_input_complete_tick == 0 || requested_input_complete_tick > current_tick)
        requested_input_complete_tick = current_tick;
    return false;
}
bool ISyncManager::calculate_skip_ticks() {
    // Attempt to find the greatest advantage.
    uint8_t max_advantage = 0.0f; // 完全使用整数代替是否可行
    for (auto &kv : peers) {
        if (kv.second->calculated_advantage > max_advantage) {
            max_advantage = kv.second->calculated_advantage;
        }
    }

    if (max_advantage >= 2 && skip_ticks == 0) {
        skip_ticks = max_advantage / 2;
        event_skip_ticks_flagged.invoke(skip_ticks);
        return true;
    }
    return false;
}
void ISyncManager::send_input_messages_to_all_peers() {
#ifdef DEBUG_ENABLED
    if (debug_skip_nth_message > 1) {
        _debug_skip_nth_message_counter += 1;
        if (_debug_skip_nth_message_counter >= debug_skip_nth_message) {
            HINT(std::format("[{}] Skipping message to simulate packet loss", std::to_string(current_tick)));
            _debug_skip_nth_message_counter = 0;
            return;
        }
    }
#endif
    for (auto &kv : peers)
        send_input_messages_to_peer(kv.first);
}
uint8_t ISyncManager::calculate_max_local_lag() const {
    uint8_t max_lag = 0;
    for (auto &kv : peers) {
        if (kv.second->local_lag > max_lag) {
            max_lag = kv.second->local_lag;
        }
    }
    return max_lag;
}

Tick_t ISyncManager::calculate_minimum_next_input_tick_requested() const {
    if (peers.size() == 0)
        return 1;
    Tick_t minimum_next_input_tick_requested = UINT32_MAX;
    for (auto &kv : peers) {
        if (kv.second->next_requested_local_input_tick < minimum_next_input_tick_requested) {
            minimum_next_input_tick_requested = kv.second->next_requested_local_input_tick;
        }
    }
    return minimum_next_input_tick_requested;
}

// CALLBACK
void ISyncManager::on_ping_timer_timeout() {
    if (peers.size() == 0)
        return;
    for (auto &&pair : peers) {
        ERR_THROW(pair.first == network_adaptor->get_network_peer_id(), "Cannot ping ourselves");
        network_adaptor->send_ping(pair.first, PingMessage());
    }
}
void ISyncManager::on_spawned(const SharedPtr<ISpawnable> &p_spawned_obj, const SharedPtr<const ISpawner> &p_spawner, const SharedPtr<const ISpawnData> &p_spawned_data) {
    event_spawned.invoke(p_spawned_obj, p_spawner, p_spawned_data);
}
void ISyncManager::on_predespawn(const SharedPtr<ISpawnable> &p_despawned_obj) { event_predespawn.invoke(p_despawned_obj); }
void ISyncManager::on_ping_received(const PeerId_t &p_peer_id, PingMessage &p_peer_msg) {
    ERR_THROW(p_peer_id == network_adaptor->get_network_peer_id(), "Cannot ping back ourselves");
    p_peer_msg.set_remote_time();
    network_adaptor->send_ping_back(p_peer_id, p_peer_msg);
}
void ISyncManager::on_ping_back_received(const PeerId_t &p_peer_id, PingMessage &p_peer_msg) {
    auto peer = peers[p_peer_id];
    auto current = Utils::get_current_system_time_msec();
    peer->last_received_ping = current;
    peer->rtt = current - p_peer_msg.local_time;
    peer->time_delta = float(p_peer_msg.remote_time - p_peer_msg.local_time) - (float(peer->rtt) * 0.5f);
    event_peer_pinged_back.invoke(peer);
}
void ISyncManager::on_remote_start_received() {
    _reset();
    tick_time_float = 1.0f / float(tick_per_second);
    tick_time_deterministic = GFloat::One() / (GFloat(tick_per_second));

    started = true;
    network_adaptor->start_network_adaptor(get_shared_prt<IBase>());
    spawn_manager.reset();
    event_sync_started.invoke();
}
void ISyncManager::on_remote_stop_received() {
    if (!started || _host_starting)
        return;

    network_adaptor->stop_network_adaptor(get_shared_prt<IBase>());
    started = false;
    _host_starting = false;
    _reset();

    for (auto &kv : peers) {
        kv.second->clear();
    }
    event_sync_stopped.invoke();
    spawn_manager.reset();
}

void ISyncManager::on_input_tick_received(const PeerId_t &p_sender_peer_id, const SerializedData &p_serialized_data) {
    if (!started)
        return;
    PeerFrameMessage msg;
    data_adaptor->deserialize_message(p_serialized_data, msg);

    std::vector<Tick_t> remote_ticks(msg.inputs.size());
    for (auto &kv : msg.inputs) {
        remote_ticks.emplace_back(kv.first);
    }
    std::sort(remote_ticks.begin(), remote_ticks.end());

    auto first_remote_tick = remote_ticks.front();
    auto last_remote_tick = remote_ticks.back();

    if (first_remote_tick >= input_tick + max_buffer_size) {
        // This either happens because we are really far behind (but maybe, just
        // maybe could catch up) or we are receiving old ticks from a previous
        // round that hadn't yet arrived. Just discard the message and hope for
        // the best, but if we can't keep up, another one of the fail safes will
        // detect that we are out of sync.
        HINT("Discarding message from the future");
        // We return because we don't even want to do the accounting that happens
        // after integrating input, since the data in this message could be
        // totally bunk (ie. if it's from a previous match).
        return;
    }

#ifdef LOG_ENABLED
    if (logger) {
        logger->begin_interframe();
    }
#endif
    auto peer = peers[p_sender_peer_id];
    // Only process if it contains ticks we haven't received yet.
    if (last_remote_tick > peer->last_received_remote_input_tick) {
        // Integrate the input we received into the input buffer.
        for (auto &remote_tick : remote_ticks) {
            // Skip ticks we already have.
            if (remote_tick <= peer->last_received_remote_input_tick)
                continue;
            // This means the input frame has already been retired, which can only
            // happen if we already had all the input.
            if (remote_tick < _input_buffer_start_tick)
                continue;
            auto remote_input = data_adaptor->deserialize_input(msg.inputs[remote_tick]);

            auto input_frame = get_or_create_input_frame(remote_tick);
            if (input_frame == nullptr) {
                // _get_or_create_input_frame() will have already flagged the error,
                // so we can just return here.
                return;
            }

            // If we already have non-predicted input for this peer, then skip it.
            if (!input_frame->is_peer_input_predicated(p_sender_peer_id))
                continue;

#ifdef LOG_ENABLED
            HINT(std::format("Received remote tick {} from {}", std::to_string(remote_tick), std::to_string(p_sender_peer_id)));
            if (logger)
                logger->add_value(std::format("remote_ticks_received_from_{}", std::to_string(p_sender_peer_id)), std::to_string(remote_tick));
#endif
            // If we received a tick in the past and we aren't already setup to
            // rollback earlier than that...
            auto tick_delta = current_tick - remote_tick;
            if (tick_delta >= 0 && rollback_ticks <= tick_delta) {
                // Grab our predicted input, and store the remote input.
                auto local_input = input_frame->get_peer_input(p_sender_peer_id);
                Hash_t predicted_hash = local_input->get_hash();
                input_frame->add_peer_input(p_sender_peer_id, remote_input, false);

                // Check if the remote input matches what we had predicted, if not,
                // flag that we need to rollback.
                if (predicted_hash != remote_input->get_hash()) {
                    rollback_ticks = tick_delta + 1;
                    event_prediction_missed.invoke(remote_tick, p_sender_peer_id, local_input, remote_input);
                    event_rollback_flagged.invoke(remote_tick);
                }
            } else {
                // Otherwise, just store it.
                input_frame->add_peer_input(p_sender_peer_id, remote_input, false);
            }
        }

        // Find what the last remote tick we received was after filling these in.
        auto index = (peer->last_received_remote_input_tick - _input_buffer_start_tick) + 1;
        while (index < input_buffer.size() && !input_buffer[index]->is_peer_input_predicated(p_sender_peer_id)) {
            peer->last_received_remote_input_tick += 1;
            index += 1;
        }

        // Update _input_complete_tick for new input.
        update_input_complete_tick();
    }

    // Record the next frame the other peer needs.
    if (msg.next_requested_input_tick > peer->next_requested_local_input_tick) {
        peer->next_requested_local_input_tick = msg.next_requested_input_tick;
    }

    // Number of frames the remote is predicting for us.
    peer->remote_lag = (peer->last_received_remote_input_tick + 1) - peer->next_requested_local_input_tick;

    // Process state hashes.
    for (auto &kv : msg.state_hashs) {
        const auto remote_tick = kv.first;
        auto state_hash_frame = get_state_hash_frame(remote_tick);
        if (state_hash_frame && !state_hash_frame->has_peer_hash(p_sender_peer_id)) {
            if (!state_hash_frame->record_peer_hash(p_sender_peer_id, msg.state_hashs[remote_tick])) {
                event_remote_state_mismatch.invoke(remote_tick, p_sender_peer_id, state_hash_frame->state_hash, msg.state_hashs[remote_tick]);
            }
        }
    }
    // Find what the last remote state hash we received was after filling these in.
    auto index = (peer->last_received_remote_hash_tick - _state_hashes_start_tick) + 1;
    while (index < state_hashes.size() && state_hashes[index]->has_peer_hash(p_sender_peer_id)) {
        peer->last_received_remote_hash_tick += 1;
        index += 1;
    }
    // Record the next state hash that the other peer needs.
    if (msg.next_hash_input_tick > peer->next_requested_local_hash_tick) {
        peer->next_requested_local_hash_tick = msg.next_hash_input_tick;
    }
}

} // namespace rollback_netcode