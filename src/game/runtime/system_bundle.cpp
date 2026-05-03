#include "system_bundle.h"

#include "engine/system/render_system.h"
#include "engine/system/light_system.h"
#include "engine/system/ysort_system.h"
#include "engine/system/debug_render_system.h"
#include "engine/system/movement_system.h"
#include "engine/system/animation_system.h"
#include "engine/system/spatial_index_system.h"
#include "engine/system/auto_tile_system.h"
#include "engine/system/remove_entity_system.h"
#include "engine/system/audio_system.h"
#include "engine/spatial/collision_resolver.h"
#include "game/system/state_system.h"
#include "game/system/action_sound_system.h"
#include "game/system/player_control_system.h"
#include "game/system/camera_follow_system.h"
#include "game/system/farm_system.h"
#include "game/system/pickup_system.h"
#include "game/system/render_target_system.h"
#include "game/system/animation_event_system.h"
#include "game/system/time_system.h"
#include "game/system/time_of_day_light_system.h"
#include "game/system/light_toggle_system.h"
#include "game/system/day_night_system.h"
#include "game/system/crop_system.h"
#include "game/system/inventory_system.h"
#include "game/system/hotbar_system.h"
#include "game/system/item_use_system.h"
#include "game/system/npc_wander_system.h"
#include "game/system/animal_behavior_system.h"
#include "game/system/dialogue_system.h"
#include "game/system/enemy_encounter_system.h"
#include "game/system/quest_interaction_system.h"
#include "game/system/shop_interaction_system.h"
#include "game/system/chest_system.h"
#include "game/system/interaction_system.h"
#include "game/system/rest_system.h"
#include "game/system/map_transition_system.h"
#include "game/system/appearance_system.h"
#include "engine/vfx/vfx_bridge_system.h"
#include "game/domain/inventory_domain_service.h"
#include "game/domain/quest_turn_in_service.h"
#include "game/domain/shop_transaction_service.h"
#include "game/factory/entity_factory.h"
#include "game/factory/blueprint_manager.h"
#include "game/data/item_catalog.h"
#include "game/data/appearance_catalog.h"
#include "game/data/quest_catalog.h"
#include "game/data/shop_catalog.h"
#include "engine/vfx/vfx_catalog.h"
#include "game/save/save_service.h"
#include "game/world/world_state.h"
#include "game/world/map_manager.h"
#include "engine/vfx/vfx_service.h"
#include "engine/script/script_host.h"

namespace game::runtime {

GameRuntimeServices::GameRuntimeServices() = default;
GameRuntimeServices::~GameRuntimeServices() noexcept = default;
GameRuntimeServices::GameRuntimeServices(GameRuntimeServices&&) noexcept = default;
GameRuntimeServices& GameRuntimeServices::operator=(GameRuntimeServices&&) noexcept = default;

GameSystemBundle::GameSystemBundle() = default;
GameSystemBundle::~GameSystemBundle() noexcept = default;
GameSystemBundle::GameSystemBundle(GameSystemBundle&&) noexcept = default;
GameSystemBundle& GameSystemBundle::operator=(GameSystemBundle&&) noexcept = default;

} // namespace game::runtime
