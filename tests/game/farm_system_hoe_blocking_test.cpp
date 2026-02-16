#include <gtest/gtest.h>

#include "game/system/farm_system.h"

#include "engine/component/collider_component.h"
#include "engine/component/render_component.h"
#include "engine/component/tags.h"
#include "engine/component/transform_component.h"
#include "engine/resource/auto_tile_library.h"
#include "engine/spatial/spatial_index_manager.h"

#include "game/factory/blueprint_manager.h"
#include "game/factory/entity_factory.h"

#include "game/defs/constants.h"
#include "game/defs/events.h"
#include "game/defs/render_layers.h"
#include "game/defs/spatial_layers.h"

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

using namespace entt::literals;

namespace {

constexpr glm::ivec2 MAP_SIZE(4, 4);
constexpr glm::ivec2 TILE_SIZE(16, 16);
const glm::vec2 WORLD_MIN(0.0f, 0.0f);
const glm::vec2 WORLD_MAX(64.0f, 64.0f);

constexpr entt::id_type LAYER_BASE = "base"_hs;
constexpr entt::id_type RULE_SOIL_TILLED = "soil_tilled"_hs;

glm::vec2 tileWorldPos(glm::ivec2 tile_coord) {
    return glm::vec2(tile_coord.x * TILE_SIZE.x, tile_coord.y * TILE_SIZE.y);
}

glm::vec2 tileSizeF() {
    return glm::vec2(static_cast<float>(TILE_SIZE.x), static_cast<float>(TILE_SIZE.y));
}

void setupTilledRule(engine::resource::AutoTileLibrary& library) {
    engine::resource::AutoTileRule rule(entt::hashed_string{"dummy_texture"}.value(), "soil_tilled");
    library.addRule(entt::hashed_string{"soil_tilled"}, std::move(rule));
    library.setSrcRect(RULE_SOIL_TILLED, 0, engine::utils::Rect(glm::vec2(0.0f, 0.0f), tileSizeF()));
}

entt::entity addBaseTile(entt::registry& registry,
                         engine::spatial::SpatialIndexManager& spatial,
                         glm::ivec2 tile_coord) {
    const glm::vec2 pos = tileWorldPos(tile_coord);
    const entt::entity entity = registry.create();
    registry.emplace<engine::component::TransformComponent>(entity, pos);
    registry.emplace<engine::component::RenderComponent>(entity, engine::component::RenderComponent::MAIN_LAYER, pos.y);
    spatial.addTileEntityAtWorldPos(pos, entity, LAYER_BASE);
    return entity;
}

entt::entity soilAt(engine::spatial::SpatialIndexManager& spatial, glm::ivec2 tile_coord) {
    return spatial.getTileEntityAtWorldPos(tileWorldPos(tile_coord), game::defs::spatial_layer::SOIL);
}

} // namespace

namespace game::system {

TEST(FarmSystemHoeBlockingTest, AabbColliderOverTile_BlocksHoeButOtherTileCanHoe) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    engine::spatial::SpatialIndexManager spatial;
    spatial.initialize(registry,
                       /*map_size*/ MAP_SIZE,
                       /*tile_size*/ TILE_SIZE,
                       /*world_bounds_min*/ WORLD_MIN,
                       /*world_bounds_max*/ WORLD_MAX,
                       /*dynamic_cell_size*/ tileSizeF());

    engine::resource::AutoTileLibrary auto_tile_library;
    setupTilledRule(auto_tile_library);

    game::factory::BlueprintManager blueprint_manager;
    game::factory::EntityFactory entity_factory(registry, blueprint_manager, &spatial, &auto_tile_library);
    FarmSystem farm(registry, dispatcher, spatial, entity_factory, blueprint_manager, nullptr);

    const glm::ivec2 free_tile(1, 1);
    const glm::ivec2 blocked_tile(2, 1);

    spatial.setTileType(free_tile, engine::component::TileType::ARABLE);
    spatial.setTileType(blocked_tile, engine::component::TileType::ARABLE);
    addBaseTile(registry, spatial, free_tile);
    addBaseTile(registry, spatial, blocked_tile);

    const entt::entity obstacle = registry.create();
    registry.emplace<engine::component::TransformComponent>(obstacle, tileWorldPos(blocked_tile) + tileSizeF() * 0.5f);
    registry.emplace<engine::component::AABBCollider>(obstacle, tileSizeF(), glm::vec2(0.0f, 0.0f));
    spatial.addColliderEntity(obstacle);

    dispatcher.trigger(game::defs::UseToolEvent{game::defs::Tool::Hoe, tileWorldPos(free_tile)});
    EXPECT_TRUE(soilAt(spatial, free_tile) != entt::null);

    dispatcher.trigger(game::defs::UseToolEvent{game::defs::Tool::Hoe, tileWorldPos(blocked_tile)});
    EXPECT_TRUE(soilAt(spatial, blocked_tile) == entt::null);
}

