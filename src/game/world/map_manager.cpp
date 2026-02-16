#include "map_manager.h"
#include "map_snapshot_serializer.h"

#include "game/loader/entity_builder.h"
#include "game/component/tags.h"
#include "engine/loader/level_loader.h"
#include "engine/loader/tiled_conventions.h"
#include "engine/loader/tiled_json_cache.h"
#include "engine/loader/tiled_json_helpers.h"
#include "engine/render/camera.h"
#include "engine/core/context.h"
#include "engine/spatial/spatial_index_manager.h"
#include "engine/component/transform_component.h"
#include "engine/component/tags.h"
#include "engine/utils/math.h"
#include "game/factory/blueprint_manager.h"
#include "game/factory/entity_factory.h"
#include "game/defs/crop_defs.h"
#include "game/defs/spatial_layers.h"
#include "game/component/crop_component.h"
#include "game/component/farmland_component.h"
#include "game/component/resource_node_component.h"
#include "game/component/chest_component.h"
#include "game/component/tags.h"
#include "engine/component/auto_tile_component.h"
#include "engine/component/render_component.h"
#include "engine/component/sprite_component.h"
#include "engine/component/animation_component.h"
#include "engine/component/collider_component.h"
#include "engine/component/tilelayer_component.h"
#include "game/data/game_time.h"
#include "game/defs/constants.h"
#include "engine/utils/events.h"
#include "engine/utils/scoped_timer.h"
#include <spdlog/spdlog.h>
#include <iterator>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <entt/signal/dispatcher.hpp>
#include <entt/entity/snapshot.hpp>

using namespace entt::literals;

namespace game::world {

namespace {
    constexpr entt::id_type RULE_SOIL_TILLED = game::defs::auto_tile_rule::SOIL_TILLED;
    constexpr entt::id_type RULE_SOIL_WET    = game::defs::auto_tile_rule::SOIL_WET;
    constexpr entt::id_type CHEST_OPEN_ANIM = "open"_hs;

    void warnIfMapScopedEntitiesMissingMapId(entt::registry& registry, entt::id_type current_map_id) {
        auto countView = [](const auto& view) -> std::size_t {
            return static_cast<std::size_t>(std::ranges::distance(view));
        };

        const std::size_t tile_layers = countView(registry.view<engine::component::TileLayerComponent>(
            entt::exclude<game::component::MapId, engine::component::NeedRemoveTag>));
        const std::size_t auto_tiles = countView(registry.view<engine::component::AutoTileComponent>(
            entt::exclude<game::component::MapId, engine::component::NeedRemoveTag>));
        const std::size_t triggers = countView(registry.view<game::component::MapTrigger>(
            entt::exclude<game::component::MapId, engine::component::NeedRemoveTag>));
        const std::size_t rest_areas = countView(registry.view<game::component::RestArea>(
            entt::exclude<game::component::MapId, engine::component::NeedRemoveTag>));
        const std::size_t crops = countView(registry.view<game::component::CropComponent>(
            entt::exclude<game::component::MapId, engine::component::NeedRemoveTag>));
        const std::size_t resources = countView(registry.view<game::component::ResourceNodeComponent>(
            entt::exclude<game::component::MapId, engine::component::NeedRemoveTag>));

        const std::size_t total = tile_layers + auto_tiles + triggers + rest_areas + crops + resources;
        if (total == 0) {
            return;
        }

        spdlog::warn(
            "MapManager: 检测到 {} 个疑似地图实体缺少 MapId (current_map_id={}, tile_layers={}, auto_tiles={}, triggers={}, rest_areas={}, crops={}, resources={})",
            total,
            current_map_id,
            tile_layers,
            auto_tiles,
            triggers,
            rest_areas,
            crops,
            resources);
    }

