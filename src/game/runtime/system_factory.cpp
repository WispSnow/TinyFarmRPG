#include "system_factory.h"

#include "engine/component/layered_sprite_component.h"
#include "engine/core/context.h"
#include "engine/debug/panels/spatial_index_debug_panel.h"
#include "engine/input/input_manager.h"
#include "engine/render/camera.h"
#include "engine/system/animation_system.h"
#include "engine/system/audio_system.h"
#include "engine/system/auto_tile_system.h"
#include "engine/system/debug_render_system.h"
#include "engine/system/light_system.h"
#include "engine/system/movement_system.h"
#include "engine/system/remove_entity_system.h"
#include "engine/system/render_system.h"
#include "engine/system/spatial_index_system.h"
#include "engine/system/ysort_system.h"
#include "engine/vfx/vfx_bridge_system.h"
#include "game/component/appearance_component.h"
#include "game/data/appearance_catalog.h"
#include "game/data/item_catalog.h"
#include "game/data/quest_catalog.h"
#include "game/data/rpg_catalog.h"
#include "game/data/shop_catalog.h"
#include "game/defs/commands.h"
#include "game/domain/equipment_domain_service.h"
#include "game/domain/inventory_domain_service.h"
#include "game/domain/quest_turn_in_service.h"
#include "game/domain/shop_transaction_service.h"
#include "game/factory/blueprint_manager.h"
#include "game/factory/entity_factory.h"
#include "game/runtime/game_content_manifest.h"
#include "game/script/script_event_bridge.h"
#include "game/system/action_sound_system.h"
#include "game/system/animal_behavior_system.h"
#include "game/system/appearance_system.h"
#include "game/system/animation_event_system.h"
#include "game/system/camera_follow_system.h"
#include "game/system/chest_system.h"
#include "game/system/closet_interaction_system.h"
#include "game/system/crop_system.h"
#include "game/system/day_night_system.h"
#include "game/system/dialogue_system.h"
#include "game/system/enemy_encounter_system.h"
#include "game/system/equipment_system.h"
#include "game/system/farm_system.h"
#include "game/system/hotbar_system.h"
#include "game/system/interaction_system.h"
#include "game/system/inventory_system.h"
#include "game/system/item_use_system.h"
#include "game/system/light_toggle_system.h"
#include "game/system/map_transition_system.h"
#include "game/system/npc_wander_system.h"
#include "game/system/party_recruitment_system.h"
#include "game/system/pickup_system.h"
#include "game/system/player_control_system.h"
#include "game/system/quest_interaction_system.h"
#include "game/system/recruitment_interaction_system.h"
#include "game/system/render_target_system.h"
#include "game/system/rest_system.h"
#include "game/system/scripted_dialogue_lifecycle_system.h"
#include "game/system/shop_interaction_system.h"
#include "game/system/state_system.h"
#include "game/system/time_of_day_light_system.h"
#include "game/system/time_system.h"
#include "game/world/map_manager.h"
#include "game/world/world_state.h"

#ifdef TF_ENABLE_DEBUG_UI
#include "engine/debug/debug_ui_manager.h"
#endif

#include <entt/entity/registry.hpp>
#include <spdlog/spdlog.h>

#include <memory>

