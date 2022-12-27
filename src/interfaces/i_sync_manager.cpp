#include "../../thirdparty/deterministic_float/glacier_float.h"
#include "core/input_buffer_frame.h"
#include "core/spawn_manager.h"
#include "interfaces/base/i_base.h"
#include "interfaces/i_data.h"
#include "interfaces/i_network_rollbackable.h"
#include <core/input_buffer_frame.h>
#include <interfaces/i_sync_manager.h>


#include <memory>
#include <numeric>

#ifdef DEBUG_ENABLED
#include <debug/debug_comparer.h>
#endif

namespace rollback_netcode {

ISyncManager *ISyncManager::singleton = nullptr;

template <typename TArr>
    requires requires(typename TArr::value_type e) {
                 requires std::ranges::forward_range<TArr>;
                 std::to_string(e);
             }
std::string join_arr(const TArr &p_arr, const std::string &p_seperator = ", ") {
    std::string ret = "";
    auto count = p_arr.size();
    for (auto it = p_arr.begin(); it != p_arr.end(); it++) {
        if (it != p_arr.begin())
            ret += p_seperator;
        ret += std::to_string(*it);
    }
    return ret;
}

template <typename TArr>
    requires requires(typename TArr::value_type e) {
                 requires std::ranges::forward_range<TArr>;
                 std::to_string(e);
             }
std::string arr_to_string(const TArr &p_arr) {
    return "[" + join_arr(p_arr) + "]";
}

ISyncManager::ISyncManager() {
    singleton = this;

    spawn_manager.event_spawned.connect(
        [this](auto &&PH1, auto &&PH2, auto &&PH3) { on_spawned(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2), std::forward<decltype(PH3)>(PH3)); });
    spawn_manager.event_predespawn.connect([this](auto &&PH1) { on_predespawn(std::forward<decltype(PH1)>(PH1)); });
}

void ISyncManager::setup(const SharedPtr<INetworkAdaptor> &p_network_adaptor, const SharedPtr<IDataSerializer> &p_data_adaptor, const SharedPtr<ITimer> &p_ping_timer) {
    //
    data_adaptor = p_data_adaptor;
    //
    network_adaptor = p_network_adaptor;
    network_adaptor->event_ping_received.connect([this](auto &&PH1, auto &&PH2) { on_ping_received(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2)); });
    network_adaptor->event_ping_back_received.connect([this](auto &&PH1, auto &&PH2) { on_ping_back_received(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2)); });
    network_adaptor->event_remote_start_received.connect([this] { on_remote_start_received(); });
    network_adaptor->event_remote_stop_received.connect([this] { on_remote_stop_received(); });
    network_adaptor->event_input_tick_received.connect([this](auto &&PH1, auto &&PH2) { on_input_tick_received(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2)); });
    //
    ping_timer = p_ping_timer;
    ping_timer->set_loop(true);
    ping_timer->set_wait_time(ping_interval);
    ping_timer->event_timeout.connect([this] { on_ping_timer_timeout(); });
    ping_timer->restart();
}

void ISyncManager::_reset() {
    input_tick = 0;
    current_tick = input_tick - input_delay;
    skip_ticks = 0;
    rollback_ticks = 0;
    state_hashes.clear();

    _input_buffer_start_tick = 1;
    _state_buffer_start_tick = 0;
    _state_hashes_start_tick = 1;

    while (!_input_send_queue.empty()) {
        _input_send_queue.pop_front();
    }
    _input_send_queue_start_tick = 1;
    _ticks_spent_regaining_sync = 0;
    _interpolation_state.clear();
    _time_since_last_tick = 0.0;
    _debug_skip_nth_message_counter = 0;
    _input_complete_tick = 0;
    _state_complete_tick = 0;
    _last_state_hashed_tick = 0;
    _state_mismatch_count = 0;
    _in_rollback = false;
    _ran_physics_process = false;
    _ticks_since_last_interpolation_frame = 0;
}

void ISyncManager::set_mechanized(const bool p_mechanized) {
    ERR_THROW(started, "Changing the mechanized flag after RollbackNetworkSyncManager has started will probably break everything");

    mechanized = p_mechanized;
    ping_timer->set_paused(mechanized);
#ifdef LOG_ENABLED
    if (mechanized) {
        stop_logging();
    }
#endif
}

void ISyncManager::set_input_delay(const uint8_t &p_input_delay) {
    if (started)
        return;
    input_delay = p_input_delay;
}

