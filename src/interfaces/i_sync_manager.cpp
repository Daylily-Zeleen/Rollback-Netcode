
#include "core/input_buffer_frame.h"
#include "core/spawn_manager.h"
#include "def.h"
#include "interfaces/base/i_base.h"
#include "interfaces/i_network_rollbackable.h"
#include "thirdparty/deterministic_float/glacier_float.h"
#include "utils.h"
#include <core/input_buffer_frame.h>
#include <interfaces/i_sync_manager.h>

#include <memory>
#include <numeric>

#ifdef DEBUG
#include <debug/debug_comparer.h>
#endif

namespace rollback_netcode {

ISyncManager::ISyncManager(SharedPtr<INetworkAdaptor> &p_network_adaptor, SharedPtr<IMessageSerializer> &p_message_serializer, SharedPtr<ITimer> &p_ping_timer) {
	singleton = this;

	message_serializer = p_message_serializer;
	ping_timer = p_ping_timer;

	network_adaptor = p_network_adaptor;
	network_adaptor->event_ping_received.connect([this](auto &&PH1, auto &&PH2) { on_ping_received(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2)); });
	network_adaptor->event_ping_back_received.connect([this](auto &&PH1, auto &&PH2) { on_ping_back_received(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2)); });
	network_adaptor->event_start_remote_received.connect([this] { on_remote_start_received(); });
	network_adaptor->event_start_stop_received.connect([this] { on_remote_stop_received(); });
	network_adaptor->event_input_tick_received.connect([this](auto &&PH1, auto &&PH2) { on_input_tick_received(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2)); });

	ping_timer->event_timeout.connect([this] { on_ping_timer_timeout(); });
	spawn_manager.event_spawned.connect([this](auto &&PH1, auto &&PH2) { on_spawned(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2)); });
	spawn_manager.event_predespawn.connect([this](auto &&PH1, auto &&PH2) { on_predespawn(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2)); });
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
	ERR_THROW(started, "Changing the mechanized flag after SyncManager has started will probably break everything");

	mechanized = p_mechanized;
#ifdef LOG_ENABLE
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

void ISyncManager::set_ping_frequency(const float &p_ping_frequency) {
	ping_frequency = p_ping_frequency;
	if (ping_timer)
		ping_timer->set_wait_time(ping_frequency);
}
void ISyncManager::set_max_messages_at_once(const uint8_t p_max_messages_at_once) {
	max_messages_at_once = p_max_messages_at_once;
	_new_messages = uint8_t(ceil(float(max_messages_at_once) / 2.0f));
	_old_messages = uint8_t(floor(float(max_messages_at_once) / 2.0f));
}
void ISyncManager::start_logging(const std::string &p_log_file_path, const SharedPtr<IMatchInfo> &p_match_info) {
#ifdef LOG_ENABLE
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
#ifdef LOG_ENABLE
	if (logger) {
		logger->stop();
		logger.reset();
	}
#endif
}

