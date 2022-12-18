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
#include "interfaces/i_match_info.h"
#include "interfaces/i_network_adapeor.h"
#include "interfaces/i_sound.h"
#include "interfaces/i_spawn_data.h"
#include <core/logger.h>
#include <core/state_hash_frame.h>
#include <def.h>
#include <interfaces/i_input.h>
#include <interfaces/i_message_serializer.h>
#include <interfaces/i_network_proccessable.h>
#include <interfaces/i_peer.h>
#include <interfaces/i_timer.h>

#include <thirdparty/deterministic_float/glacier_float.h>
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
	Event<std::function<void(const Tick_t p_tick, const PeerId_t &p_peer_id, const SharedPtr<IInput> &p_local_input, const SharedPtr<IInput> &p_remote_input)>> event_prediction_missed;
	Event<std::function<void(const Tick_t p_tick, const PeerId_t &p_peer_id, const Hash_t &local_hash, const Hash_t &remote_hash)>> event_remote_state_mismatch;

	Event<std::function<void(const PeerId_t &p_peer_id)>> event_peer_added;
	Event<std::function<void(const PeerId_t &p_peer_id)>> event_peer_removed;
	Event<std::function<void(const IPeer &p_peer)>> event_peer_pinged_back;

	Event<std::function<void(const Tick_t p_rollback_ticks)>> event_state_loaded;
	Event<std::function<void(const bool p_rollback)>> event_tick_finished;
	Event<std::function<void(const Tick_t p_rollback)>> event_tick_retired;
	Event<std::function<void(const Tick_t p_tick)>> event_tick_input_completed;

	Event<std::function<void(SharedPtr<ISpawnable> &p_spawned_obj, const SharedPtr<ISpawnData> &p_spawned_data)>> event_spawned;
	Event<std::function<void(SharedPtr<ISpawnable> &p_despawned_obj, const SharedPtr<ISpawnData> &p_spawned_data)>> event_predespawn;
	Event<std::function<void()>> event_interpolation_frame;

	void set_mechanized(const bool p_mechanized);
	void set_input_delay(const uint8_t &p_input_delay);
	void set_ping_frequency(const float &p_ping_frequency);
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

	ISyncManager(SharedPtr<INetworkAdaptor> &p_network_adaptor, SharedPtr<IMessageSerializer> &p_message_serializer, SharedPtr<ITimer> &p_ping_timer);

	virtual ~ISyncManager() { stop_logging(); }

	void start_logging(const std::string &p_log_file_path, const SharedPtr<IMatchInfo> &p_match_info);

	void stop_logging();

	void add_peer(SharedPtr<IPeer> &p_peer);

	void remove_peer(const PeerId_t &p_peer_id);
	bool has_peer(const PeerId_t &p_peer_id) const { return peers.find(p_peer_id) != peers.end(); }
	SharedPtr<IPeer> get_peer(const PeerId_t &p_peer_id) const;

	void start(const SharedPtr<ITimer> &p_delay_timer);
	void stop();

	const SharedPtr<const IInput> get_latest_input_from_peer(const PeerId_t &p_peer_id) const;

	void process_logic_tick();
	void process_frame(const float delta);

	void reset_mechanized_data();
	void execute_mechanized_tick();
	void execute_mechanized_interpolation_frame(float delta);
	void execute_mechanized_interframe();
	void process_mechanized_input();

	FORCE_INLINE_ SharedPtr<ISpawnable> spawn(const SharedPtr<const ISpawner> &p_spawner, const SharedPtr<ISpawnData> &p_spawn_data);

	FORCE_INLINE_ void despawn(SharedPtr<ISpawnable> &p_despawnable);

	FORCE_INLINE_ bool is_in_rollback() const { return _in_rollback; }
	FORCE_INLINE_ bool is_respawning() const { return spawn_manager.is_respawning(); }

	FORCE_INLINE_ void play_sound(const SoundIdentifier_t &p_identifier, const SharedPtr<ISound> &p_sound) { sound_manager.play_sound(p_identifier, p_sound); }

	FORCE_INLINE_ bool ensure_current_tick_input_complete();

	static ISyncManager *get_singleton() { return singleton; }

protected:
	FORCE_INLINE_ SharedPtr<const IInput> call_get_local_input();
	FORCE_INLINE_ void call_network_process(const InputBufferFrame &p_input_buffer_frame);
	void call_save_state(std::map<UUID_t, SharedPtr<const IState>> &r_saved_state_frame);
	void call_load_state(const std::map<UUID_t, SharedPtr<const IState>> &p_state_frame);
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
	FORCE_INLINE_ void on_spawned(SharedPtr<ISpawnable> &p_spawned_obj, const SharedPtr<const ISpawnData> &p_spawned_data);
	FORCE_INLINE_ void on_predespawn(SharedPtr<ISpawnable> &p_despawned_obj, const SharedPtr<const ISpawnData> &p_spawned_data);
	FORCE_INLINE_ void on_ping_received(const PeerId_t &p_peer_id, PingMessage &p_peer_msg);
	FORCE_INLINE_ void on_ping_back_received(const PeerId_t &p_peer_id, PingMessage &p_peer_msg);

	FORCE_INLINE_ void on_remote_start_received();
	FORCE_INLINE_ void on_remote_stop_received();
	FORCE_INLINE_ void on_input_tick_received(const PeerId_t &p_sender_peer_id, const SerializedData &p_serialized_data);

private:
	SharedPtr<INetworkAdaptor> network_adaptor = nullptr;
	SharedPtr<IMessageSerializer> message_serializer = nullptr;

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

#ifdef LOG_ENABLE
	std::unique_ptr<Logger> logger;
#endif

#ifdef DEBUG
	uint8_t debug_rollback_ticks = 0;
	uint8_t debug_random_rollback_ticks = 0;
	uint8_t debug_skip_nth_message = 0;
	float debug_physics_process_msecs = 10.0;
	float debug_process_msecs = 10.0;
	bool debug_check_message_serializer_roundtrip = false;
	bool debug_check_local_state_consistency = false;
	uint32_t debug_message_bytes = 700;
	std::vector<rollback_netcode::StateBufferFrame> debug_check_local_state_consistency_buffer;
#endif
	// In seconds, because we don't want it to be dependent on the network tick.
	float ping_frequency = 1.0;

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
	std::map<UUID_t, std::pair<SharedPtr<const IState>, SharedPtr<const IState>>> _interpolation_state;
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

	FORCE_INLINE_ void send_input_messages_to_peer(const PeerId_t &peer_id);

	FORCE_INLINE_ void send_input_messages_to_all_peers();

#ifdef DEBUG
	void debug_check_consistent_local_state(const std::shared_ptr<StateBufferFrame> &p_state, const std::string &message = "Loaded");
#endif

	friend INetwokProcessable;
	friend INetworkRollbackable;
};

} // namespace rollback_netcode