void ISyncManager::set_ping_interval(const float &p_ping_interval) {
    ping_interval = p_ping_interval;
    ping_timer->set_wait_time(ping_interval);
}
void ISyncManager::set_max_messages_at_once(const uint8_t p_max_messages_at_once) {
    max_messages_at_once = p_max_messages_at_once;
    _new_messages = uint8_t(ceil(float(max_messages_at_once) / 2.0f));
    _old_messages = uint8_t(floor(float(max_messages_at_once) / 2.0f));
}
void ISyncManager::start_logging(const std::string &p_log_file_path, const SharedPtr<const IData> &p_match_info) {
#ifdef LOG_ENABLED
    if (mechanized)
        return;
    if (logger)
        logger = std::make_unique<Logger>();
    else
        logger->stop();

    if (!logger->start(p_log_file_path, network_adaptor->get_network_peer_id(), p_match_info))
        stop_logging();
#endif
}

void ISyncManager::stop_logging() {
#ifdef LOG_ENABLED
    if (logger) {
        logger->stop();
        logger.reset();
    }
#endif
}

void ISyncManager::add_peer(const SharedPtr<IPeer> &p_peer) {
    if (p_peer->peer_id < 0) {
        WARNING(("Can't add invalid peer with negetive \"peer_id\": " + std::to_string(p_peer->peer_id) + "."));
        return;
    }
    ERR_THROW(peers.count(p_peer->get_peer_id()) > 0, "IPeer with given id already exists");
    ERR_THROW(p_peer->get_peer_id() == network_adaptor->get_network_peer_id(), "Cannot add ourselves as a peer in RollbackNetworkSyncManager");

    peers.insert_or_assign(p_peer->get_peer_id(), p_peer);
    event_peer_added.invoke(p_peer->get_peer_id());
}

void ISyncManager::remove_peer(const PeerId_t &p_peer_id) {
    auto it = peers.find(p_peer_id);
    if (it != peers.end()) {
        peers.erase(it);
        event_peer_removed.invoke(p_peer_id);
    }
    if (peers.size() == 0) {
        stop();
    }
}

SharedPtr<IPeer> ISyncManager::get_peer(const PeerId_t &p_peer_id) const {
    auto it = peers.find(p_peer_id);
    if (it == peers.end())
        return nullptr;
    return it->second;
}

void ISyncManager::start(const SharedPtr<ITimer> &p_start_delay_timer) {
    ERR_THROW(!network_adaptor->is_network_host() || !mechanized, "start() should only be called on the host");
    if (started || _host_starting) {
        return;
    }
    if (mechanized) {
        on_remote_start_received();
        return;
    }
    if (network_adaptor->is_network_host()) {
        uint32_t highest_rtt = 0;
        for (auto &&kv : peers) {
            if (kv.second->rtt > highest_rtt)
                highest_rtt = kv.second->rtt;
        }
        // Call _remote_start() on all the other peers.
        for (auto &&kv : peers) {
            network_adaptor->send_remote_start(kv.first);
        }
        // Attempt to prevent double starting on the host.
        _host_starting = true;

        // Wait for half the highest RTT to start locally.
        HINT(std::format("Delaying host start by {} ms", std::to_string(highest_rtt / 2)));
        start_delay_timer = p_start_delay_timer;
        start_delay_timer->set_wait_time(float(highest_rtt) / 2000.0f);
        start_delay_timer_connect_key = start_delay_timer->event_timeout.connect([this]() {
            on_remote_start_received();
            _host_starting = false;
            start_delay_timer->event_timeout.disconnect(start_delay_timer_connect_key);
            start_delay_timer.reset();
        });
        start_delay_timer->restart();
    }
}

void ISyncManager::stop() {
    if (!(started || _host_starting))
        return;
    network_adaptor->stop_network_adaptor(get_shared_prt<IBase>());
    started = false;
    _reset();
    for (auto &&kv : peers) {
        kv.second->clear();
    }
    event_sync_stopped.invoke();
    spawn_manager.reset();
}

const SharedPtr<const IData> ISyncManager::get_latest_input_from_peer(const PeerId_t &p_peer_id) const {
    auto it = peers.find(p_peer_id);
    if (it != peers.end()) {
        auto input_frame = get_input_frame(it->second->last_received_remote_input_tick);
        if (input_frame)
            return input_frame->get_peer_input(p_peer_id);
    }
    return nullptr;
}

