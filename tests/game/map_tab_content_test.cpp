#include <gtest/gtest.h>

#include "engine/component/transform_component.h"
#include "game/ui/map_tab_content.h"
#include "game/world/world_state.h"

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>

#include <filesystem>
#include <string_view>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

using namespace entt::literals;

namespace game::ui {
namespace {

[[nodiscard]] game::world::WorldState loadWorld(entt::id_type initial_map_id = "home_exterior"_hs) {
    game::world::WorldState world_state;
    const auto world_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "assets/maps/farm-rpg.world").lexically_normal();
    EXPECT_TRUE(world_state.loadFromWorldFile(world_path.string(), initial_map_id, "assets/maps/"));
    return world_state;
}

TEST(MapTabContentTest, BuildViewStateUsesCurrentMapPreviewAndPlayerLocalPosition) {
    entt::registry registry;
    const entt::entity player = registry.create();
    registry.emplace<engine::component::TransformComponent>(player, glm::vec2{280.0F, 200.0F});
    game::world::WorldState world_state = loadWorld();

    const MapTabViewState state = buildMapTabViewState(
        registry,
        player,
        &world_state,
        world_state.getCurrentMap(),
        MapTabPreviewInput{.source_uri = "generated://map-preview/home_exterior", .width = 560, .height = 400});

    EXPECT_EQ(state.map_title, "Home Exterior");
    EXPECT_TRUE(state.has_map_preview);
    EXPECT_TRUE(state.has_player_marker);
    EXPECT_EQ(state.map_status_text, "Current Position");
    EXPECT_EQ(state.map_preview_src, "generated://map-preview/home_exterior");
    EXPECT_NE(state.player_marker_left, "0dp");
    EXPECT_NE(state.player_marker_top, "0dp");
}

TEST(MapTabContentTest, BuildViewStateUsesRuntimeLocalPositionAfterMapSwitch) {
    entt::registry registry;
    const entt::entity player = registry.create();
    registry.emplace<engine::component::TransformComponent>(player, glm::vec2{100.0F, 200.0F});
    game::world::WorldState world_state = loadWorld("town"_hs);

    const MapTabViewState state = buildMapTabViewState(
        registry,
        player,
        &world_state,
        world_state.getCurrentMap(),
        MapTabPreviewInput{.source_uri = "generated://map-preview/town", .width = 560, .height = 400});

    EXPECT_TRUE(state.has_player_marker);
    EXPECT_EQ(state.map_title, "Town");
    EXPECT_EQ(state.player_marker_left, "48.30dp");
}

TEST(MapTabContentTest, BuildViewStateClampsOutOfRangePlayerPositionToMapBounds) {
    entt::registry registry;
    const entt::entity player = registry.create();
    registry.emplace<engine::component::TransformComponent>(player, glm::vec2{660.0F, 200.0F});
    game::world::WorldState world_state = loadWorld("town"_hs);

    const MapTabViewState state = buildMapTabViewState(
        registry,
        player,
        &world_state,
        world_state.getCurrentMap(),
        MapTabPreviewInput{.source_uri = "generated://map-preview/town", .width = 560, .height = 400});

    EXPECT_TRUE(state.has_player_marker);
    EXPECT_EQ(state.player_marker_left, "193.20dp");
    EXPECT_EQ(state.player_marker_top, "59.00dp");
}

TEST(MapTabContentTest, MissingWorldStateShowsNoMapData) {
    entt::registry registry;
    const entt::entity player = registry.create();

    const MapTabViewState state = buildMapTabViewState(
        registry,
        player,
        nullptr,
        entt::null,
        MapTabPreviewInput{.source_uri = "generated://map-preview/missing", .width = 560, .height = 400});

    EXPECT_FALSE(state.has_map_preview);
    EXPECT_FALSE(state.has_player_marker);
    EXPECT_EQ(state.map_status_text, "No map data");
}

TEST(MapTabContentTest, MissingPlayerTransformKeepsPreviewWithoutMarker) {
    entt::registry registry;
    const entt::entity player = registry.create();
    game::world::WorldState world_state = loadWorld();

    const MapTabViewState state = buildMapTabViewState(
        registry,
        player,
        &world_state,
        world_state.getCurrentMap(),
        MapTabPreviewInput{.source_uri = "generated://map-preview/home_exterior", .width = 560, .height = 400});

    EXPECT_TRUE(state.has_map_preview);
    EXPECT_FALSE(state.has_player_marker);
    EXPECT_EQ(state.map_status_text, "Current Position");
}

} // namespace
} // namespace game::ui