TEST(FarmSystemHoeBlockingTest, CircleColliderOverTile_DoesNotBlockHoe) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    engine::spatial::SpatialIndexManager spatial;
    spatial.initialize(registry,
                       /*map_size*/ MAP_SIZE,
                       /*tile_size*/ TILE_SIZE,
                       /*world_bounds_min*/ WORLD_MIN,
                       /*world_bounds_max*/ WORLD_MAX,
                       /*dynamic_cell_size*/ tileSizeF());

    engine::resource::AutoTileLibrary auto_tile_library;
    setupTilledRule(auto_tile_library);

    game::factory::BlueprintManager blueprint_manager;
    game::factory::EntityFactory entity_factory(registry, blueprint_manager, &spatial, &auto_tile_library);
    FarmSystem farm(registry, dispatcher, spatial, entity_factory, blueprint_manager, nullptr);

    const glm::ivec2 tile(1, 1);
    spatial.setTileType(tile, engine::component::TileType::ARABLE);
    addBaseTile(registry, spatial, tile);

    const entt::entity actor = registry.create();
    registry.emplace<engine::component::TransformComponent>(actor, tileWorldPos(tile) + tileSizeF() * 0.5f);
    registry.emplace<engine::component::CircleCollider>(actor, 8.0f, glm::vec2(0.0f, 0.0f));
    spatial.addColliderEntity(actor);

    dispatcher.trigger(game::defs::UseToolEvent{game::defs::Tool::Hoe, tileWorldPos(tile)});
    EXPECT_TRUE(soilAt(spatial, tile) != entt::null);
}

TEST(FarmSystemHoeBlockingTest, HoeUsesMainLayerTileAsBase) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    engine::spatial::SpatialIndexManager spatial;
    spatial.initialize(registry,
                       /*map_size*/ MAP_SIZE,
                       /*tile_size*/ TILE_SIZE,
                       /*world_bounds_min*/ WORLD_MIN,
                       /*world_bounds_max*/ WORLD_MAX,
                       /*dynamic_cell_size*/ tileSizeF());

    engine::resource::AutoTileLibrary auto_tile_library;
    setupTilledRule(auto_tile_library);

    game::factory::BlueprintManager blueprint_manager;
    game::factory::EntityFactory entity_factory(registry, blueprint_manager, &spatial, &auto_tile_library);
    FarmSystem farm(registry, dispatcher, spatial, entity_factory, blueprint_manager, nullptr);

    const glm::ivec2 tile(1, 1);
    const glm::vec2 pos = tileWorldPos(tile);
    spatial.setTileType(tile, engine::component::TileType::ARABLE);

    const entt::entity main_tile = registry.create();
    registry.emplace<engine::component::TransformComponent>(main_tile, pos);
    registry.emplace<engine::component::RenderComponent>(main_tile, 10, 1.0f);
    spatial.addTileEntityAtWorldPos(pos, main_tile, game::defs::spatial_layer::MAIN);

    const entt::entity overlay_tile = registry.create();
    registry.emplace<engine::component::TransformComponent>(overlay_tile, pos);
    registry.emplace<engine::component::RenderComponent>(overlay_tile, 100, 2.0f);
    spatial.addTileEntityAtWorldPos(pos, overlay_tile, "overlay"_hs);

    dispatcher.trigger(game::defs::UseToolEvent{game::defs::Tool::Hoe, pos});

    const entt::entity soil = soilAt(spatial, tile);
    ASSERT_TRUE(soil != entt::null);

    const auto& render = registry.get<engine::component::RenderComponent>(soil);
    EXPECT_EQ(render.layer_, game::defs::render_layer::SOIL);
    EXPECT_FLOAT_EQ(render.depth_, 1.0f);
}

TEST(FarmSystemHoeBlockingTest, NeedRemoveTaggedAabbCollider_IgnoredForHoeBlocking) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    engine::spatial::SpatialIndexManager spatial;
    spatial.initialize(registry,
                       /*map_size*/ MAP_SIZE,
                       /*tile_size*/ TILE_SIZE,
                       /*world_bounds_min*/ WORLD_MIN,
                       /*world_bounds_max*/ WORLD_MAX,
                       /*dynamic_cell_size*/ tileSizeF());

    engine::resource::AutoTileLibrary auto_tile_library;
    setupTilledRule(auto_tile_library);

    game::factory::BlueprintManager blueprint_manager;
    game::factory::EntityFactory entity_factory(registry, blueprint_manager, &spatial, &auto_tile_library);
    FarmSystem farm(registry, dispatcher, spatial, entity_factory, blueprint_manager, nullptr);

    const glm::ivec2 tile(1, 1);
    spatial.setTileType(tile, engine::component::TileType::ARABLE);
    addBaseTile(registry, spatial, tile);

    const entt::entity obstacle = registry.create();
    registry.emplace<engine::component::TransformComponent>(obstacle, tileWorldPos(tile) + tileSizeF() * 0.5f);
    registry.emplace<engine::component::AABBCollider>(obstacle, tileSizeF(), glm::vec2(0.0f, 0.0f));
    registry.emplace<engine::component::NeedRemoveTag>(obstacle);
    spatial.addColliderEntity(obstacle);

    dispatcher.trigger(game::defs::UseToolEvent{game::defs::Tool::Hoe, tileWorldPos(tile)});
    EXPECT_TRUE(soilAt(spatial, tile) != entt::null);
}

} // namespace game::system