void ISyncManager::process_logic_tick() {
    if (!started)
        return;
#ifdef LOG_ENABLED
    if (logger) {
        logger->begin_tick(current_tick + 1);
        logger->add_message("input_complete_tick", std::to_string(_input_complete_tick));
        logger->add_message("state_complete_tick", std::to_string(_state_complete_tick));
    }
#endif
#if defined(LOG_ENABLED) || defined(DEBUG_ENABLED)
    auto start_time = Utils::get_current_tick_usec();
#endif

    // @todo Is there a way we can move this to _remote_start()?
    // Store an initial state before any ticks.
    if (current_tick == 0) {
        save_current_state();
#ifdef LOG_ENABLED
        if (logger) {
            // _calculate_data_hash(state_buffer[0].data);
            logger->write_state(*state_buffer[0]);
        }
#endif
        // #####
        // # STEP 1: PERFORM ANY ROLLBACKS, IF NECESSARY.
        // ####
        if (mechanized) {
            rollback_ticks = mechanized_rollback_ticks;
        } else {
#ifdef DEBUG_ENABLED
            if (debug_random_rollback_ticks > 0) {
                srand(time(nullptr));
                debug_rollback_ticks = rand() % debug_random_rollback_ticks;
            }
            if (debug_rollback_ticks > 0 && current_tick > 0 && rollback_ticks == 0) {
                rollback_ticks = std::max(rollback_ticks, debug_rollback_ticks);
            }
#endif
            // We need to reload the current tick since we did a partial rollback
            // to the previous tick in order to interpolate.
            if (interpolation && current_tick > 0 && rollback_ticks == 0) {
                call_load_state(state_buffer[-1]->data);
            }
        }

        if (rollback_ticks > 0) {
#ifdef LOG_ENABLED
            if (logger) {
                logger->add_message("rollback_ticks", std::to_string(rollback_ticks));
                logger->start_timing("rollback");
            }
#endif
            auto original_tick = current_tick;

            // Rollback our internal state.
            ERR_THROW((rollback_ticks + 1) > state_buffer.size(), "Not enough state in buffer to rollback requested number of frames");
            if ((rollback_ticks + 1) > state_buffer.size()) {
                handle_fatal_error(std::format("Not enough state in buffer to rollback {} frames", std::to_string(rollback_ticks)));
                return;
            }

            call_load_state(state_buffer[state_buffer.size() - 1 - rollback_ticks]->data);

            current_tick -= rollback_ticks;

#ifdef DEBUG_ENABLED
            if (debug_check_local_state_consistency) {
                // Save already computed states for better logging in case of discrepancy
                debug_check_local_state_consistency_buffer.clear();
                for (auto i = state_buffer.size() - rollback_ticks - 1; i < state_buffer.size(); i++) {
                    debug_check_local_state_consistency_buffer.emplace_back(state_buffer[i]);
                }
                // Debug check that states computed multiple times with complete inputs are the same
                if (_last_state_hashed_tick >= current_tick) {
                    std::map<UUID_t, SharedPtr<const IData>> saved_state;
                    this->call_save_state(saved_state);
                    debug_check_consistent_local_state(std::make_shared<StateBufferFrame>(current_tick, saved_state), "Loaded");
                }
            }
#endif
            state_buffer.resize(state_buffer.size() - rollback_ticks);
            // Invalidate sync ticks after this, they may be asked for again
            if (requested_input_complete_tick > 0 && current_tick < requested_input_complete_tick) {
                requested_input_complete_tick = 0;
            }
            event_state_loaded.invoke(rollback_ticks);

            _in_rollback = true;

            // Iterate forward until we're at the same spot we left off.
            while (rollback_ticks > 0) {
                current_tick += 1;
                if (!do_tick(true)) {
                    return;
                }
                rollback_ticks -= 1;
            }
            ERR_THROW(current_tick != original_tick, "Rollback didn't return to the original tick");

            _in_rollback = false;
#ifdef LOG_ENABLED
            if (logger) {
                logger->stop_timing("rollback");
            }
#endif
        }

        // #####
        // # STEP 2: SKIP TICKS, IF NECESSARY.
        // #####
        if (!mechanized) {
            record_advantage();
            if (_ticks_spent_regaining_sync > 0) {
                _ticks_spent_regaining_sync += 1;
                if (max_ticks_to_regain_sync > 0 && _ticks_spent_regaining_sync > max_ticks_to_regain_sync) {
                    handle_fatal_error("Unable to regain synchronization");
                    return;
                }

                // Check again if we're still getting input buffer underruns.
                if (!cleanup_buffers()) {
                    // This can happen if there's a fatal error in _cleanup_buffers().
                    if (!started) {
                        return;
                    }
                    // Even when we're skipping ticks, still send input.
                    send_input_messages_to_all_peers();
#ifdef LOG_ENABLED
                    if (logger) {
                        logger->skip_tick(Logger::SkipReason::INPUT_BUFFER_UNDERRUN, start_time);
                    }
#endif
                    return;
                }

                // Check if our max lag is still greater than the min lag to regain sync.
                if (min_lag_to_regain_sync > 0 && calculate_max_local_lag() > min_lag_to_regain_sync) {
                    // print ("REGAINING SYNC: wait for local lag to reduce")
                    // Even when we're skipping ticks, still send input.
                    send_input_messages_to_all_peers();
#ifdef LOG_ENABLED
                    if (logger) {
                        logger->skip_tick(Logger::SkipReason::WAITING_TO_REGAIN_SYNC, start_time);
                    }
#endif
                    return;
                }

                // If we've reach this point, that means we've regained sync!
                _ticks_spent_regaining_sync = 0;
                event_sync_regained.invoke();

                // We don't want to skip ticks through the normal mechanism, because
                // any skips that were previously calculated don't apply anymore.
                skip_ticks = 0;
            }

            // Attempt to clean up buffers, but if we can't, that means we've lost sync.
            else if (!cleanup_buffers()) {
                // This can happen if there's a fatal error in _cleanup_buffers().
                if (!started)
                    return;
                event_sync_lost.invoke();
                _ticks_spent_regaining_sync = 1;
                // Even when we're skipping ticks, still send input.
                send_input_messages_to_all_peers();
#ifdef LOG_ENABLED
                if (logger) {
                    logger->skip_tick(Logger::SkipReason::INPUT_BUFFER_UNDERRUN, start_time);
                }
#endif
                return;
            }

            if (skip_ticks > 0) {
                skip_ticks -= 1;
                if (skip_ticks == 0) {
                    for (auto &kv : peers) {
                        kv.second->clear_advantage();
                    }
                } else {
                    // Even when we're skipping ticks, still send input.
                    send_input_messages_to_all_peers();
#ifdef LOG_ENABLED
                    if (logger) {
                        logger->skip_tick(Logger::SkipReason::ADVANTAGE_ADJUSTMENT, start_time);
                    }
#endif
                    return;
                }
            }

            if (calculate_skip_ticks()) {
                // This means we need to skip some ticks, so may as well start now!
#ifdef LOG_ENABLED
                if (logger)
                    logger->skip_tick(Logger::SkipReason::ADVANTAGE_ADJUSTMENT, start_time);
#endif
                return;
            }
        } else {
            cleanup_buffers();
        }

        // #####
        // # STEP 3: GATHER INPUT AND RUN CURRENT TICK
        // #####
        input_tick += 1;
        current_tick += 1;

        if (!mechanized) {
            auto input_frame = get_or_create_input_frame(input_tick);
            // The underlying error would have already been reported in
            // _get_or_create_input_frame() so we can just return here.
            if (input_frame == nullptr)
                return;

#ifdef LOG_ENABLED
            if (logger)
                logger->add_message("input_tick", std::to_string(input_tick));
#endif
            auto local_input = call_get_local_input();
            // _calculate_data_hash(local_input)
            input_frame->peers_input.insert_or_assign(network_adaptor->get_network_peer_id(), InputBufferFrame::PeerInput(local_input, false));

            // Only serialize and send input when we have real remote peers.
            if (peers.size() > 0) {
                SerializedData serialized_inputs = data_adaptor->serialize_input(local_input);
                // check that the serialized then unserialized input matches the original
#ifdef DEBUG_ENABLED
                if (debug_check_message_serializer_roundtrip) {
                    auto deserialized_local_inputs = data_adaptor->deserialize_input(serialized_inputs);
                    ERR_THROW(local_input->get_hash() != deserialized_local_inputs->get_hash(),
                              std::format("The input is different after being serialized and unserialized \n Original: {} \n Unserialized: {}", std::to_string(local_input),
                                          std::to_string(deserialized_local_inputs)));
                }
#endif
                _input_send_queue.emplace_back(serialized_inputs);
                ERR_THROW(input_tick != _input_send_queue_start_tick + _input_send_queue.size() - 1, "Input send queue ticks numbers are misaligned");
                send_input_messages_to_all_peers();
            }
        }
        if (current_tick > 0) {
#ifdef LOG_ENABLED
            if (logger)
                logger->start_timing("current_tick");
#endif
            if (!do_tick())
                return;
#ifdef LOG_ENABLED
            if (logger)
                logger->stop_timing("current_tick");
#endif
            if (interpolation) {
                // Capture the state data to interpolate between.
                auto to_state = state_buffer[state_buffer.size() - 1]->data;
                auto from_state = state_buffer[state_buffer.size() - 2]->data;
                _interpolation_state.clear();
                for (auto &kv : to_state) {
                    auto uuid = kv.first;
                    auto it = from_state.find(uuid);
                    if (it != from_state.end()) {
                        std::pair<SharedPtr<const IData>, SharedPtr<const IData>> pair(from_state.at(uuid), to_state.at(uuid));
                        _interpolation_state.emplace(uuid, pair);
                    }
                }

                // Return to state from the previous frame, so we can interpolate
                // towards the state of the current frame.
                call_load_state(state_buffer[state_buffer.size() - 2]->data);
            }
        }
    }

    _time_since_last_tick = 0.0;
    _ran_physics_process = true;
    _ticks_since_last_interpolation_frame += 1;

#ifdef DEBUG_ENABLED
    auto total_time_msecs = float(Utils::get_current_tick_usec() - start_time) / 1000.0;
    if (debug_physics_process_msecs > 0 && total_time_msecs > debug_physics_process_msecs) {
        PUSH_ERR(std::format("[{}] RollbackNetworkSyncManager._physics_process() took {}ms", std::to_string(current_tick), std::to_string(total_time_msecs)));
    }
#endif
#ifdef LOG_ENABLED
    if (logger)
        logger->end_tick(start_time);
#endif
}

