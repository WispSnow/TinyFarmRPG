#include "game_runtime_assembler.h"

#include "engine/core/context.h"
#include "engine/debug/panels/spatial_index_debug_panel.h"
#include "engine/input/input_manager.h"
#include "engine/render/camera.h"
#include "engine/resource/resource_manager.h"
#include "engine/spatial/collision_resolver.h"
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
#ifdef TF_ENABLE_DEBUG_UI
#include "engine/debug/debug_ui_manager.h"
#endif
#include "engine/scene/scene.h"
#include "game/factory/blueprint_manager.h"
#include "game/factory/entity_factory.h"
#include "game/data/game_time.h"
#include "game/data/item_catalog.h"
#include "game/domain/inventory_domain_service.h"
#include "game/save/save_service.h"
#include "game/system/action_sound_system.h"
#include "game/system/animal_behavior_system.h"
#include "game/system/animation_event_system.h"
#include "game/system/camera_follow_system.h"
#include "game/system/chest_system.h"
#include "game/system/crop_system.h"
#include "game/system/day_night_system.h"
#include "game/system/dialogue_system.h"
#include "game/system/farm_system.h"
#include "game/system/hotbar_system.h"
#include "game/system/interaction_system.h"
#include "game/system/inventory_system.h"
#include "game/system/item_use_system.h"
#include "game/system/light_toggle_system.h"
#include "game/system/map_transition_system.h"
#include "game/system/npc_wander_system.h"
#include "game/system/player_control_system.h"
#include "game/system/pickup_system.h"
#include "game/system/render_target_system.h"
#include "game/system/rest_system.h"
#include "game/system/state_system.h"
#include "game/system/time_of_day_light_system.h"
#include "game/system/time_system.h"
#include "game/world/map_manager.h"
#include "game/world/map_loading_settings.h"
#include "game/world/world_state.h"

#include <entt/core/hashed_string.hpp>
#include <spdlog/spdlog.h>

using namespace entt::literals;