void ISyncManager::add_peer(SharedPtr<IPeer> &p_peer) {
	ERR_THROW(peers.count(p_peer->get_peer_id()) > 0, "IPeer with given id already exists");
	ERR_THROW(p_peer->get_peer_id() == network_adaptor->get_network_peer_id(), "Cannot add ourselves as a peer in SyncManager");

	peers.insert({ p_peer->get_peer_id(), p_peer });
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
		HINT(string_sprintf("Delaying host start by %d ms", (highest_rtt / 2)));
		start_delay_timer = p_start_delay_timer;
		start_delay_timer->set_wait_time(float(highest_rtt) / 2000.0f);
		start_delay_timer_connect_key = start_delay_timer->event_timeout.connect([this]() {
			on_remote_start_received();
			_host_starting = false;
			start_delay_timer->event_timeout.disconnect(start_delay_timer_connect_key);
			start_delay_timer.reset();
		});
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

const SharedPtr<const IInput> ISyncManager::get_latest_input_from_peer(const PeerId_t &p_peer_id) const {
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
#ifdef LOG_ENABLE
	if (logger) {
		logger->begin_tick(current_tick + 1);
		logger->add_message("input_complete_tick", std::to_string(_input_complete_tick));
		logger->add_message("state_complete_tick", std::to_string(_state_complete_tick));
	}
#endif
#if defined(LOG_ENABLE) || defined(DEBUG)
	auto start_time = get_current_tick_usec();
#endif

	// @todo Is there a way we can move this to _remote_start()?
	// Store an initial state before any ticks.
	if (current_tick == 0) {
		save_current_state();
#ifdef LOG_ENABLE
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
#ifdef DEBUG
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
#ifdef LOG_ENABLE
			if (logger) {
				logger->add_message("rollback_ticks", std::to_string(rollback_ticks));
				logger->start_timing("rollback");
			}
#endif
			auto original_tick = current_tick;

			// Rollback our internal state.
			ERR_THROW((rollback_ticks + 1) > state_buffer.size(), "Not enough state in buffer to rollback requested number of frames");
			if ((rollback_ticks + 1) > state_buffer.size()) {
				handle_fatal_error(string_sprintf("Not enough state in buffer to rollback %d frames", rollback_ticks));
				return;
			}

			call_load_state(state_buffer[state_buffer.size() - 1 - rollback_ticks]->data);

			current_tick -= rollback_ticks;

#ifdef DEBUG
			if (debug_check_local_state_consistency) {
				// Save already computed states for better logging in case of discrepancy
				debug_check_local_state_consistency_buffer.clear();
				for (auto i = state_buffer.size() - rollback_ticks - 1; i < state_buffer.size(); i++) {
					debug_check_local_state_consistency_buffer.emplace_back(state_buffer[i]);
				}
				// Debug check that states computed multiple times with complete inputs are the same
				if (_last_state_hashed_tick >= current_tick) {
					std::map<UUID_t, SharedPtr<const IState>> saved_state;
					call_save_state(saved_state);
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
#ifdef LOG_ENABLE
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
#ifdef LOG_ENABLE
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
#ifdef LOG_ENABLE
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
#ifdef LOG_ENABLE
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
#ifdef LOG_ENABLE
					if (logger) {
						logger->skip_tick(Logger::SkipReason::ADVANTAGE_ADJUSTMENT, start_time);
					}
#endif
					return;
				}
			}

			if (calculate_skip_ticks()) {
				// This means we need to skip some ticks, so may as well start now!
#ifdef LOG_ENABLE
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

#ifdef LOG_ENABLE
			if (logger)
				logger->add_message("input_tick", std::to_string(input_tick));
#endif
			auto local_input = call_get_local_input();
			// _calculate_data_hash(local_input)
			input_frame->peers_input.insert_or_assign(network_adaptor->get_network_peer_id(), InputBufferFrame::PeerInput(local_input, false));

			// Only serialize and send input when we have real remote peers.
			if (peers.size() > 0) {
				SerializedData serialized_inputs;
				message_serializer->serialize_input(local_input, serialized_inputs);
				// check that the serialized then unserialized input matches the original
#ifdef DEBUG
				if (debug_check_message_serializer_roundtrip) {
					auto deserialized_local_inputs = message_serializer->deserialize_input(serialized_inputs);
					ERR_THROW(local_input->get_hash() != deserialized_local_inputs->get_hash(),
							string_sprintf("The input is different after being serialized and unserialized \n Original: %s \n Unserialized: %s", std::to_string(local_input),
									std::to_string(deserialized_local_inputs)));
				}
#endif
				_input_send_queue.emplace_back(serialized_inputs);
				ERR_THROW(input_tick != _input_send_queue_start_tick + _input_send_queue.size() - 1, "Input send queue ticks numbers are misaligned");
				send_input_messages_to_all_peers();
			}
		}
		if (current_tick > 0) {
#ifdef LOG_ENABLE
			if (logger)
				logger->start_timing("current_tick");
#endif
			if (!do_tick())
				return;
#ifdef LOG_ENABLE
			if (logger)
				logger->stop_timing("current_tick");
#endif
			if (interpolation) {
				// Capture the state data to interpolate between.
				auto to_state = state_buffer[state_buffer.size() - 1]->data;
				auto from_state = state_buffer[state_buffer.size() - 2]->data;
				_interpolation_state.clear();
				for (auto &kv : to_state) {
					auto it = from_state.find(kv.first);
					if (it != from_state.end()) {
						std::pair<SharedPtr<const IState>, SharedPtr<const IState>> pair(from_state[kv.first], to_state[kv.first]);
						_interpolation_state.insert({ kv.first, pair });
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

#ifdef DEBUG
	auto total_time_msecs = float(get_current_tick_usec() - start_time) / 1000.0;
	if (debug_physics_process_msecs > 0 && total_time_msecs > debug_physics_process_msecs) {
		PUSH_ERR(string_sprintf("[%d] SyncManager._physics_process() took %.02fms", current_tick, total_time_msecs));
	}
#endif
#ifdef LOG_ENABLE
	if (logger)
		logger->end_tick(start_time);
#endif
}

void ISyncManager::process_frame(const float delta) {
	if (!started)
		return;

#if defined(DEBUG) || defined(LOG_ENABLE)
	auto start_time = get_current_tick_usec();
#endif
	// These are things that we want to run during "interpolation frames", in
	// order to slim down the normal frames. Or, if interpolation is disabled,
	// we need to run these always. If we haven't managed to run this for more
	// one tick, we make sure to sneak it in just in case.
	if (!interpolation || !_ran_physics_process || _ticks_since_last_interpolation_frame > 1) {
#ifdef LOG_ENABLE
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

#ifdef LOG_ENABLE
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

#ifdef DEBUG
	auto total_time_msecs = float(get_current_tick_usec() - start_time) / 1000.0;
	if (debug_process_msecs > 0 && total_time_msecs > debug_process_msecs)
		PUSH_ERR(string_sprintf("[%d] SyncManager._process() took %.02fms", current_tick, total_time_msecs));
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

bool ISyncManager::ensure_current_tick_input_complete() {
	if (is_current_tick_input_complete())
		return true;
	if (requested_input_complete_tick == 0 || requested_input_complete_tick > current_tick)
		requested_input_complete_tick = current_tick;
	return false;
}

// private
SharedPtr<const IInput> ISyncManager::call_get_local_input() {
	return network_adaptor->get_network_peer()->get_local_input();
}

void ISyncManager::call_network_process(const InputBufferFrame &p_input_buffer_frame) {
	for (auto it = network_processables.begin(); it != network_processables.end(); it++) {
		auto obj = it->second;
		if (obj) {
			if (obj->is_valid()) {
				auto input = p_input_buffer_frame.get_peer_input(obj->get_network_master_peer_id());
				if (input) {
					obj->network_preprocess(input);
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
			}
		}
	}
	for (auto &kv : network_processables) {
		if (kv.second->is_valid()) {
			auto input = p_input_buffer_frame.get_peer_input(kv.second->get_network_master_peer_id());
			if (input) {
				kv.second->network_postprocess(input);
			}
		}
	}
}

void ISyncManager::call_save_state(std::map<UUID_t, SharedPtr<const IState>> &r_saved_state_frame) {
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

void ISyncManager::call_load_state(const std::map<UUID_t, SharedPtr<const IState>> &p_state_frame) {
	ERR_THROW(p_state_frame.find(SpawnManager::UUID) == p_state_frame.end(), "Can't find 'SpawnManager::UUID' in state");
	spawn_manager.load_state(p_state_frame.find(SpawnManager::UUID)->second);
	for (auto &kv : p_state_frame) {
		ERR_THROW(network_rollbackables.count(kv.first) <= 0, string_sprintf("Unable to restore state to missing obj: %d", kv.first))
		network_rollbackables[kv.first]->load_state(kv.second);
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
	std::map<UUID_t, SharedPtr<const IState>> current_state;
	call_save_state(current_state);
	state_buffer.emplace_back(std::make_shared<StateBufferFrame>(current_tick, current_state));

	// If the input for this state is complete, then update _state_complete_tick.
	if (_input_complete_tick > _state_complete_tick) {
		// Set to the current_tick so long as its less than or equal to the
		// _input_complete_tick, otherwise, cap it to the _input_complete_tick.
		_state_complete_tick = current_tick <= _input_complete_tick ? current_tick : _input_complete_tick;
	}
}

void ISyncManager::update_input_complete_tick() {
	while (input_tick >= _input_complete_tick + 1) {
		auto input_frame = get_input_frame(_input_complete_tick + 1);
		if (input_frame == nullptr)
			break;
		if (!input_frame->is_complete(peers))
			break;
#ifdef DEBUG
		// When we add debug rollbacks mark the input as not complete
		// so that the invariant "a complete input frame cannot be rolled back" is respected
		// NB: a complete input frame can still be loaded in a rollback for the incomplete input next frame
		if (debug_random_rollback_ticks > 0 && _input_complete_tick + 1 > current_tick - debug_random_rollback_ticks)
			break;
		if (debug_rollback_ticks > 0 && _input_complete_tick + 1 > current_tick - debug_rollback_ticks)
			break;
#endif
#ifdef LOG_ENABLE
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
#ifdef LOG_ENABLE
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
#ifdef LOG_ENABLE
	if (logger) {
		logger->log_fatal_error(msg);
	}
#endif
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
void ISyncManager::on_spawned(SharedPtr<ISpawnable> &p_spawned_obj, const SharedPtr<const ISpawnData> &p_spawned_data) { event_spawned.invoke(p_spawned_obj, p_spawned_data); }
void ISyncManager::on_predespawn(SharedPtr<ISpawnable> &p_despawned_obj, const SharedPtr<const ISpawnData> &p_spawned_data) { event_predespawn.invoke(p_despawned_obj, p_spawned_data); }
void ISyncManager::on_ping_received(const PeerId_t &p_peer_id, PingMessage &p_peer_msg) {
	ERR_THROW(p_peer_id == network_adaptor->get_network_peer_id(), "Cannot ping back ourselves");
	p_peer_msg.set_remote_time();
	network_adaptor->send_ping_back(p_peer_id, p_peer_msg);
}
void ISyncManager::on_ping_back_received(const PeerId_t &p_peer_id, PingMessage &p_peer_msg) {
	auto peer = peers[p_peer_id];
	auto current = get_current_system_time_msec();
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

// PRIVATE
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

#ifdef DEBUG
	// Debug check that states computed multiple times with complete inputs are the same
	if (_last_state_hashed_tick >= current_tick) {
		debug_check_consistent_local_state(state_buffer.back(), "Recomputed");
	}
#endif

	event_tick_finished.invoke(rollback);
	return true;
}

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
			handle_fatal_error(string_sprintf("Requested input frame (%i) not found in buffer", p_tick));
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
			WARNING(string_sprintf("Attempting to retire state frame %s, but input frame %s is missing", state_frame_to_retire->tick, state_frame_to_retire->tick + 1));
#ifdef LOG_ENABLE
			if (logger) {
				logger->add_message("buffer_underrun_message",
						string_sprintf("Attempting to retire state frame %s, but input frame %s is missing", state_frame_to_retire->tick, state_frame_to_retire->tick + 1));
			}
#endif
			return false;
		}
		if (!input_frame->is_complete(peers)) {
			WARNING(string_sprintf("Attempting to retire state frame %s, but input frame %s is still missing input (missing peer(s): %s)", state_frame_to_retire->tick, input_frame->tick,
					arr_to_string(input_frame->get_missing_peers(peers))));
#ifdef LOG_ENABLE
			if (logger) {
				logger->add_message("buffer_underrun_message", string_sprintf("Attempting to retire state frame %s, but input frame %s is still missing input (missing peer(s): %s)", state_frame_to_retire->tick, input_frame->tick, arr_to_string(input_frame->get_missing_peers(peers))));
			}
#endif
			return false;
		}
		if (state_frame_to_retire->tick > _last_state_hashed_tick) {
			WARNING(string_sprintf("Unable to retire state frame %s, because we haven't hashed it yet", state_frame_to_retire->tick));
#ifdef LOG_ENABLE
			if (logger) {
				logger->add_message("buffer_underrun_message", string_sprintf("Unable to retire state frame %s, because we haven't hashed it yet", state_frame_to_retire->tick));
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
			WARNING(string_sprintf("Attempting to retire state hash frame %d, but we're still missing hashes (missing peer(s): %s)", state_hash_to_retire->tick,
					arr_to_string(state_hash_to_retire->get_missing_peers(peers))));
#ifdef LOG_ENABLE
			if (logger) {
				logger->add_message("buffer_underrun_message", string_sprintf("Attempting to retire state hash frame %s, but we're still missing hashes (missing peer(s): %s)", state_hash_to_retire->tick, arr_to_string(state_hash_to_retire->get_missing_peers(peers))));
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

std::shared_ptr<StateHashFrame> ISyncManager::get_state_hash_frame(const Tick_t p_tick) const {
	if (p_tick < _state_hashes_start_tick)
		return nullptr;
	Tick_t index = p_tick - _state_hashes_start_tick;
	if (index >= state_hashes.size())
		return nullptr;
	ERR_THROW(state_hashes[index]->tick != p_tick, "State hash frame retreived from state hashes has mismatched tick number");
	return state_hashes[index];
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
#ifdef LOG_ENABLE
		if (logger)
			logger->add_peer_statues(peer);
#endif
	}
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

void ISyncManager::send_input_messages_to_peer(const PeerId_t &peer_id) {
	ERR_THROW(peer_id == network_adaptor->get_network_peer_id(), "Cannot send input to ourselves");
	ERR_THROW(peers.count(peer_id) == 0, std::bad_exception());

	const auto peer = peers[peer_id];

	std::map<Tick_t, Hash_t> state_hashes;
	get_state_hashes_for_peer(peer, state_hashes);

	// TODO:: the capacity of this std::vector should be specify.
	std::vector<std::map<Tick_t, SerializedData>> input_messages(_input_send_queue.size());
	get_input_messages_from_send_queue_for_peer(*peer, input_messages);

#ifdef LOG_ENABLE
	if (logger) {
		logger->add_message(string_sprintf("messages_sent_to_peer_%s", peer_id), std::to_string(input_messages.size()));
	}
#endif
	for (auto &inputs : input_messages) {
		SerializedData data;
		message_serializer->serialize_message(PeerFrameMessage(peer->last_received_remote_input_tick + 1, inputs, peer->last_received_remote_hash_tick + 1, state_hashes), data);
#ifdef DEBUG
		// See https://gafferongames.com/post/packet_fragmentation_and_reassembly/
		if (debug_message_bytes > 0) {
			if (data.size > debug_message_bytes)
				WARNING(string_sprintf("Sending message w/ size %D bytes", data.size));
		}
#endif

#ifdef LOG_ENABLE
		if (logger) {
			logger->add_value(string_sprintf("messages_sent_to_peer_%d_size", peer_id), std::to_string(data.size));
			logger->increment_value(string_sprintf("messages_sent_to_peer_%d_total_size", peer_id), data.size);
			jsonxx::Array ticks;
			for (auto &kv : inputs) {
				ticks << kv.first;
			}
			logger->merge_array_value(string_sprintf("input_ticks_sent_to_peer_%d", peer_id), ticks);
		}
#endif
		// var ticks = msg[InputMessageKey.INPUT].keys()
		// print ("[%s] Sending ticks %s - %s" % [current_tick, min(ticks[0], ticks[-1]), max(ticks[0], ticks[-1])])

		network_adaptor->send_input_tick(peer_id, data);
	}
}

void ISyncManager::send_input_messages_to_all_peers() {
#ifdef DEBUG
	if (debug_skip_nth_message > 1) {
		_debug_skip_nth_message_counter += 1;
		if (_debug_skip_nth_message_counter >= debug_skip_nth_message) {
			HINT(string_sprintf("[%d] Skipping message to simulate packet loss", current_tick));
			_debug_skip_nth_message_counter = 0;
			return;
		}
	}
#endif
	for (auto &kv : peers)
		send_input_messages_to_peer(kv.first);
}

#ifdef DEBUG
void ISyncManager::debug_check_consistent_local_state(const std::shared_ptr<StateBufferFrame> &p_state, const std::string &message) {
	auto hashed_state = p_state->get_data_hash();
	auto previously_hashed_frame = get_state_hash_frame(current_tick);
	auto previous_state = debug_check_local_state_consistency_buffer.front();
	debug_check_local_state_consistency_buffer.pop_front();
	if (previously_hashed_frame && previously_hashed_frame->state_hash != hashed_state) {
		auto comparer = DebugComparer();
		comparer.find_mismatches(previous_state.to_json_obj(), p_state->to_json_obj());
		ERR_THROW(true, message + " state is not consistent with saved state:\n " + comparer.get_mismatches_string());
	}
}
#endif

SharedPtr<ISpawnable>
ISyncManager::spawn(const SharedPtr<const ISpawner> &p_spawner, const SharedPtr<ISpawnData> &p_spawn_data) // name: String, parent: Node, scene: PackedScene, data: Dictionary = {}, rename: bool = true, signal_name: String = '')
{
	if (!started) {
		PUSH_ERR(string_sprintf("Refusing to spawn %s before SyncManager has started", std::to_string(p_spawn_data)));
		return nullptr;
	}

	return spawn_manager.spawn(p_spawner, p_spawn_data);
}
inline void ISyncManager::despawn(SharedPtr<ISpawnable> &p_despawnable) { spawn_manager.despawn(p_despawnable); }

void ISyncManager::on_input_tick_received(const PeerId_t &p_sender_peer_id, const SerializedData &p_serialized_data) {
	if (!started)
		return;
	PeerFrameMessage msg = message_serializer->deserialize_message(p_serialized_data);

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

#ifdef LOG_ENABLE
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
			auto remote_input = message_serializer->deserialize_input(msg.inputs[remote_tick]);

			auto input_frame = get_or_create_input_frame(remote_tick);
			if (input_frame == nullptr) {
				// _get_or_create_input_frame() will have already flagged the error,
				// so we can just return here.
				return;
			}

			// If we already have non-predicted input for this peer, then skip it.
			if (!input_frame->is_peer_input_predicated(p_sender_peer_id))
				continue;

#ifdef LOG_ENABLE
			HINT(string_sprintf("Received remote tick %Ds from %D", remote_tick, p_sender_peer_id));
			if (logger)
				logger->add_value(string_sprintf("remote_ticks_received_from_%s", p_sender_peer_id), std::to_string(remote_tick));
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