void ISyncManager::process_frame(const float delta) {
    if (!started)
        return;

#if defined(DEBUG_ENABLED) || defined(LOG_ENABLED)
    auto start_time = Utils::get_current_tick_usec();
#endif
    // These are things that we want to run during "interpolation frames", in
    // order to slim down the normal frames. Or, if interpolation is disabled,
    // we need to run these always. If we haven't managed to run this for more
    // one tick, we make sure to sneak it in just in case.
    if (!interpolation || !_ran_physics_process || _ticks_since_last_interpolation_frame > 1) {
#ifdef LOG_ENABLED
        if (logger) {
            logger->begin_interpolation_frame(current_tick);
        }
#endif
        _time_since_last_tick += delta;
        // Don't interpolate if we are skipping ticks, or just ran physics process.
        if (interpolation && skip_ticks == 0 && !_ran_physics_process) {
            auto weight = _time_since_last_tick / tick_time_float;
            if (weight > 1.0) {
                weight = 1.0;
            }
            call_interpolate_state(weight);
        }

        // If there are no other peers, then we'll never receive any new input,
        // so we need to update the _input_complete_tick elsewhere. Here's a fine
        // place to do it!
        if (peers.size() == 0) {
            update_input_complete_tick();
        }

        update_state_hashes();

        if (interpolation)
            event_interpolation_frame.invoke();

        // Do this last to catch any data that came in late.
        network_adaptor->poll();

#ifdef LOG_ENABLED
        if (logger) {
            logger->end_interpolation_frame(current_tick);
        }
#endif
        // Clear counter, because we just did an interpolation frame.
        _ticks_since_last_interpolation_frame = 0;
    }
    // Clear flag so subsequent _process() calls will know that they weren't
    // preceeded by _physics_process().
    _ran_physics_process = false;

#ifdef DEBUG_ENABLED
    auto total_time_msecs = float(Utils::get_current_tick_usec() - start_time) / 1000.0;
    if (debug_process_msecs > 0 && total_time_msecs > debug_process_msecs)
        PUSH_ERR(std::format("[{}] RollbackNetworkSyncManager._process() took {}ms", std::to_string(current_tick), std::to_string(total_time_msecs)));
#endif
}

