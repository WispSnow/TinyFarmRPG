#include "game_runtime_assembler.h"

#include "engine/core/context.h"
#include "engine/debug/panels/spatial_index_debug_panel.h"
#include "engine/input/input_manager.h"
#include "engine/render/camera.h"
#include "engine/render/opengl/gl_renderer.h"
#include "engine/resource/asset_registry.h"
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
#include "engine/utils/json_file_loader.h"
#include "engine/vfx/null_vfx_backend.h"
#include "engine/vfx/vfx_service.h"
#include "engine/vfx/effekseer_backend_factory.h"
#ifdef TF_ENABLE_DEBUG_UI
#include "engine/debug/debug_ui_manager.h"
#endif
#include "engine/scene/scene.h"
#include "game/factory/blueprint_manager.h"
#include "game/factory/entity_factory.h"
#include "game/component/appearance_component.h"
#include "game/data/game_time.h"
#include "game/data/appearance_catalog.h"
#include "game/data/item_catalog.h"
#include "game/data/quest_catalog.h"
#include "game/data/shop_catalog.h"
#include "game/data/rpg_catalog.h"
#include "engine/vfx/vfx_catalog.h"
#include "game/defs/commands.h"
#include "game/domain/inventory_domain_service.h"
#include "game/domain/quest_turn_in_service.h"
#include "game/domain/shop_transaction_service.h"
#include "game/runtime/gameplay_camera_defaults.h"
#include "game/runtime/rpg_catalog_loader.h"
#include "game/save/save_service.h"
#include "engine/script/script_host.h"
#include "game/script/tinyfarm_script_module.h"
#include "game/system/action_sound_system.h"
#include "game/system/animal_behavior_system.h"
#include "game/system/appearance_system.h"
#include "game/system/animation_event_system.h"
#include "game/system/camera_follow_system.h"
#include "game/system/chest_system.h"
#include "game/system/crop_system.h"
#include "game/system/day_night_system.h"
#include "game/system/dialogue_system.h"
#include "game/system/enemy_encounter_system.h"
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
#include "game/system/party_recruitment_system.h"
#include "game/system/quest_interaction_system.h"
#include "game/system/recruitment_interaction_system.h"
#include "game/system/shop_interaction_system.h"
#include "game/system/render_target_system.h"
#include "game/system/rest_system.h"
#include "game/system/state_system.h"
#include "game/system/time_of_day_light_system.h"
#include "game/system/time_system.h"
#include "engine/vfx/vfx_bridge_system.h"
#include "game/world/map_manager.h"
#include "engine/component/layered_sprite_component.h"
#include "game/world/map_loading_settings.h"
#include "game/world/world_state.h"

#include <entt/core/hashed_string.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
// Use filesystem only to distinguish "bootstrap script missing" from execution errors in logs.
#include <filesystem>
#include <optional>
#include <unordered_set>
#include <vector>

using namespace entt::literals;

