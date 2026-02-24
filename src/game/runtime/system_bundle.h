#pragma once

#include "engine/system/fwd.h"
#include "game/system/fwd.h"

#include <memory>

namespace engine::spatial {
class CollisionResolver;
}

namespace game::factory {
class BlueprintManager;
class EntityFactory;
}

namespace game::data {
class ItemCatalog;
}

namespace game::domain {
class InventoryDomainService;
}

namespace game::save {
class SaveService;
}

namespace game::world {
class WorldState;
class MapManager;
}

#ifdef TF_ENABLE_SCRIPTING
namespace engine::script {
class ScriptHost;
}
#endif

namespace game::runtime {

struct GameRuntimeServices {
    std::shared_ptr<game::factory::BlueprintManager> blueprint_manager;
    std::shared_ptr<game::data::ItemCatalog> item_catalog;

    std::unique_ptr<engine::spatial::CollisionResolver> collision_resolver;
    std::unique_ptr<game::world::WorldState> world_state;
    std::unique_ptr<game::factory::EntityFactory> entity_factory;
    std::unique_ptr<game::world::MapManager> map_manager;
    std::unique_ptr<game::domain::InventoryDomainService> inventory_domain_service;
    std::unique_ptr<game::save::SaveService> save_service;
#ifdef TF_ENABLE_SCRIPTING
    std::unique_ptr<engine::script::ScriptHost> script_host;
#endif

    GameRuntimeServices();
    ~GameRuntimeServices() noexcept;
    GameRuntimeServices(GameRuntimeServices&&) noexcept;
    GameRuntimeServices& operator=(GameRuntimeServices&&) noexcept;

    GameRuntimeServices(const GameRuntimeServices&) = delete;
    GameRuntimeServices& operator=(const GameRuntimeServices&) = delete;
};

struct GameSystemBundle {
    std::unique_ptr<engine::system::RenderSystem> render_system;
    std::unique_ptr<engine::system::LightSystem> light_system;
    std::unique_ptr<engine::system::YSortSystem> ysort_system;
#ifdef TF_ENABLE_DEBUG_UI
    std::unique_ptr<engine::system::DebugRenderSystem> debug_render_system;
#endif
    std::unique_ptr<engine::system::MovementSystem> movement_system;
    std::unique_ptr<engine::system::AnimationSystem> animation_system;
    std::unique_ptr<engine::system::SpatialIndexSystem> spatial_index_system;
    std::unique_ptr<engine::system::AutoTileSystem> auto_tile_system;
    std::unique_ptr<engine::system::RemoveEntitySystem> remove_entity_system;
    std::unique_ptr<engine::system::AudioSystem> audio_system;

    std::unique_ptr<game::system::StateSystem> state_system;
    std::unique_ptr<game::system::ActionSoundSystem> action_sound_system;
    std::unique_ptr<game::system::PlayerControlSystem> player_control_system;
    std::unique_ptr<game::system::CameraFollowSystem> camera_follow_system;
    std::unique_ptr<game::system::FarmSystem> farm_system;
    std::unique_ptr<game::system::PickupSystem> pickup_system;
    std::unique_ptr<game::system::RenderTargetSystem> render_target_system;
    std::unique_ptr<game::system::AnimationEventSystem> animation_event_system;
    std::unique_ptr<game::system::TimeSystem> time_system;
    std::unique_ptr<game::system::TimeOfDayLightSystem> time_of_day_light_system;
    std::unique_ptr<game::system::LightToggleSystem> light_toggle_system;
    std::unique_ptr<game::system::DayNightSystem> day_night_system;
    std::unique_ptr<game::system::CropSystem> crop_system;
    std::unique_ptr<game::system::InventorySystem> inventory_system;
    std::unique_ptr<game::system::HotbarSystem> hotbar_system;
    std::unique_ptr<game::system::ItemUseSystem> item_use_system;
    std::unique_ptr<game::system::NPCWanderSystem> npc_wander_system;
    std::unique_ptr<game::system::AnimalBehaviorSystem> animal_behavior_system;
    std::unique_ptr<game::system::DialogueSystem> dialogue_system;
    std::unique_ptr<game::system::ChestSystem> chest_system;
    std::unique_ptr<game::system::InteractionSystem> interaction_system;
    std::unique_ptr<game::system::RestSystem> rest_system;
    std::unique_ptr<game::system::MapTransitionSystem> map_transition_system;

    GameSystemBundle();
    ~GameSystemBundle() noexcept;
    GameSystemBundle(GameSystemBundle&&) noexcept;
    GameSystemBundle& operator=(GameSystemBundle&&) noexcept;

    GameSystemBundle(const GameSystemBundle&) = delete;
    GameSystemBundle& operator=(const GameSystemBundle&) = delete;
};

} // namespace game::runtime