void ISyncManager::reset_mechanized_data() {
    mechanized_input_received.clear();
    mechanized_rollback_ticks = 0;
}
void ISyncManager::execute_mechanized_tick() {
    process_mechanized_input();
    process_logic_tick();
    reset_mechanized_data();
}
void ISyncManager::execute_mechanized_interpolation_frame(float delta) {
    update_input_complete_tick();
    _ran_physics_process = false;
    process_frame(delta);
    process_mechanized_input();
    reset_mechanized_data();
}
void ISyncManager::execute_mechanized_interframe() {
    process_mechanized_input();
    reset_mechanized_data();
}
void ISyncManager::process_mechanized_input() {
    for (auto &mkv : mechanized_input_received) {
        const PeerId_t &peer_id = mkv.first;
        for (auto &pkv : mkv.second) {
            const Tick_t &tick = pkv.first;
            auto input_frame = get_or_create_input_frame(tick);
            input_frame->add_peer_input(peer_id, pkv.second.input, false);
        }
    }
}

void ISyncManager::call_save_state(std::map<UUID_t, SharedPtr<const IData>> &r_saved_state_frame) {
    r_saved_state_frame.emplace(SpawnManager::UUID, spawn_manager.save_state());
    for (auto it = network_rollbackables.begin(); it != network_rollbackables.end(); it++) {
        if (it->second) {
            if (it->second->is_valid()) {
                r_saved_state_frame.emplace(it->first, it->second->save_state());
            }
        } else {
            auto to_remove = it;
            network_rollbackables.erase(to_remove);
            it++;
            if (it == network_rollbackables.end())
                break;
        }
    }
}