namespace game::runtime {

bool SystemFactory::assemble(GameRuntimeAssembler::SystemBuildParams params) {
    auto& services = params.services;
    if (!services.collision_resolver || !services.entity_factory || !services.blueprint_manager ||
        !services.item_catalog || !services.appearance_catalog || !services.world_state || !services.map_manager ||
        !services.vfx_service || !services.quest_catalog || !services.shop_catalog) {
        spdlog::error("Runtime services 未完成装配，无法创建 systems");
        return false;
    }

    auto& systems = params.systems;

    auto& dispatcher = params.context.getDispatcher();
    auto& input_manager = params.context.getInputManager();
    auto& camera = params.context.getCamera();
    auto& spatial_index_manager = params.context.getSpatialIndexManager();
    auto& auto_tile_library = params.context.getAutoTileLibrary();

#ifdef TF_ENABLE_DEBUG_UI
    auto& debug_ui_manager = params.context.getDebugUIManager();
    auto* spatial_panel = debug_ui_manager.getPanel<engine::debug::SpatialIndexDebugPanel>(
        engine::debug::PanelCategory::Engine);
#endif

    if (!services.inventory_domain_service) {
        services.inventory_domain_service = std::make_unique<game::domain::InventoryDomainService>(
            params.registry,
            dispatcher,
            *services.item_catalog);
    }
    if (!services.equipment_domain_service) {
        services.equipment_domain_service = std::make_unique<game::domain::EquipmentDomainService>(
            params.registry,
            dispatcher,
            *services.rpg_catalog,
            *services.item_catalog,
            *services.inventory_domain_service);
    }
    if (!services.quest_turn_in_service) {
        services.quest_turn_in_service = std::make_unique<game::domain::QuestTurnInService>(
            params.registry,
            *services.item_catalog,
            *services.inventory_domain_service);
    }
    if (!services.shop_transaction_service) {
        services.shop_transaction_service = std::make_unique<game::domain::ShopTransactionService>(
            params.registry,
            *services.item_catalog,
            *services.shop_catalog,
            *services.inventory_domain_service);
    }

    systems.render_system = std::make_unique<engine::system::RenderSystem>();
    systems.light_system = std::make_unique<engine::system::LightSystem>();
    systems.ysort_system = std::make_unique<engine::system::YSortSystem>();
#ifdef TF_ENABLE_DEBUG_UI
    systems.debug_render_system =
        std::make_unique<engine::system::DebugRenderSystem>(spatial_index_manager, spatial_panel);
#endif
    systems.movement_system = std::make_unique<engine::system::MovementSystem>(services.collision_resolver.get());
    systems.spatial_index_system = std::make_unique<engine::system::SpatialIndexSystem>(spatial_index_manager);
    systems.animation_system = std::make_unique<engine::system::AnimationSystem>(params.registry, dispatcher);
    systems.appearance_system = std::make_unique<game::system::AppearanceSystem>(
        params.registry,
        dispatcher,
        *services.appearance_catalog);
    systems.vfx_bridge_system = std::make_unique<engine::vfx::VfxBridgeSystem>(
        dispatcher,
        *services.vfx_service,
        services.vfx_catalog.get());
    {
        auto layered_view =
            params.registry.view<game::component::AppearanceComponent, engine::component::LayeredSpriteComponent>();
        for (const auto entity : layered_view) {
            dispatcher.trigger(game::defs::RefreshAppearanceCommand{entity});
        }
    }

    systems.state_system = std::make_unique<game::system::StateSystem>(params.registry, dispatcher);
    systems.action_sound_system = std::make_unique<game::system::ActionSoundSystem>(params.registry, dispatcher);
    systems.player_control_system = std::make_unique<game::system::PlayerControlSystem>(
        params.registry,
        dispatcher,
        input_manager,
        camera,
        spatial_index_manager,
        services.item_catalog.get());
    systems.camera_follow_system =
        std::make_unique<game::system::CameraFollowSystem>(params.registry, camera, input_manager);
    systems.auto_tile_system = std::make_unique<engine::system::AutoTileSystem>(
        params.registry,
        dispatcher,
        auto_tile_library,
        spatial_index_manager);
    systems.farm_system = std::make_unique<game::system::FarmSystem>(
        params.registry,
        dispatcher,
        spatial_index_manager,
        *services.entity_factory,
        *services.blueprint_manager,
        services.item_catalog.get(),
        *services.inventory_domain_service);
    systems.pickup_system = std::make_unique<game::system::PickupSystem>(
        params.registry,
        dispatcher,
        *services.inventory_domain_service);
    systems.render_target_system = std::make_unique<game::system::RenderTargetSystem>(params.registry);
    systems.animation_event_system = std::make_unique<game::system::AnimationEventSystem>(params.registry, dispatcher);

    systems.time_system = std::make_unique<game::system::TimeSystem>(params.registry, dispatcher);
    systems.time_of_day_light_system =
        std::make_unique<game::system::TimeOfDayLightSystem>(params.registry, dispatcher);
    systems.day_night_system = std::make_unique<game::system::DayNightSystem>(params.registry);
    systems.light_toggle_system = std::make_unique<game::system::LightToggleSystem>(
        params.registry,
        dispatcher,
        GameContentManifest::LightConfig);

    systems.npc_wander_system = std::make_unique<game::system::NPCWanderSystem>(params.registry);
    systems.animal_behavior_system = std::make_unique<game::system::AnimalBehaviorSystem>(params.registry);
    systems.dialogue_system = std::make_unique<game::system::DialogueSystem>(params.registry, dispatcher);
    systems.dialogue_system->loadDialogueFile(GameContentManifest::DialogueScript);
    systems.scripted_dialogue_lifecycle_system =
        std::make_unique<game::system::ScriptedDialogueLifecycleSystem>(params.registry, dispatcher);
    systems.quest_interaction_system = std::make_unique<game::system::QuestInteractionSystem>(
        params.registry,
        dispatcher,
        *services.quest_catalog,
        *services.quest_turn_in_service);
    systems.recruitment_interaction_system = std::make_unique<game::system::RecruitmentInteractionSystem>(
        params.registry,
        dispatcher,
        *services.rpg_catalog);
    systems.party_recruitment_system = std::make_unique<game::system::PartyRecruitmentSystem>(
        params.registry,
        dispatcher,
        *services.rpg_catalog,
        &spatial_index_manager);
    systems.shop_interaction_system = std::make_unique<game::system::ShopInteractionSystem>(
        params.registry,
        params.context,
        *services.shop_catalog,
        *services.item_catalog,
        *services.shop_transaction_service);
    systems.enemy_encounter_system = std::make_unique<game::system::EnemyEncounterSystem>(
        params.registry,
        dispatcher,
        spatial_index_manager,
        *services.world_state,
        services.rpg_catalog.get());

    systems.chest_system = std::make_unique<game::system::ChestSystem>(
        params.registry,
        dispatcher,
        *services.world_state,
        *services.item_catalog,
        *services.inventory_domain_service);
    systems.interaction_system = std::make_unique<game::system::InteractionSystem>(
        params.registry,
        dispatcher,
        input_manager,
        spatial_index_manager,
        *services.world_state);
    systems.rest_system = std::make_unique<game::system::RestSystem>(
        params.registry,
        params.context,
        services.rpg_catalog.get());
    systems.closet_interaction_system = std::make_unique<game::system::ClosetInteractionSystem>(
        params.registry,
        params.context,
        services.appearance_catalog);

    systems.inventory_system = std::make_unique<game::system::InventorySystem>(
        params.registry,
        dispatcher,
        *services.item_catalog,
        *services.inventory_domain_service);
    systems.equipment_system = std::make_unique<game::system::EquipmentSystem>(
        dispatcher,
        *services.equipment_domain_service);
    systems.hotbar_system = std::make_unique<game::system::HotbarSystem>(
        params.registry,
        dispatcher,
        services.item_catalog.get());
    systems.item_use_system = std::make_unique<game::system::ItemUseSystem>(
        params.registry,
        dispatcher,
        *services.item_catalog,
        *services.inventory_domain_service,
        services.rpg_catalog.get());

    if (!systems.day_night_system->loadConfig(GameContentManifest::LightConfig)) {
        spdlog::warn("光照配置加载失败，将使用默认配置");
    }

    systems.crop_system = std::make_unique<game::system::CropSystem>(
        params.registry,
        dispatcher,
        spatial_index_manager,
        *services.blueprint_manager);

    systems.remove_entity_system = std::make_unique<engine::system::RemoveEntitySystem>();
    systems.audio_system = std::make_unique<engine::system::AudioSystem>(params.registry, params.context);

    systems.map_transition_system = std::make_unique<game::system::MapTransitionSystem>(
        params.registry,
        dispatcher,
        *services.world_state,
        *services.map_manager,
        services.collision_resolver.get());

    if (services.script_host) {
        systems.script_event_bridge = std::make_unique<game::script::ScriptEventBridge>(
            *services.script_host,
            params.registry,
            dispatcher);
    }

    return true;
}

} // namespace game::runtime