    void clearBaseResourceNodes(entt::registry& registry,
                               engine::spatial::SpatialIndexManager& spatial_index,
                               entt::id_type map_id) {
        auto res_view = registry.view<
            game::component::ResourceNodeComponent,
            game::component::MapId,
            engine::component::TransformComponent
        >();

        std::vector<entt::entity> to_destroy;
        for (auto entity : res_view) {
            const auto& map = res_view.get<game::component::MapId>(entity);
            if (map.id_ != map_id) {
                continue;
            }

            const auto& transform = res_view.get<engine::component::TransformComponent>(entity);
            spatial_index.removeColliderEntity(entity);
            spatial_index.removeTileEntityAtWorldPos(transform.position_, entity);
            spatial_index.clearTileType(spatial_index.getTileCoordAtWorldPos(transform.position_), engine::component::TileType::SOLID);
            to_destroy.push_back(entity);
        }

        for (auto entity : to_destroy) {
            registry.destroy(entity);
        }
    }

    void rebuildSpatialIndexForDynamicEntities(entt::registry& registry,
                                              engine::spatial::SpatialIndexManager& spatial_index,
                                              entt::id_type map_id,
                                              game::world::DynamicSnapshotFlags flags) {
        {
            auto soil_view = registry.view<
                engine::component::AutoTileComponent,
                game::component::MapId,
                engine::component::TransformComponent
            >();
            for (auto entity : soil_view) {
                const auto& map = soil_view.get<game::component::MapId>(entity);
                if (map.id_ != map_id) {
                    continue;
                }
                const auto& auto_tile = soil_view.get<engine::component::AutoTileComponent>(entity);
                const auto& transform = soil_view.get<engine::component::TransformComponent>(entity);
                if (auto_tile.rule_id_ == RULE_SOIL_TILLED) {
                    spatial_index.addTileEntityAtWorldPos(transform.position_, entity, game::defs::spatial_layer::SOIL);
                } else if (auto_tile.rule_id_ == RULE_SOIL_WET) {
                    spatial_index.addTileEntityAtWorldPos(transform.position_, entity, game::defs::spatial_layer::WET);
                }
            }
        }

        {
            auto crop_view = registry.view<
                game::component::CropComponent,
                game::component::MapId,
                engine::component::TransformComponent
            >();
            for (auto entity : crop_view) {
                const auto& map = crop_view.get<game::component::MapId>(entity);
                if (map.id_ != map_id) {
                    continue;
                }
                const auto& transform = crop_view.get<engine::component::TransformComponent>(entity);
                spatial_index.addTileEntityAtWorldPos(transform.position_, entity, game::defs::spatial_layer::CROP);
            }
        }

        if (!game::world::hasFlag(flags, game::world::DynamicSnapshotFlags::IncludeResources)) {
            return;
        }

        auto res_view = registry.view<
            game::component::ResourceNodeComponent,
            game::component::MapId,
            engine::component::TransformComponent
        >();
        for (auto entity : res_view) {
            const auto& map = res_view.get<game::component::MapId>(entity);
            if (map.id_ != map_id) {
                continue;
            }
            const auto& transform = res_view.get<engine::component::TransformComponent>(entity);
            const auto& node = res_view.get<game::component::ResourceNodeComponent>(entity);

            const entt::id_type layer = node.type_ == game::component::ResourceNodeType::Rock
                                            ? game::defs::spatial_layer::ROCK
                                            : game::defs::spatial_layer::MAIN;

            // 石头位于静态层，树木位于动态层(main与玩家同级层)，需要分别注册到静态网格和动态网格
            if (node.type_ == game::component::ResourceNodeType::Rock) {
                spatial_index.addTileEntityAtWorldPos(transform.position_, entity, layer);
                spatial_index.setTileType(spatial_index.getTileCoordAtWorldPos(transform.position_), engine::component::TileType::SOLID);
            } else if (registry.any_of<engine::component::SpatialIndexTag>(entity) &&
                (registry.any_of<engine::component::AABBCollider>(entity) || registry.any_of<engine::component::CircleCollider>(entity))) {
                spatial_index.updateColliderEntity(entity);
            }
        }
    }

