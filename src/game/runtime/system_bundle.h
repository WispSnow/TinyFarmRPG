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
class AppearanceCatalog;
class RpgCatalog;
class QuestCatalog;
class ShopCatalog;
class AudioCueCatalog;
}

namespace game::domain {
class InventoryDomainService;
class EquipmentDomainService;
class QuestTurnInService;
class ShopTransactionService;
}

namespace game::save {
class SaveService;
}

namespace game::world {
class WorldState;
class MapManager;
}

namespace engine::script {
class ScriptHost;
}

namespace engine::vfx {
class VfxService;
class VfxCatalog;
class VfxBridgeSystem;
}

namespace game::runtime {

class UserSettingsService;

struct GameRuntimeServices {
    std::shared_ptr<game::factory::BlueprintManager> blueprint_manager;
    std::shared_ptr<game::data::ItemCatalog> item_catalog;
    std::shared_ptr<game::data::AppearanceCatalog> appearance_catalog;
    std::shared_ptr<engine::vfx::VfxCatalog> vfx_catalog;
    std::shared_ptr<game::data::RpgCatalog> rpg_catalog;
    std::shared_ptr<game::data::QuestCatalog> quest_catalog;
    std::shared_ptr<game::data::ShopCatalog> shop_catalog;
    std::shared_ptr<game::data::AudioCueCatalog> audio_cue_catalog;

    std::unique_ptr<engine::spatial::CollisionResolver> collision_resolver;
    std::unique_ptr<game::world::WorldState> world_state;
    std::unique_ptr<game::factory::EntityFactory> entity_factory;
    std::unique_ptr<game::world::MapManager> map_manager;
    std::unique_ptr<game::domain::InventoryDomainService> inventory_domain_service;
    std::unique_ptr<game::domain::EquipmentDomainService> equipment_domain_service;
    std::unique_ptr<game::domain::QuestTurnInService> quest_turn_in_service;
    std::unique_ptr<game::domain::ShopTransactionService> shop_transaction_service;
    std::unique_ptr<game::save::SaveService> save_service;
    std::unique_ptr<engine::vfx::VfxService> vfx_service;
    std::unique_ptr<engine::script::ScriptHost> script_host;
    std::unique_ptr<UserSettingsService> user_settings_service;

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
    std::unique_ptr<game::system::EquipmentSystem> equipment_system;
    std::unique_ptr<game::system::HotbarSystem> hotbar_system;
    std::unique_ptr<game::system::ItemUseSystem> item_use_system;
    std::unique_ptr<game::system::NPCWanderSystem> npc_wander_system;
    std::unique_ptr<game::system::AnimalBehaviorSystem> animal_behavior_system;
    std::unique_ptr<game::system::DialogueSystem> dialogue_system;
    std::unique_ptr<game::system::QuestInteractionSystem> quest_interaction_system;
    std::unique_ptr<game::system::RecruitmentInteractionSystem> recruitment_interaction_system;
    std::unique_ptr<game::system::PartyRecruitmentSystem> party_recruitment_system;
    std::unique_ptr<game::system::ShopInteractionSystem> shop_interaction_system;
    std::unique_ptr<game::system::EnemyEncounterSystem> enemy_encounter_system;
    std::unique_ptr<game::system::ChestSystem> chest_system;
    std::unique_ptr<game::system::InteractionSystem> interaction_system;
    std::unique_ptr<game::system::RestSystem> rest_system;
    std::unique_ptr<game::system::ClosetInteractionSystem> closet_interaction_system;
    std::unique_ptr<game::system::MapTransitionSystem> map_transition_system;
    std::unique_ptr<game::system::AppearanceSystem> appearance_system;
    std::unique_ptr<engine::vfx::VfxBridgeSystem> vfx_bridge_system;

    GameSystemBundle();
    ~GameSystemBundle() noexcept;
    GameSystemBundle(GameSystemBundle&&) noexcept;
    GameSystemBundle& operator=(GameSystemBundle&&) noexcept;

    GameSystemBundle(const GameSystemBundle&) = delete;
    GameSystemBundle& operator=(const GameSystemBundle&) = delete;
};

} // namespace game::runtime
