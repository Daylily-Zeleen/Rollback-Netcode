#include "thirdparty/jsonxx/jsonxx.h"
#include <interfaces/i_sync_manager.h>
#include <core/sound_manager.h>
#include <sys/stat.h>
#include <algorithm>
#include <functional>
#include <numeric>

namespace rollback_netcode {

void SoundManager::setup_sound_manager(SharedPtr<IBase> &p_sync_manmager) {
	ERR_THROW(dynamic_cast<ISyncManager *>(p_sync_manmager.get()) != nullptr, "ERROR_TYPE");
	sync_manager_ref = p_sync_manmager;
	auto *sync_manager = dynamic_cast<ISyncManager *>(p_sync_manmager.get());
	sync_manager->event_tick_retired.connect([this](const Tick_t p_tick) { on_sync_manager_tick_retired(p_tick); });
	sync_manager->event_sync_stopped.connect([this]() { on_sync_manager_sync_stop(); });
}

void SoundManager::play_sound(const SoundIdentifier_t &p_identifier, const SharedPtr<ISound> &p_sound) {
	ERR_THROW(sync_manager_ref.expired() == false, "ERROR sync_manager_ref.expired()");
	ERR_THROW(dynamic_cast<ISyncManager *>(sync_manager_ref.lock().get()) == nullptr, "ERROR_TYPE");
	static const ISyncManager *sync_manager = dynamic_cast<ISyncManager *>(sync_manager_ref.lock().get());
	ERR_THROW(sync_manager == nullptr, "ERROR_TYPE");
	if (sync_manager->is_respawning())
		return;

	auto it = ticks.find(sync_manager->get_current_tick());
	if (it != ticks.end()) {
		if (std::find(it->second.begin(), it->second.end(), p_identifier) != it->second.end())
			return;
		else {
			it->second.emplace_back(p_identifier);
		}
	} else {
		ticks.emplace(sync_manager->get_current_tick(), std::vector<SoundIdentifier_t>({ p_identifier }));
	}

	p_sound->play();
}

void SoundManager::on_sync_manager_tick_retired(const Tick_t p_tick) {
	ticks.erase(p_tick);
}

void SoundManager::on_sync_manager_sync_stop() {
	ticks.clear();
}

} //namespace rollback_netcode