    [[nodiscard]] std::optional<glm::vec3> loadAmbientOverride(std::string_view map_path) {
        const auto json_ptr = engine::loader::tiled::getOrLoadLevelJson(map_path);
        if (!json_ptr) {
            return std::nullopt;
        }
        const auto& json = *json_ptr;

        const auto* properties = engine::loader::tiled::findMember(json, "properties");
        if (!properties || !properties->is_array()) {
            return std::nullopt;
        }

        for (const auto& prop : *properties) {
            const auto* name = engine::loader::tiled::findStringMember(prop, "name");
            if (!name || *name != engine::loader::tiled::MAP_PROPERTY_AMBIENT) {
                continue;
            }

            const auto* value = engine::loader::tiled::findMember(prop, "value");
            if (!value || !value->is_object()) {
                continue;
            }

            return glm::vec3{
                engine::loader::tiled::jsonFloatOr(*value, "red", 1.0f),
                engine::loader::tiled::jsonFloatOr(*value, "green", 1.0f),
                engine::loader::tiled::jsonFloatOr(*value, "blue", 1.0f),
            };
        }
        return std::nullopt;
    }

    void applyChestOpenedVisual(entt::registry& registry, entt::entity entity) {
        auto* sprite = registry.try_get<engine::component::SpriteComponent>(entity);
        auto* anim = registry.try_get<engine::component::AnimationComponent>(entity);
        if (!sprite || !anim) return;

        auto it = anim->animations_.find(CHEST_OPEN_ANIM);
        if (it == anim->animations_.end() || it->second.frames_.empty()) return;
        sprite->sprite_.src_rect_ = it->second.frames_.back().src_rect_;
        anim->current_animation_id_ = entt::null;
    }
}

MapManager::MapManager(engine::scene::Scene& scene,
                       engine::core::Context& context,
                       entt::registry& registry,
                       game::world::WorldState& world_state,
                       game::factory::EntityFactory& entity_factory,
                       game::factory::BlueprintManager& blueprint_manager)
    : scene_(scene),
      context_(context),
      registry_(registry),
      world_state_(world_state),
      entity_factory_(entity_factory),
      blueprint_manager_(blueprint_manager) {
        auto& dispatcher = context_.getDispatcher();
    dispatcher.sink<engine::utils::DayChangedEvent>().connect<&MapManager::onDayChanged>(this);
}

MapManager::~MapManager(){
    auto& dispatcher = context_.getDispatcher();
    dispatcher.disconnect(this);
}

void MapManager::setLoadingSettings(game::world::MapLoadingSettings settings) {
    loading_settings_ = std::move(settings);
    preloaded_maps_.clear();
}

bool MapManager::isMapPreloaded(entt::id_type map_id) const {
    return preloaded_maps_.contains(map_id);
}

bool MapManager::preloadMap(entt::id_type map_id) {
    if (map_id == entt::null) {
        return false;
    }
    if (preloaded_maps_.contains(map_id)) {
        return true;
    }

    const auto* map_state = world_state_.getMapState(map_id);
    if (!map_state) {
        spdlog::warn("MapManager::preloadMap: 未知地图ID {}", map_id);
        return false;
    }

    auto loader = std::make_unique<engine::loader::LevelLoader>(scene_);

    loader->setUseSpatialIndex(false);
    loader->setUseAutoTile(true);
    loader->setTimingEnabled(loading_settings_.log_timings);
    if (!loader->preloadLevelData(map_state->info.file_path)) {
        spdlog::warn("MapManager::preloadMap: 预加载失败: {}", map_state->info.file_path);
        return false;
    }

    preloaded_maps_.insert(map_id);
    return true;
}

void MapManager::preloadAllMaps() {
    engine::utils::ScopedTimer timer(
        "MapManager::preloadAllMaps",
        loading_settings_.log_timings,
        spdlog::level::info
    );

    for (const auto& [map_id, _] : world_state_.maps()) {
        preloadMap(map_id);
    }
}

void MapManager::preloadRelatedMaps(entt::id_type map_id) {
    // 邻接地图
    const auto neighbors = world_state_.neighborsOf(map_id);
    for (entt::id_type neighbor : {neighbors.north, neighbors.south, neighbors.east, neighbors.west}) {
        if (neighbor == entt::null) {
            continue;
        }
        preloadMap(neighbor);
    }

    // 触发器目标地图（在当前地图加载后，EntityBuilder 会把触发器同步到 WorldState::MapState::triggers）
    for (const auto& trigger : world_state_.outgoingTriggers(map_id)) {
        entt::id_type target = trigger.target_map;
        if (!world_state_.getMapState(target) && !trigger.target_map_name.empty()) {
            target = world_state_.ensureExternalMap(trigger.target_map_name);
        }
        preloadMap(target);
    }
}

bool MapManager::loadMap(std::string_view map_name) {
    auto id_it = world_state_.getMapState(map_name);
    entt::id_type map_id = entt::null;
    if (!id_it) {
        map_id = world_state_.ensureExternalMap(map_name);
    } else {
        map_id = id_it->info.id;
    }
    return loadMap(map_id);
}

bool MapManager::loadMap(entt::id_type map_id) {
    engine::utils::ScopedTimer total_timer(
        "MapManager::loadMap",
        loading_settings_.log_timings,
        spdlog::level::info
    );

    const auto* map_state = world_state_.getMapState(map_id);
    if (!map_state) {
        spdlog::error("MapManager: 无法加载地图，未知ID {}", map_id);
        return false;
    }

    if (auto* mutable_state = world_state_.getMapStateMutable(map_id)) {
        mutable_state->triggers.clear();
        mutable_state->info.ambient_override.reset();
        if (!mutable_state->info.in_world) {
            mutable_state->info.ambient_override = loadAmbientOverride(mutable_state->info.file_path);
        }
    }

    {
        engine::utils::ScopedTimer stage_timer(
            "MapManager::unloadCurrentMap",
            loading_settings_.log_timings,
            spdlog::level::info
        );
        unloadCurrentMap();
    }

    level_loader_ = std::make_unique<engine::loader::LevelLoader>(scene_);
    level_loader_->setTimingEnabled(loading_settings_.log_timings);

    auto builder = std::make_unique<game::loader::EntityBuilder>(*level_loader_, context_, registry_, entity_factory_);
    builder->setMapId(map_id);
    const bool has_player = !registry_.view<game::component::PlayerTag>().empty();
    builder->setReusePlayerIfExists(has_player);
    level_loader_->setEntityBuilder(std::move(builder));

    {
        engine::utils::ScopedTimer stage_timer(
            "LevelLoader::loadLevel",
            loading_settings_.log_timings,
            spdlog::level::info
        );
        if (!level_loader_->loadLevel(map_state->info.file_path)) {
            spdlog::error("MapManager: 加载地图失败: {}", map_state->info.file_path);
            return false;
        }
    }

    warnIfMapScopedEntitiesMissingMapId(registry_, map_id);

    // 当前地图视为已预热（至少 JSON/tileset/纹理会进入缓存）
    preloaded_maps_.insert(map_id);

    auto map_size = level_loader_->getMapSize();
    auto tile_size = level_loader_->getTileSize();
    current_map_pixel_size_ = glm::vec2(map_size * tile_size);
    if (auto* mutable_state = world_state_.getMapStateMutable(map_id)) {
        mutable_state->info.size_px = glm::ivec2(current_map_pixel_size_);
    }
    configureCamera(current_map_pixel_size_);
    restoreSnapshot(map_id);
    applyPendingResourceNodes(map_id);
    applyOpenedChests(map_id);

    current_map_id_ = map_id;
    world_state_.setCurrentMap(map_id);

    // 让持久化玩家的 MapId 与当前地图同步
    auto player_view = registry_.view<game::component::PlayerTag, engine::component::TransformComponent>();
    for (auto player : player_view) {
        registry_.emplace_or_replace<game::component::MapId>(player, map_id);
    }

    spdlog::info("MapManager: 已加载地图 '{}' (ID {})", map_state->info.name, map_id);

    if (loading_settings_.preload_mode == game::world::MapPreloadMode::Neighbors) {
        preloadRelatedMaps(map_id);
    }
    return true;
}

void MapManager::unloadCurrentMap() {
    if (current_map_id_ == entt::null) return;

    snapshotMap(current_map_id_);
    destroyEntitiesByMap(current_map_id_);
    current_map_id_ = entt::null;
    current_map_pixel_size_ = glm::vec2(0.0f);
}

void MapManager::snapshotCurrentMap() {
    if (current_map_id_ == entt::null) {
        return;
    }
    snapshotMap(current_map_id_);
}

void MapManager::snapCameraTo(const glm::vec2& world_pos) {
    context_.getCamera().setPosition(world_pos);
}

void MapManager::configureCamera(glm::vec2 map_pixel_size) {
    auto& camera = context_.getCamera();
    camera.setLimitBounds(engine::utils::Rect{glm::vec2(0.0f), map_pixel_size});
    camera.setMaxZoom(3.0f);
    camera.setMinZoom(1.6f);
    if (map_pixel_size != glm::vec2(0.0f)) {
        camera.setPosition(map_pixel_size / 2.0f);
    }
}

void MapManager::destroyEntitiesByMap(entt::id_type map_id) {
    auto view = registry_.view<game::component::MapId>();
    std::vector<entt::entity> to_destroy;

    for (auto entity : view) {
        const auto& comp = view.get<game::component::MapId>(entity);
        if (comp.id_ != map_id) continue;
        // 玩家保持持久化，仅更新位置
        if (registry_.any_of<game::component::PlayerTag>(entity)) {
            continue;
        }
        to_destroy.push_back(entity);
    }

    for (auto entity : to_destroy) {
        registry_.destroy(entity);
    }
}

void MapManager::onDayChanged(const engine::utils::DayChangedEvent& event) {
    const auto day = event.day_;
    for (auto& [map_id, state] : world_state_.mapsMutable()) {
        if (map_id == current_map_id_) continue;
        advanceOffline(state, day);
    }
}

void MapManager::snapshotMap(entt::id_type map_id) {
    auto* map_state = world_state_.getMapStateMutable(map_id);
    if (!map_state) {
        return;
    }

    game::world::writeDynamicSnapshot(registry_, map_id, map_state->persistent.snapshot, game::world::DynamicSnapshotFlags::IncludeResources);

    if (auto* game_time = registry_.ctx().find<game::data::GameTime>()) {
        map_state->persistent.last_updated_day = game_time->day_;
    }
}

void MapManager::restoreSnapshot(entt::id_type map_id) {
    auto* map_state = world_state_.getMapStateMutable(map_id);
    if (!map_state || !map_state->persistent.snapshot.valid) {
        return;
    }

    game::world::DynamicSnapshotFlags flags{};
    if (!game::world::peekDynamicSnapshotFlags(map_state->persistent.snapshot, flags)) {
        return;
    }

    std::uint32_t current_day = 0;
    if (auto* game_time = registry_.ctx().find<game::data::GameTime>()) {
        current_day = game_time->day_;
    }
    advanceOffline(*map_state, current_day);

    auto& spatial_index = context_.getSpatialIndexManager();

    // 资源节点：先清理地图文件生成的基础节点（并清除 SOLID），再从快照恢复
    if (game::world::hasFlag(flags, game::world::DynamicSnapshotFlags::IncludeResources)) {
        clearBaseResourceNodes(registry_, spatial_index, map_id);
    }

    if (!game::world::readDynamicSnapshot(registry_, map_state->persistent.snapshot, &flags)) {
        return;
    }

    // 将快照恢复的动态实体重新注册到 SpatialIndex（StaticTileGrid + DynamicGrid）
    rebuildSpatialIndexForDynamicEntities(registry_, spatial_index, map_id, flags);
}

void MapManager::applyPendingResourceNodes(entt::id_type map_id) {
    auto* map_state = world_state_.getMapStateMutable(map_id);
    if (!map_state || !map_state->persistent.pending_resource_nodes) {
        return;
    }

    if (map_state->persistent.snapshot.valid) {
        game::world::DynamicSnapshotFlags flags{};
        if (game::world::peekDynamicSnapshotFlags(map_state->persistent.snapshot, flags) &&
            game::world::hasFlag(flags, game::world::DynamicSnapshotFlags::IncludeResources)) {
            map_state->persistent.pending_resource_nodes.reset();
            return;
        }
    }

    struct ResourceKey {
        int x{0};
        int y{0};
        game::component::ResourceNodeType type{game::component::ResourceNodeType::Unknown};
    };

    struct ResourceKeyHash {
        std::size_t operator()(const ResourceKey& key) const noexcept {
            const std::size_t h1 = std::hash<int>()(key.x * 73856093 ^ key.y * 19349663);
            const std::size_t h2 = std::hash<int>()(static_cast<int>(key.type));
            return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
        }
    };

    struct ResourceKeyEq {
        bool operator()(const ResourceKey& a, const ResourceKey& b) const noexcept {
            return a.x == b.x && a.y == b.y && a.type == b.type;
        }
    };

    std::unordered_map<ResourceKey, int, ResourceKeyHash, ResourceKeyEq> alive_hit_count;
    alive_hit_count.reserve(map_state->persistent.pending_resource_nodes->size());
    for (const auto& node : *map_state->persistent.pending_resource_nodes) {
        alive_hit_count[ResourceKey{node.tile_coord.x, node.tile_coord.y, node.type_}] = node.hit_count_;
    }

    auto& spatial_index = context_.getSpatialIndexManager();
    auto view = registry_.view<
        game::component::ResourceNodeComponent,
        game::component::MapId,
        engine::component::TransformComponent
    >();

    std::vector<entt::entity> to_destroy;
    for (auto entity : view) {
        const auto& map = view.get<game::component::MapId>(entity);
        if (map.id_ != map_id) {
            continue;
        }

        auto& node = view.get<game::component::ResourceNodeComponent>(entity);
        const auto& transform = view.get<engine::component::TransformComponent>(entity);
        const glm::ivec2 tile_coord = spatial_index.getTileCoordAtWorldPos(transform.position_);

        auto it = alive_hit_count.find(ResourceKey{tile_coord.x, tile_coord.y, node.type_});
        if (it == alive_hit_count.end()) {
            spatial_index.removeColliderEntity(entity);
            spatial_index.removeTileEntityAtWorldPos(transform.position_, entity);
            spatial_index.clearTileType(tile_coord, engine::component::TileType::SOLID);
            to_destroy.push_back(entity);
            continue;
        }

        node.hit_count_ = it->second;
    }

    for (auto entity : to_destroy) {
        registry_.destroy(entity);
    }

    snapshotMap(map_id);
    map_state->persistent.pending_resource_nodes.reset();
}

void MapManager::applyOpenedChests(entt::id_type map_id) {
    const auto* map_state = world_state_.getMapState(map_id);
    if (!map_state || map_state->persistent.opened_chests.empty()) {
        return;
    }

    auto view = registry_.view<game::component::ChestComponent, game::component::MapId>();
    for (auto entity : view) {
        const auto& map = view.get<game::component::MapId>(entity);
        if (map.id_ != map_id) {
            continue;
        }
        auto& chest = view.get<game::component::ChestComponent>(entity);
        if (!map_state->persistent.opened_chests.contains(chest.chest_id_)) {
            continue;
        }
        chest.opened_ = true;
        applyChestOpenedVisual(registry_, entity);
    }
}

void MapManager::advanceOffline(game::world::MapState& state, std::uint32_t current_day) {
    if (!state.persistent.snapshot.valid) {
        return;
    }

    if (state.persistent.last_updated_day == 0) {
        state.persistent.last_updated_day = current_day;
        return;
    }

    if (current_day <= state.persistent.last_updated_day) {
        return;
    }

    entt::registry temp;
    game::world::DynamicSnapshotFlags flags{};
    if (!game::world::readDynamicSnapshot(temp, state.persistent.snapshot, &flags)) {
        return;
    }

    game::world::simulateOfflineDays(temp, blueprint_manager_, current_day - state.persistent.last_updated_day);
    game::world::writeDynamicSnapshot(temp, state.info.id, state.persistent.snapshot, flags);
    state.persistent.last_updated_day = current_day;
}

} // namespace game::world
