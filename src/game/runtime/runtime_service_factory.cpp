#include "runtime_service_factory.h"

#include "asset_preload_registrar.h"
#include "content_catalog_loader.h"
#include "engine/core/context.h"
#include "engine/render/camera.h"
#include "engine/render/opengl/gl_renderer.h"
#include "engine/resource/resource_manager.h"
#include "engine/spatial/collision_resolver.h"
#include "engine/vfx/effekseer_backend_factory.h"
#include "engine/vfx/null_vfx_backend.h"
#include "engine/vfx/vfx_service.h"
#include "game/data/game_time.h"
#include "game/data/quest_catalog.h"
#include "game/data/rpg_catalog.h"
#include "game/data/shop_catalog.h"
#include "game/factory/blueprint_manager.h"
#include "game/factory/entity_factory.h"
#include "game/runtime/game_content_manifest.h"
#include "game/runtime/gameplay_camera_defaults.h"
#include "game/runtime/localization_service.h"
#include "game/runtime/script_runtime_factory.h"
#include "game/runtime/user_settings_service.h"
#include "game/save/save_service.h"
#include "game/world/map_loading_settings.h"
#include "game/world/map_manager.h"
#include "game/world/world_state.h"

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <spdlog/spdlog.h>

#include <memory>

namespace {

void logWebDirectMapBootCheckpoint(const char* checkpoint) {
#ifdef TF_WEB_DIRECT_MAP_BOOT
    spdlog::info("Web direct map boot checkpoint: {}", checkpoint);
#else
    (void)checkpoint;
#endif
}

[[nodiscard]] bool ensureGameTime(entt::registry& registry, std::shared_ptr<game::data::GameTime>& game_time) {
    if (!game_time) {
        game_time = game::data::GameTime::loadFromConfig(game::runtime::GameContentManifest::GameTime);
        if (!game_time) {
            spdlog::error("从配置文件加载 GameTime 失败");
            return false;
        }
    }

    game_time->loadEmissiveVisibilityFromLightConfig(game::runtime::GameContentManifest::LightConfig);

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

    if (!services.world_state->loadFromWorldFile(
            game::runtime::GameContentManifest::World,
            entt::hashed_string{game::runtime::GameContentManifest::InitialMapName}.value())) {
        spdlog::error("加载 world 布局失败");
        return false;
    }

    (void)services.world_state->ensureExternalMap(game::runtime::GameContentManifest::HomeInteriorMapName);

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
    auto& auto_tile_library = context.getAutoTileLibrary();

    services.entity_factory = std::make_unique<game::factory::EntityFactory>(
        registry,
        *services.blueprint_manager,
        &spatial_index_manager,
        &auto_tile_library,
        &context.getDispatcher(),
        services.appearance_catalog.get());

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

    game::runtime::AssetPreloadRegistrar::collectWorldMapAssets(
        *services.world_state,
        context.getResourceManager().getAssetRegistry());

    const auto settings =
        game::world::MapLoadingSettings::loadFromFile(game::runtime::GameContentManifest::MapLoadingConfig);
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
        *services.blueprint_manager,
        services.rpg_catalog.get());

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
            if (const auto* map_state =
                    services.world_state->getMapState(game::runtime::GameContentManifest::InitialMapName)) {
                initial_map = map_state->info.id;
            }
        }
    }

    if (initial_map == entt::null) {
        initial_map = entt::hashed_string{game::runtime::GameContentManifest::InitialMapName}.value();
    }

    if (!services.map_manager->loadMap(initial_map)) {
        spdlog::error("加载关卡失败");
        return false;
    }

    return true;
}

void configureCamera(engine::core::Context& context) {
    auto& camera = context.getCamera();
    game::runtime::applyGameplayCameraDefaultZoom(camera);
}