void ISyncManager::call_load_state(const std::map<UUID_t, SharedPtr<const IData>> &p_state_frame) {
    ERR_THROW(p_state_frame.find(SpawnManager::UUID) == p_state_frame.end(), "Can't find 'SpawnManager::UUID' in state");
    spawn_manager.load_state(p_state_frame.find(SpawnManager::UUID)->second);
    for (auto &kv : p_state_frame) {
        ERR_THROW(network_rollbackables.count(kv.first) <= 0, std::format("Unable to restore state to missing obj: {}", std::to_string(kv.first)))
        network_rollbackables[kv.first]->load_state(kv.second);
    }
}

void ISyncManager::update_input_complete_tick() {
    while (input_tick >= _input_complete_tick + 1) {
        auto input_frame = get_input_frame(_input_complete_tick + 1);
        if (input_frame == nullptr)
            break;
        if (!input_frame->is_complete(peers))
            break;
#ifdef DEBUG_ENABLED
        // When we add debug rollbacks mark the input as not complete
        // so that the invariant "a complete input frame cannot be rolled back" is respected
        // NB: a complete input frame can still be loaded in a rollback for the incomplete input next frame
        if (debug_random_rollback_ticks > 0 && _input_complete_tick + 1 > current_tick - debug_random_rollback_ticks)
            break;
        if (debug_rollback_ticks > 0 && _input_complete_tick + 1 > current_tick - debug_rollback_ticks)
            break;
#endif
#ifdef LOG_ENABLED
        if (logger)
            logger->write_input(*input_frame);
#endif
        _input_complete_tick += 1;

        // This tick should be recomputed with complete inputs, let's roll back
        if (_input_complete_tick == requested_input_complete_tick) {
            requested_input_complete_tick = 0;
            uint32_t tick_delta = current_tick - _input_complete_tick;
            if (tick_delta >= 0 && rollback_ticks <= tick_delta) {
                rollback_ticks = tick_delta + 1;
                event_rollback_flagged.invoke(_input_complete_tick);
            }
        }
        event_tick_input_completed.invoke(_input_complete_tick);
    }
}

void ISyncManager::clear_peers() {
    std::vector<PeerId_t> peer_ids(peers.size());
    for (auto &&pair : peers) {
        peer_ids.emplace_back(pair.first);
    }
    for (auto &&peer_id : peer_ids) {
        remove_peer(peer_id);
    }
}

void ISyncManager::handle_fatal_error(const std::string &msg) {
    event_sync_error.invoke(msg);
    WARNING("NETWORK SYNC LOST: " + msg);
    stop();
#ifdef LOG_ENABLED
    if (logger) {
        logger->log_fatal_error(msg);
    }
#endif
}

// PRIVATE

std::shared_ptr<InputBufferFrame> ISyncManager::get_or_create_input_frame(const Tick_t p_tick) {
    if (input_buffer.size() == 0) {
        std::shared_ptr<InputBufferFrame> input_frame = std::make_shared<InputBufferFrame>(p_tick);
        input_buffer.emplace_back(input_frame);
        return input_frame;
    } else if (p_tick > input_buffer.back()->tick) {
        auto highest = input_buffer.back()->tick;
        while (highest < p_tick) {
            highest += 1;
            std::shared_ptr<InputBufferFrame> input_frame = std::make_shared<InputBufferFrame>(highest);
            input_buffer.emplace_back(input_frame);
        }
        return input_buffer.back();
    } else {
        auto input_frame = get_input_frame(p_tick);
        if (input_frame == nullptr) {
            handle_fatal_error(std::format("Requested input frame ({}) not found in buffer", std::to_string(p_tick)));
        }
        return nullptr;
    }
}