namespace {

[[nodiscard]] bool ensureBlueprintManager(game::runtime::GameRuntimeServices& services) {
    if (!services.blueprint_manager) {
        services.blueprint_manager = std::make_shared<game::factory::BlueprintManager>();
        if (!services.blueprint_manager->loadActorBlueprints("assets/data/actor_blueprint.json")) {
            spdlog::error("加载角色蓝图失败");
            return false;
        }
        if (!services.blueprint_manager->loadAnimalBlueprints("assets/data/animal_blueprint.json")) {
            spdlog::error("加载动物蓝图失败");
            return false;
        }
        if (!services.blueprint_manager->loadCropBlueprints("assets/data/crop_config.json")) {
            spdlog::error("加载作物蓝图失败");
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool ensureItemCatalog(game::runtime::GameRuntimeServices& services) {
    if (!services.item_catalog) {
        services.item_catalog = std::make_shared<game::data::ItemCatalog>();
        if (!services.item_catalog->loadIconConfig("assets/data/icon_config.json")) {
            spdlog::error("加载物品图标配置失败");
            return false;
        }
        if (!services.item_catalog->loadItemConfig("assets/data/item_config.json")) {
            spdlog::error("加载物品配置失败");
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool ensureGameTime(entt::registry& registry, std::shared_ptr<game::data::GameTime>& game_time) {
    if (!game_time) {
        game_time = game::data::GameTime::loadFromConfig("assets/data/game_time_config.json");
        if (!game_time) {
            spdlog::error("从配置文件加载 GameTime 失败");
            return false;
        }
    }

    game_time->loadEmissiveVisibilityFromLightConfig("assets/data/light_config.json");

    if (auto* ctx_game_time = registry.ctx().find<game::data::GameTime>()) {
        *ctx_game_time = *game_time;
    } else {
        registry.ctx().emplace<game::data::GameTime>(*game_time);
    }

    spdlog::trace("已将 GameTime 放入注册表上下文。");
    return true;
}

[[nodiscard]] bool initWorldState(entt::registry& registry, game::runtime::GameRuntimeServices& services) {
    if (!services.world_state) {
        services.world_state = std::make_unique<game::world::WorldState>();
    }

    if (!services.world_state->loadFromWorldFile("assets/maps/farm-rpg.world", "home_exterior"_hs)) {
        spdlog::error("加载 world 布局失败");
        return false;
    }

    (void)services.world_state->ensureExternalMap("home_interior");

    if (auto* world_ptr = registry.ctx().find<game::world::WorldState*>()) {
        *world_ptr = services.world_state.get();
    } else {
        registry.ctx().emplace<game::world::WorldState*>(services.world_state.get());
    }

    return true;
}

[[nodiscard]] bool initFactory(engine::core::Context& context,
                               entt::registry& registry,
                               game::runtime::GameRuntimeServices& services) {
    if (!services.blueprint_manager) {
        return false;
    }

    auto& spatial_index_manager = context.getSpatialIndexManager();
    auto& auto_tile_library = context.getResourceManager().getAutoTileLibrary();

    services.entity_factory = std::make_unique<game::factory::EntityFactory>(
        registry,
        *services.blueprint_manager,
        &spatial_index_manager,
        &auto_tile_library);

    return true;
}

[[nodiscard]] bool initMapManager(engine::scene::Scene& scene,
                                  engine::core::Context& context,
                                  entt::registry& registry,
                                  game::runtime::GameRuntimeServices& services) {
    if (!services.world_state || !services.entity_factory || !services.blueprint_manager) {
        return false;
    }

    services.map_manager = std::make_unique<game::world::MapManager>(
        scene,
        context,
        registry,
        *services.world_state,
        *services.entity_factory,
        *services.blueprint_manager);

    const auto settings = game::world::MapLoadingSettings::loadFromFile("assets/data/map_loading_config.json");
    services.map_manager->setLoadingSettings(settings);

    spdlog::info("MapLoading: preload_mode={}, log_timings={}",
                 game::world::MapLoadingSettings::toString(settings.preload_mode),
                 settings.log_timings);

    if (settings.preload_mode == game::world::MapPreloadMode::All) {
        services.map_manager->preloadAllMaps();
    }

    return true;
}

[[nodiscard]] bool initSaveService(engine::core::Context& context,
                                   entt::registry& registry,
                                   game::runtime::GameRuntimeServices& services) {
    if (!services.map_manager || !services.world_state || !services.blueprint_manager) {
        spdlog::error("MapManager/WorldState/BlueprintManager 未初始化，无法创建 SaveService");
        return false;
    }

    services.save_service = std::make_unique<game::save::SaveService>(
        context,
        registry,
        *services.world_state,
        *services.map_manager,
        *services.blueprint_manager);

    return true;
}

[[nodiscard]] bool loadInitialMap(game::runtime::GameRuntimeServices& services) {
    if (!services.map_manager) {
        spdlog::error("MapManager 未初始化，无法加载地图");
        return false;
    }

    entt::id_type initial_map = entt::null;
    if (services.world_state) {
        initial_map = services.world_state->getCurrentMap();
        if (initial_map == entt::null) {
            if (const auto* map_state = services.world_state->getMapState("home_exterior")) {
                initial_map = map_state->info.id;
            }
        }
    }

    if (initial_map == entt::null) {
        initial_map = "home_exterior"_hs;
    }

    if (!services.map_manager->loadMap(initial_map)) {
        spdlog::error("加载关卡失败");
        return false;
    }

    return true;
}

void configureCamera(engine::core::Context& context) {
    auto& camera = context.getCamera();
    camera.setZoom(2.0f);
}

} // namespace

namespace game::runtime {

bool GameRuntimeAssembler::assembleServices(ServiceBuildParams params) {
    if (!ensureBlueprintManager(params.services)) {
        return false;
    }

    if (!ensureItemCatalog(params.services)) {
        return false;
    }

    params.services.collision_resolver = std::make_unique<engine::spatial::CollisionResolver>(
        params.registry,
        params.context.getSpatialIndexManager());

    if (!ensureGameTime(params.registry, params.game_time)) {
        return false;
    }

    if (!initWorldState(params.registry, params.services)) {
        return false;
    }

    if (!initFactory(params.context, params.registry, params.services)) {
        return false;
    }

    if (!initMapManager(params.scene, params.context, params.registry, params.services)) {
        return false;
    }

    if (!initSaveService(params.context, params.registry, params.services)) {
        return false;
    }

    if (!loadInitialMap(params.services)) {
        return false;
    }

    configureCamera(params.context);
    return true;
}

bool GameRuntimeAssembler::assembleSystems(SystemBuildParams params) {
    auto& services = params.services;
    if (!services.collision_resolver || !services.entity_factory || !services.blueprint_manager ||
        !services.item_catalog || !services.world_state || !services.map_manager) {
        spdlog::error("Runtime services 未完成装配，无法创建 systems");
        return false;
    }

    auto& systems = params.systems;

    auto& dispatcher = params.context.getDispatcher();
    auto& input_manager = params.context.getInputManager();
    auto& camera = params.context.getCamera();
    auto& spatial_index_manager = params.context.getSpatialIndexManager();
    auto& resource_manager = params.context.getResourceManager();
    auto& auto_tile_library = resource_manager.getAutoTileLibrary();

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

    systems.render_system = std::make_unique<engine::system::RenderSystem>();
    systems.light_system = std::make_unique<engine::system::LightSystem>();
    systems.ysort_system = std::make_unique<engine::system::YSortSystem>();
#ifdef TF_ENABLE_DEBUG_UI
    systems.debug_render_system = std::make_unique<engine::system::DebugRenderSystem>(spatial_index_manager, spatial_panel);
#endif
    systems.movement_system = std::make_unique<engine::system::MovementSystem>(services.collision_resolver.get());
    systems.spatial_index_system = std::make_unique<engine::system::SpatialIndexSystem>(spatial_index_manager);
    systems.animation_system = std::make_unique<engine::system::AnimationSystem>(params.registry, dispatcher);

    systems.state_system = std::make_unique<game::system::StateSystem>(params.registry, dispatcher);
    systems.action_sound_system = std::make_unique<game::system::ActionSoundSystem>(params.registry, dispatcher);
    systems.player_control_system = std::make_unique<game::system::PlayerControlSystem>(
        params.registry,
        dispatcher,
        input_manager,
        camera,
        spatial_index_manager,
        services.item_catalog.get());
    systems.camera_follow_system = std::make_unique<game::system::CameraFollowSystem>(params.registry, camera, input_manager);
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
    systems.time_of_day_light_system = std::make_unique<game::system::TimeOfDayLightSystem>(params.registry, dispatcher);
    systems.day_night_system = std::make_unique<game::system::DayNightSystem>(params.registry);
    systems.light_toggle_system = std::make_unique<game::system::LightToggleSystem>(
        params.registry,
        dispatcher,
        "assets/data/light_config.json");

    systems.npc_wander_system = std::make_unique<game::system::NPCWanderSystem>(params.registry);
    systems.animal_behavior_system = std::make_unique<game::system::AnimalBehaviorSystem>(params.registry);
    systems.dialogue_system = std::make_unique<game::system::DialogueSystem>(params.registry, dispatcher);
    systems.dialogue_system->loadDialogueFile("assets/data/dialogue_script.json");

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
    systems.rest_system = std::make_unique<game::system::RestSystem>(params.registry, params.context);

    systems.inventory_system = std::make_unique<game::system::InventorySystem>(
        params.registry,
        dispatcher,
        *services.item_catalog,
        *services.inventory_domain_service);
    systems.hotbar_system = std::make_unique<game::system::HotbarSystem>(params.registry, dispatcher);
    systems.item_use_system = std::make_unique<game::system::ItemUseSystem>(
        params.registry,
        dispatcher,
        *services.item_catalog,
        *services.inventory_domain_service);

    if (!systems.day_night_system->loadConfig("assets/data/light_config.json")) {
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
        *services.world_state,
        *services.map_manager,
        services.collision_resolver.get());

    return true;
}

} // namespace game::runtime