void initVfxService(engine::core::Context& context, game::runtime::GameRuntimeServices& services) {
    if (!services.vfx_service) {
        std::unique_ptr<engine::vfx::VfxBackend> backend{};
        backend = engine::vfx::createEffekseerBackend();
#ifdef TF_ENABLE_EFFEKSEER
        if (!backend) {
            spdlog::warn("EffekseerBackend 初始化失败，将回退到 NullVfxBackend。");
        }
#if defined(__EMSCRIPTEN__)
        spdlog::info("Web release VFX policy: effekseer_enabled=true backend={} status={}.",
                     backend ? "effekseer" : "null_vfx_backend",
                     backend ? "enabled" : "fallback");
#endif
#else
        spdlog::info("Effekseer VFX 后端未启用，将使用 NullVfxBackend。");
#if defined(__EMSCRIPTEN__)
        spdlog::info("Web release VFX policy: effekseer_enabled=false backend=null_vfx_backend status=deferred.");
#endif
#endif
        if (!backend) {
            backend = std::make_unique<engine::vfx::NullVfxBackend>();
        }
        services.vfx_service = std::make_unique<engine::vfx::VfxService>(std::move(backend));
    }

    context.getGLRenderer().setVfxBackend(services.vfx_service ? services.vfx_service->backend() : nullptr);
}

template <typename T>
void injectRegistryContextPointer(entt::registry& registry, T* pointer) {
    if (!pointer) {
        return;
    }
    if (auto** current = registry.ctx().find<T*>()) {
        *current = pointer;
    } else {
        registry.ctx().emplace<T*>(pointer);
    }
}

void initUserSettings(engine::core::Context& context,
                      entt::registry& registry,
                      game::runtime::GameRuntimeServices& services) {
    auto* rml_runtime = context.getRmlUi();
    auto* ctx_game_time = registry.ctx().find<game::data::GameTime>();
    if (rml_runtime && ctx_game_time && services.localization_service) {
        services.user_settings_service = std::make_unique<game::runtime::UserSettingsService>(
            context.getDispatcher(),
            context.getAudioPlayer(),
            *ctx_game_time,
            *services.localization_service,
            context.getTextRenderer(),
            *rml_runtime);
        services.user_settings_service->loadFromFileOrFallback();
        services.user_settings_service->applyAll();
    } else {
        spdlog::warn("GameRuntimeAssembler: 缺少 RmlUiRuntime 或 ctx GameTime，UserSettingsService 跳过初始化。");
    }
}

void initLocalization(entt::registry& registry, game::runtime::GameRuntimeServices& services) {
    if (!services.localization_service) {
        services.localization_service = std::make_unique<game::runtime::LocalizationService>();
    }
    if (!services.localization_service->loadLanguageIndex(game::runtime::GameContentManifest::I18nLanguages)) {
        spdlog::warn("GameRuntimeAssembler: 本地化语言表加载失败，将继续使用内建默认语言。");
    }
    injectRegistryContextPointer(registry, services.localization_service.get());
}

void injectResourceManager(engine::core::Context& context, entt::registry& registry) {
    auto& resource_manager = context.getResourceManager();
    if (auto* resource_ptr = registry.ctx().find<engine::resource::ResourceManager*>()) {
        *resource_ptr = &resource_manager;
    } else {
        registry.ctx().emplace<engine::resource::ResourceManager*>(&resource_manager);
    }
}

void injectCatalogPointers(entt::registry& registry, game::runtime::GameRuntimeServices& services) {
    injectRegistryContextPointer(registry, services.rpg_catalog.get());
    injectRegistryContextPointer(registry, services.quest_catalog.get());
    injectRegistryContextPointer(registry, services.shop_catalog.get());
}

} // namespace