namespace {

[[nodiscard]] entt::id_type hashPath(std::string_view path) {
    if (path.empty()) {
        return entt::null;
    }
    return entt::hashed_string{path.data(), path.size()}.value();
}

void registerTexturePath(engine::resource::AssetRegistry& registry, entt::id_type id, std::string_view path) {
    if (path.empty()) {
        return;
    }
    const entt::id_type resolved_id = (id != entt::null) ? id : hashPath(path);
    if (resolved_id == entt::null) {
        return;
    }
    registry.registerTexture(resolved_id, path);
}

void registerImageTexture(engine::resource::AssetRegistry& registry, const engine::render::Image& image) {
    registerTexturePath(registry, image.getTextureId(), image.getTexturePath());
}

void collectBlueprintAssets(const game::factory::BlueprintManager& manager, engine::resource::AssetRegistry& registry) {
    const auto collect_animations = [&registry](const auto& animations) {
        for (const auto& [_, animation] : animations) {
            registerTexturePath(registry, animation.texture_id_, animation.texture_path_);
        }
    };

    for (const auto& [_, blueprint] : manager.actorBlueprints()) {
        registerTexturePath(registry, blueprint.sprite_.id_, blueprint.sprite_.path_);
        collect_animations(blueprint.animations_);
    }

    for (const auto& [_, blueprint] : manager.animalBlueprints()) {
        registerTexturePath(registry, blueprint.sprite_.id_, blueprint.sprite_.path_);
        collect_animations(blueprint.animations_);
    }

    for (const auto& [_, blueprint] : manager.cropBlueprints()) {
        for (const auto& stage : blueprint.stages_) {
            registerTexturePath(registry, stage.sprite_.id_, stage.sprite_.path_);
        }
    }
}

void collectItemCatalogAssets(const game::data::ItemCatalog& catalog, engine::resource::AssetRegistry& registry) {
    for (const auto& [_, icon] : catalog.icons()) {
        registerImageTexture(registry, icon);
    }
}

void collectAppearanceAssets(const game::data::AppearanceCatalog& catalog, engine::resource::AssetRegistry& registry) {
    constexpr std::size_t kRuntimeVariantPreloadLimitPerSlot = 3;

    const auto* profile = catalog.defaultProfile();
    if (!profile) {
        return;
    }

    const auto preload_paths = catalog.collectPreloadTexturePaths(*profile, kRuntimeVariantPreloadLimitPerSlot);
    for (const auto& path : preload_paths) {
        registerTexturePath(registry, hashPath(path), path);
    }
}

[[nodiscard]] std::string resolveRelativePath(std::string_view relative_path, std::string_view anchor_file) {
    const auto anchor_dir = std::filesystem::path(anchor_file).parent_path();
    return (anchor_dir / std::filesystem::path(relative_path)).lexically_normal().string();
}

void registerTilesetTexturesFromTsj(std::string_view tsj_path, engine::resource::AssetRegistry& registry) {
    nlohmann::json tsj_json;
    if (!engine::utils::loadJsonObjectFile(tsj_path, tsj_json, "AssetRegistry")) {
        return;
    }

    if (const auto image_it = tsj_json.find("image");
        image_it != tsj_json.end() && image_it->is_string() && !image_it->get_ref<const std::string&>().empty()) {
        const auto texture_path = resolveRelativePath(image_it->get_ref<const std::string&>(), tsj_path);
        registerTexturePath(registry, hashPath(texture_path), texture_path);
    }

    if (const auto tiles_it = tsj_json.find("tiles"); tiles_it != tsj_json.end() && tiles_it->is_array()) {
        for (const auto& tile_json : *tiles_it) {
            if (!tile_json.is_object()) {
                continue;
            }
            if (const auto tile_image_it = tile_json.find("image");
                tile_image_it != tile_json.end() && tile_image_it->is_string() &&
                !tile_image_it->get_ref<const std::string&>().empty()) {
                const auto texture_path = resolveRelativePath(tile_image_it->get_ref<const std::string&>(), tsj_path);
                registerTexturePath(registry, hashPath(texture_path), texture_path);
            }
        }
    }
}

void registerMapTilesetTextures(std::string_view tmj_path,
                                engine::resource::AssetRegistry& registry,
                                std::unordered_set<std::string>& scanned_tilesets) {
    nlohmann::json map_json;
    if (!engine::utils::loadJsonObjectFile(tmj_path, map_json, "AssetRegistry")) {
        return;
    }

    if (const auto layers_it = map_json.find("layers"); layers_it != map_json.end() && layers_it->is_array()) {
        for (const auto& layer_json : *layers_it) {
            if (!layer_json.is_object()) {
                continue;
            }
            const std::string layer_type = layer_json.value("type", "");
            if (layer_type != "imagelayer") {
                continue;
            }
            const std::string image_path = layer_json.value("image", "");
            if (image_path.empty()) {
                continue;
            }
            const auto resolved_path = resolveRelativePath(image_path, tmj_path);
            registerTexturePath(registry, hashPath(resolved_path), resolved_path);
        }
    }

    const auto tilesets_it = map_json.find("tilesets");
    if (tilesets_it == map_json.end() || !tilesets_it->is_array()) {
        return;
    }

    for (const auto& tileset_ref : *tilesets_it) {
        if (!tileset_ref.is_object()) {
            continue;
        }

        if (const auto source_it = tileset_ref.find("source");
            source_it != tileset_ref.end() && source_it->is_string()) {
            const auto tsj_path = resolveRelativePath(source_it->get_ref<const std::string&>(), tmj_path);
            if (!scanned_tilesets.insert(tsj_path).second) {
                continue;
            }
            registerTilesetTexturesFromTsj(tsj_path, registry);
            continue;
        }

        if (const auto image_it = tileset_ref.find("image"); image_it != tileset_ref.end() && image_it->is_string()) {
            const auto texture_path = resolveRelativePath(image_it->get_ref<const std::string&>(), tmj_path);
            registerTexturePath(registry, hashPath(texture_path), texture_path);
        }
    }
}

void collectWorldMapAssets(const game::world::WorldState& world_state, engine::resource::AssetRegistry& registry) {
    std::unordered_set<std::string> scanned_tilesets{};
    for (const auto& [_, map_state] : world_state.maps()) {
        if (map_state.info.file_path.empty()) {
            continue;
        }
        registerMapTilesetTextures(map_state.info.file_path, registry, scanned_tilesets);
    }
}

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

[[nodiscard]] bool ensureAppearanceCatalog(game::runtime::GameRuntimeServices& services) {
    if (!services.appearance_catalog) {
        services.appearance_catalog = std::make_shared<game::data::AppearanceCatalog>();
        if (!services.appearance_catalog->loadFromFile("assets/data/appearance_catalog.json")) {
            spdlog::error("加载外观目录配置失败");
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool ensureVfxCatalog(game::runtime::GameRuntimeServices& services) {
    if (!services.vfx_catalog) {
        services.vfx_catalog = std::make_shared<engine::vfx::VfxCatalog>();
        if (!services.vfx_catalog->loadFromFile("assets/data/vfx_catalog.json")) {
            spdlog::warn("加载 VFX 目录配置失败，将继续运行但禁用 catalog 驱动播放。");
        }
    }
    return true;
}

[[nodiscard]] bool ensureRpgCatalog(game::runtime::GameRuntimeServices& services) {
    if (services.rpg_catalog) {
        return true;
    }

    services.rpg_catalog = std::make_shared<game::data::RpgCatalog>();
    game::runtime::RpgCatalogLoadOptions options{};
    options.item_catalog = services.item_catalog.get();
    std::string load_error{};
    if (!game::runtime::loadRpgCatalogFromManifest(*services.rpg_catalog, options, load_error)) {
        spdlog::error("{}", load_error);
        return false;
    }

    return true;
}

[[nodiscard]] bool ensureQuestCatalog(game::runtime::GameRuntimeServices& services) {
    if (services.quest_catalog) {
        return true;
    }

    if (!services.rpg_catalog || !services.item_catalog) {
        spdlog::error("QuestCatalog 依赖 RpgCatalog 和 ItemCatalog。");
        return false;
    }

    services.quest_catalog = std::make_shared<game::data::QuestCatalog>();
    constexpr std::string_view kQuestCatalogPath = "assets/data/quests.json";
    if (!services.quest_catalog->loadFromFile(kQuestCatalogPath)) {
        spdlog::error("加载 QuestCatalog 失败: {}", kQuestCatalogPath);
        return false;
    }

    std::string reference_error{};
    if (!services.quest_catalog->validateReferences(
            services.rpg_catalog.get(),
            services.item_catalog.get(),
            reference_error)) {
        spdlog::error("QuestCatalog 引用校验失败: {}", reference_error);
        return false;
    }

    return true;
}

[[nodiscard]] bool ensureShopCatalog(game::runtime::GameRuntimeServices& services) {
    if (services.shop_catalog) {
        return true;
    }

    if (!services.item_catalog) {
        spdlog::error("ShopCatalog 依赖 ItemCatalog。");
        return false;
    }

    services.shop_catalog = std::make_shared<game::data::ShopCatalog>();
    constexpr std::string_view kShopCatalogPath = "assets/data/shops.json";
    if (!services.shop_catalog->loadFromFile(kShopCatalogPath)) {
        spdlog::error("加载 ShopCatalog 失败: {}", kShopCatalogPath);
        return false;
    }

    std::string reference_error{};
    if (!services.shop_catalog->validateReferences(services.item_catalog.get(), reference_error)) {
        spdlog::error("ShopCatalog 引用校验失败: {}", reference_error);
        return false;
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

    collectWorldMapAssets(*services.world_state, context.getResourceManager().getAssetRegistry());

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
    game::runtime::applyGameplayCameraDefaultZoom(camera);
}

void initVfxService(engine::core::Context& context, game::runtime::GameRuntimeServices& services) {
    if (!services.vfx_service) {
        std::unique_ptr<engine::vfx::VfxBackend> backend{};
        backend = engine::vfx::createEffekseerBackend();
        if (!backend) {
            spdlog::warn("EffekseerBackend 初始化失败，将回退到 NullVfxBackend。");
        }
        if (!backend) {
            backend = std::make_unique<engine::vfx::NullVfxBackend>();
        }
        services.vfx_service = std::make_unique<engine::vfx::VfxService>(std::move(backend));
    }

    context.getGLRenderer().setVfxBackend(services.vfx_service ? services.vfx_service->backend() : nullptr);
}

void tryInitScriptHost(entt::registry& registry,
                       engine::core::Context& context,
                       game::runtime::GameRuntimeServices& services) {
    services.script_host = std::make_unique<engine::script::ScriptHost>(registry);
    const std::vector<engine::script::ScriptModuleInstaller> installers{
        game::script::installTinyFarmScriptModule,
    };
    if (!services.script_host->init(context.getDispatcher(), installers)) {
        spdlog::warn("ScriptHost 初始化失败，脚本功能将禁用。");
        services.script_host.reset();
        return;
    }

    const std::filesystem::path bootstrap_script = "scripts/bootstrap.lua";
    if (!std::filesystem::exists(bootstrap_script)) {
        spdlog::info("ScriptHost: 未找到启动脚本 {}", bootstrap_script.string());
        return;
    }

    if (!services.script_host->loadFile(bootstrap_script.string())) {
        spdlog::warn("ScriptHost: 启动脚本执行失败，将继续游戏主流程。");
    } else {
        spdlog::info("ScriptHost: 启动脚本执行成功 {}", bootstrap_script.string());
    }
}

} // namespace

namespace game::runtime {

bool GameRuntimeAssembler::assembleServices(ServiceBuildParams params) {
    auto& resource_manager = params.context.getResourceManager();
    auto& asset_registry = resource_manager.getAssetRegistry();
    // AppearanceSystem 在调试换装时会按需请求纹理加载（可能不在首批预加载集合中），
    // 通过 registry.ctx() 注入 ResourceManager*，避免 engine 层反向依赖 game 配置对象。
    if (auto* resource_ptr = params.registry.ctx().find<engine::resource::ResourceManager*>()) {
        *resource_ptr = &resource_manager;
    } else {
        params.registry.ctx().emplace<engine::resource::ResourceManager*>(&resource_manager);
    }

    if (!ensureBlueprintManager(params.services)) {
        return false;
    }
    if (params.services.blueprint_manager) {
        collectBlueprintAssets(*params.services.blueprint_manager, asset_registry);
    }

    if (!ensureItemCatalog(params.services)) {
        return false;
    }
    if (params.services.item_catalog) {
        collectItemCatalogAssets(*params.services.item_catalog, asset_registry);
    }
    if (!ensureAppearanceCatalog(params.services)) {
        return false;
    }
    if (params.services.appearance_catalog) {
        collectAppearanceAssets(*params.services.appearance_catalog, asset_registry);
    }
    if (!ensureVfxCatalog(params.services)) {
        return false;
    }
    if (!ensureRpgCatalog(params.services)) {
        return false;
    }
    if (!ensureQuestCatalog(params.services)) {
        return false;
    }
    if (!ensureShopCatalog(params.services)) {
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

    resource_manager.preloadRegisteredResources();

    if (!initSaveService(params.context, params.registry, params.services)) {
        return false;
    }

    if (!loadInitialMap(params.services)) {
        return false;
    }

    configureCamera(params.context);
    initVfxService(params.context, params.services);
    tryInitScriptHost(params.registry, params.context, params.services);
    return true;
}

bool GameRuntimeAssembler::assembleSystems(SystemBuildParams params) {
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
    systems.debug_render_system = std::make_unique<engine::system::DebugRenderSystem>(spatial_index_manager, spatial_panel);
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
    systems.quest_interaction_system = std::make_unique<game::system::QuestInteractionSystem>(
        params.registry,
        dispatcher,
        *services.quest_catalog,
        *services.quest_turn_in_service);
    systems.recruitment_interaction_system = std::make_unique<game::system::RecruitmentInteractionSystem>(
        params.registry,
        dispatcher,
        *services.rpg_catalog);
    const bool recruitment_dialogue_loaded =
        systems.recruitment_interaction_system->loadDialogueFile("assets/data/dialogue_script.json");
    if (!recruitment_dialogue_loaded) {
        spdlog::warn("GameRuntimeAssembler: 招募对话脚本加载失败，将使用兜底招募文案。");
    }
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