bool ISyncManager::cleanup_buffers() {
    // Clean-up the input send queue.
    auto min_next_input_tick_requested = calculate_minimum_next_input_tick_requested();
    while (_input_send_queue_start_tick < min_next_input_tick_requested) {
        _input_send_queue.pop_front();
        _input_send_queue_start_tick += 1;
    }
    // Clean-up old state buffer frames. We need to keep one extra frame of state
    // because when we rollback, we need to load the state for the frame before
    // the first one we need to run again.
    while (state_buffer.size() > max_buffer_size + 1) {
        auto state_frame_to_retire = state_buffer.front();
        auto input_frame = get_input_frame(state_frame_to_retire->tick + 1);
        if (input_frame == nullptr) {
            WARNING(std::format("Attempting to retire state frame {}, but input frame {} is missing", std::to_string(state_frame_to_retire->tick), std::to_string(state_frame_to_retire->tick + 1)));
#ifdef LOG_ENABLED
            if (logger) {
                logger->add_message("buffer_underrun_message", std::format("Attempting to retire state frame {}, but input frame {} is missing", std::to_string(state_frame_to_retire->tick),
                                                                           std::to_string(state_frame_to_retire->tick + 1)));
            }
#endif
            return false;
        }
        if (!input_frame->is_complete(peers)) {
            WARNING(std::format("Attempting to retire state frame {}, but input frame {} is still missing input (missing peer(s): {})", std::to_string(state_frame_to_retire->tick),
                                std::to_string(input_frame->tick), arr_to_string(input_frame->get_missing_peers(peers))));
#ifdef LOG_ENABLED
            if (logger) {
                logger->add_message("buffer_underrun_message",
                                    std::format("Attempting to retire state frame {}, but input frame {} is still missing input (missing peer(s): {})", std::to_string(state_frame_to_retire->tick),
                                                std::to_string(input_frame->tick), arr_to_string(input_frame->get_missing_peers(peers))));
            }
#endif
            return false;
        }
        if (state_frame_to_retire->tick > _last_state_hashed_tick) {
            WARNING(std::format("Unable to retire state frame {}, because we haven't hashed it yet", std::to_string(state_frame_to_retire->tick)));
#ifdef LOG_ENABLED
            if (logger) {
                logger->add_message("buffer_underrun_message", std::format("Unable to retire state frame {}, because we haven't hashed it yet", std::to_string(state_frame_to_retire->tick)));
            }
#endif
            return false;
        }
        state_buffer.pop_front();
        _state_buffer_start_tick += 1;

        event_tick_retired.invoke(state_frame_to_retire->tick);
    }

    // Clean-up old input buffer frames. Unlike state frames, we can have many
    // frames from the future if we are running behind. We don't want having too
    // many future frames to end up discarding input for the current frame, so we
    // only count input frames before the current frame towards the buffer size.
    while ((current_tick - _input_buffer_start_tick) > max_buffer_size) {
        _input_buffer_start_tick += 1;
        auto front = input_buffer.front();
        input_buffer.pop_front();
    }

    while (state_hashes.size() > (uint64_t(max_buffer_size) * 2)) {
        auto state_hash_to_retire = state_hashes.front();
        ERR_THROW(state_hash_to_retire == nullptr, std::bad_exception());
        if (!state_hash_to_retire->is_complete(peers) && !mechanized) {
            WARNING(std::format("Attempting to retire state hash frame {}, but we're still missing hashes (missing peer(s): {})", std::to_string(state_hash_to_retire->tick),
                                arr_to_string(state_hash_to_retire->get_missing_peers(peers))));
#ifdef LOG_ENABLED
            if (logger) {
                logger->add_message("buffer_underrun_message", std::format("Attempting to retire state hash frame {}, but we're still missing hashes (missing peer(s): {})",
                                                                           std::to_string(state_hash_to_retire->tick), arr_to_string(state_hash_to_retire->get_missing_peers(peers))));
            }
#endif
            return false;
        }

        if (state_hash_to_retire->mismatch) {
            _state_mismatch_count += 1;
        } else {
            _state_mismatch_count = 0;
        }

        if (_state_mismatch_count > max_state_mismatch_count) {
            handle_fatal_error("Fatal state mismatch");
            return false;
        }

        _state_hashes_start_tick += 1;
        auto front = state_buffer.front();
        state_hashes.pop_front();
    }

    return true;
}