namespace game::runtime {

bool RuntimeServiceFactory::assemble(GameRuntimeAssembler::ServiceBuildParams params) {
    auto& resource_manager = params.context.getResourceManager();
    auto& asset_registry = resource_manager.getAssetRegistry();

    logWebDirectMapBootCheckpoint("assemble services begin");
    injectResourceManager(params.context, params.registry);

    if (!ContentCatalogLoader::ensureBlueprintManager(params.services)) {
        return false;
    }
    logWebDirectMapBootCheckpoint("blueprint catalog ready");
    if (params.services.blueprint_manager) {
        AssetPreloadRegistrar::collectBlueprintAssets(*params.services.blueprint_manager, asset_registry);
    }

    if (!ContentCatalogLoader::ensureItemCatalog(params.services)) {
        return false;
    }
    logWebDirectMapBootCheckpoint("item catalog ready");
    if (params.services.item_catalog) {
        AssetPreloadRegistrar::collectItemCatalogAssets(*params.services.item_catalog, asset_registry);
    }
    if (!ContentCatalogLoader::ensureAppearanceCatalog(params.services)) {
        return false;
    }
    logWebDirectMapBootCheckpoint("appearance catalog ready");
    if (params.services.appearance_catalog) {
        AssetPreloadRegistrar::collectAppearanceAssets(*params.services.appearance_catalog, asset_registry);
    }
    if (!ContentCatalogLoader::ensureVfxCatalog(params.services)) {
        return false;
    }
    logWebDirectMapBootCheckpoint("vfx catalog ready");
    if (!ContentCatalogLoader::ensureRpgCatalog(params.services)) {
        return false;
    }
    logWebDirectMapBootCheckpoint("rpg catalog ready");
    if (!ContentCatalogLoader::ensureQuestCatalog(params.services)) {
        return false;
    }
    logWebDirectMapBootCheckpoint("quest catalog ready");
    if (!ContentCatalogLoader::ensureShopCatalog(params.services)) {
        return false;
    }
    logWebDirectMapBootCheckpoint("shop catalog ready");
    injectCatalogPointers(params.registry, params.services);
    initLocalization(params.registry, params.services);
    ContentCatalogLoader::ensureAudioCueCatalog(params.services, asset_registry);
    logWebDirectMapBootCheckpoint("localization and audio cue catalogs ready");

    params.services.collision_resolver = std::make_unique<engine::spatial::CollisionResolver>(
        params.registry,
        params.context.getSpatialIndexManager());
    logWebDirectMapBootCheckpoint("collision resolver ready");

    if (!ensureGameTime(params.registry, params.game_time)) {
        return false;
    }
    logWebDirectMapBootCheckpoint("game time ready");

    initUserSettings(params.context, params.registry, params.services);
    logWebDirectMapBootCheckpoint("user settings step complete");

    if (!initWorldState(params.registry, params.services)) {
        return false;
    }
    logWebDirectMapBootCheckpoint("world state ready");

    if (!initFactory(params.context, params.registry, params.services)) {
        return false;
    }
    logWebDirectMapBootCheckpoint("entity factory ready");

    if (!initMapManager(params.scene, params.context, params.registry, params.services)) {
        return false;
    }
    logWebDirectMapBootCheckpoint("map manager ready");

    logWebDirectMapBootCheckpoint("resource preload begin");
    resource_manager.preloadRegisteredResources();
    logWebDirectMapBootCheckpoint("resource preload complete");

    if (!initSaveService(params.context, params.registry, params.services)) {
        return false;
    }
    logWebDirectMapBootCheckpoint("save service ready");

    if (params.load_initial_map) {
        if (!loadInitialMap(params.services)) {
            return false;
        }
        logWebDirectMapBootCheckpoint("initial map loaded");
    } else {
        spdlog::info("RuntimeServiceFactory: initial map load skipped.");
    }

    configureCamera(params.context);
    initVfxService(params.context, params.services);
    ScriptRuntimeFactory::tryInitScriptHost(params.registry, params.context, params.services);
    logWebDirectMapBootCheckpoint("assemble services complete");
    return true;
}

} // namespace game::runtime
