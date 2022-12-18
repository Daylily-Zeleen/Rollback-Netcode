/*
 * @Author       : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @Date         : 2022-12-08 11:13:47
 * @LastEditors  : Daylily-Zeleen daylily-zeleen@foxmail.com
 * @LastEditTime : 2022-12-14 16:46:24
 * @FilePath     : \rollback_netcode\include\core\interfaces\i_sound_manager.h
 * @Description  :
 * Copyright (c) 2022 by Daylily-Zeleen email: daylily-zeleen@foxmail.com, All Rights Reserved.
 */
#pragma once

#include <interfaces/i_sound.h>
#include <def.h>
#include <unordered_map>
#include <vector>

namespace rollback_netcode {

class SoundManager {
public:
	void setup_sound_manager(SharedPtr<IBase> &p_sync_manmager);
	void play_sound(const SoundIdentifier_t &p_identifier, const SharedPtr<ISound> &p_sound);

private:
	std::weak_ptr<IBase> sync_manager_ref;
	std::unordered_map<Tick_t, std::vector<SoundIdentifier_t>> ticks;

	void on_sync_manager_tick_retired(const Tick_t p_tick);
	void on_sync_manager_sync_stop();
};
} //namespace rollback_netcode