void ISyncManager::get_input_messages_from_send_queue_in_range(const Tick_t p_first_index, const Tick_t p_last_index, std::vector<std::map<Tick_t, SerializedData>> &r_input_messages_from_send_queue,
                                                               const bool p_reverse) const {
    if (p_reverse) {
        std::map<Tick_t, SerializedData> msg;
        for (Tick_t i = p_last_index; i <= p_last_index; i++) {
            msg.emplace(i, _input_send_queue[i]);
            if (max_input_frames_per_message > 0 && msg.size() == max_input_frames_per_message) {
                r_input_messages_from_send_queue.emplace_back(msg);
                msg = std::map<Tick_t, SerializedData>();
            }
        }
        if (msg.size() > 0)
            r_input_messages_from_send_queue.emplace_back(msg);
    } else {
        std::map<Tick_t, SerializedData> msg;
        for (auto i = p_last_index; i >= p_first_index; i--) {
            msg.emplace(i, _input_send_queue[i]);
            if (max_input_frames_per_message > 0 && msg.size() == max_input_frames_per_message) {
                r_input_messages_from_send_queue.emplace_back(msg);
                msg = std::map<Tick_t, SerializedData>();
            }
        }
        if (msg.size() > 0)
            r_input_messages_from_send_queue.emplace_back(msg);
    }
}

void ISyncManager::send_input_messages_to_peer(const PeerId_t &peer_id) {
    ERR_THROW(peer_id == network_adaptor->get_network_peer_id(), "Cannot send input to ourselves");
    ERR_THROW(peers.count(peer_id) == 0, std::bad_exception());

    const auto peer = peers[peer_id];

    std::map<Tick_t, Hash_t> state_hashes;
    get_state_hashes_for_peer(peer, state_hashes);

    // TODO:: the capacity of this std::vector should be specify.
    std::vector<std::map<Tick_t, SerializedData>> input_messages(_input_send_queue.size());
    get_input_messages_from_send_queue_for_peer(*peer, input_messages);

#ifdef LOG_ENABLED
    if (logger) {
        logger->add_message(std::format("messages_sent_to_peer_{}", std::to_string(peer_id)), std::to_string(input_messages.size()));
    }
#endif
    for (auto &inputs : input_messages) {
        SerializedData data = data_adaptor->serialize_message(PeerFrameMessage(peer->last_received_remote_input_tick + 1, inputs, peer->last_received_remote_hash_tick + 1, state_hashes));
#ifdef DEBUG_ENABLED
        // See https://gafferongames.com/post/packet_fragmentation_and_reassembly/
        if (debug_message_bytes > 0) {
            if (data.size > debug_message_bytes)
                WARNING(std::format("Sending message w/ size {} bytes", std::to_string(data.size)));
        }
#endif

#ifdef LOG_ENABLED
        if (logger) {
            logger->add_value(std::format("messages_sent_to_peer_{}_size", std::to_string(peer_id)), std::to_string(data.size));
            logger->increment_value(std::format("messages_sent_to_peer_{}_total_size", std::to_string(peer_id)), data.size);
            jsonxx::Array ticks;
            for (auto &kv : inputs) {
                ticks << kv.first;
            }
            logger->merge_array_value(std::format("input_ticks_sent_to_peer_{}", std::to_string(peer_id)), ticks);
        }
#endif
        // var ticks = msg[InputMessageKey.INPUT].keys()
        // print ("[%s] Sending ticks %s - %s" % [current_tick, min(ticks[0], ticks[-1]), max(ticks[0], ticks[-1])])

        network_adaptor->send_input_tick(peer_id, data);
    }
}

#ifdef DEBUG_ENABLED
void ISyncManager::debug_check_consistent_local_state(const std::shared_ptr<StateBufferFrame> &p_state, const std::string &message) {
    auto hashed_state = p_state->get_data_hash();
    auto previously_hashed_frame = get_state_hash_frame(current_tick);
    auto previous_state = debug_check_local_state_consistency_buffer.front();
    debug_check_local_state_consistency_buffer.pop_front();
    if (previously_hashed_frame && previously_hashed_frame->state_hash != hashed_state) {
        auto comparer = DebugComparer();
        comparer.find_mismatches(previous_state->to_json_obj(), p_state->to_json_obj());
        ERR_THROW(true, message + " state is not consistent with saved state:\n " + comparer.get_mismatches_string());
    }
}
#endif

// SharedPtr<ISpawnable>
// ISyncManager::spawn(const SharedPtr<const ISpawner> &p_spawner,
//                     const SharedPtr<ISpawnData> &p_spawn_data) // name: String, parent: Node, scene: PackedScene, data: Dictionary = {}, rename: bool = true, signal_name: String = '')
// {
//     if (!started) {
//         PUSH_ERR(std::format("Refusing to spawn {} before RollbackNetworkSyncManager has started", std::to_string(p_spawn_data)));
//         return nullptr;
//     }

//     return spawn_manager.spawn(p_spawner, p_spawn_data);
// }

} // namespace rollback_netcode