#include <gtest/gtest.h>

#include "appearance_test_fixture_utils.h"
#include "engine/component/transform_component.h"
#include "game/data/quest_catalog.h"
#include "game/data/rpg_catalog.h"
#include "game/data/shop_catalog.h"
#include "game/ui/map_tab_content.h"
#include "game/world/world_state.h"
#include "../engine/render/test_source_utils.h"

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

struct MapMarkerCatalogFixture {
    game::data::QuestCatalog quest_catalog{};
    game::data::ShopCatalog shop_catalog{};
    game::data::RpgCatalog rpg_catalog{};
};

[[nodiscard]] MapMarkerCatalogFixture loadInlineMarkerCatalogs() {
    const auto temp_root = game::test::createUniqueTempDir("map_tab_content_catalog_fixture");

    const auto quests_path = temp_root / "quests.json";
    game::test::writeTextFile(
        quests_path,
        R"json({
  "schema_version": 1,
  "quests": [
    {
      "id": "quest.test.empty_description",
      "title": "Inline Quest",
      "description": "",
      "objectives": [
        {
          "id": "defeat_one",
          "kind": "defeat_enemy_count",
          "enemy_id": "enemy.test",
          "required_count": 1
        }
      ]
    }
  ]
})json");

    const auto shops_path = temp_root / "shops.json";
    game::test::writeTextFile(
        shops_path,
        R"json({
  "schema_version": 1,
  "shops": [
    {
      "id": "shop.test.empty_greeting",
      "title": "Inline Shop",
      "greeting": "",
      "buy_entries": [
        { "item_id": "potion", "buy_price": 1 }
      ]
    }
  ]
})json");

    const auto actors_path = temp_root / "actors.json";
    game::test::writeTextFile(
        actors_path,
        R"json({
  "actors": [
    {
      "id": "actor.inline",
      "display_name": "Inline Actor",
      "class_id": "class.test",
      "initial_level": 1,
      "max_level": 1
    }
  ]
})json");

    MapMarkerCatalogFixture fixture{};
    EXPECT_TRUE(fixture.quest_catalog.loadFromFile(quests_path.string()));
    EXPECT_TRUE(fixture.shop_catalog.loadFromFile(shops_path.string()));
    EXPECT_TRUE(fixture.rpg_catalog.loadActors(actors_path.string()));
    return fixture;
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
    EXPECT_EQ(state.map_status_text, "Current Position");
    EXPECT_EQ(state.map_preview_src, "generated://map-preview/home_exterior");
}

TEST(MapTabContentTest, BuildsPlayerMarkerFromRuntimeLocalPositionAfterMapSwitch) {
    entt::registry registry;
    const entt::entity player = registry.create();
    registry.emplace<engine::component::TransformComponent>(player, glm::vec2{100.0F, 200.0F});
    game::world::WorldState world_state = loadWorld("town"_hs);

    const MapTabViewState state = buildMapTabViewState(
        registry,
        player,
        &world_state,
        world_state.getCurrentMap(),
        MapTabPreviewInput{.source_uri = "generated://map-preview/town", .width = 560, .height = 400},
        {},
        {},
        nullptr,
        nullptr,
        0);

    EXPECT_EQ(state.map_title, "Town");
    ASSERT_EQ(state.map_markers.size(), 1U);
    EXPECT_EQ(state.map_markers[0].kind, "player");
    EXPECT_EQ(state.map_markers[0].left, "44dp");
    EXPECT_EQ(state.map_markers[0].top, "47dp");
    EXPECT_EQ(state.map_markers[0].width, "16dp");
    EXPECT_EQ(state.map_markers[0].height, "16dp");
}

TEST(MapTabContentTest, BuildsPlayerMarkerAndClampsOutOfRangePositionToMapBounds) {
    entt::registry registry;
    const entt::entity player = registry.create();
    registry.emplace<engine::component::TransformComponent>(player, glm::vec2{660.0F, 200.0F});
    game::world::WorldState world_state = loadWorld("town"_hs);

    const MapTabViewState state = buildMapTabViewState(
        registry,
        player,
        &world_state,
        world_state.getCurrentMap(),
        MapTabPreviewInput{.source_uri = "generated://map-preview/town", .width = 560, .height = 400},
        {},
        {},
        nullptr,
        nullptr,
        0);

    ASSERT_EQ(state.map_markers.size(), 1U);
    EXPECT_EQ(state.map_markers[0].kind, "player");
    EXPECT_EQ(state.map_markers[0].left, "189dp");
    EXPECT_EQ(state.map_markers[0].top, "47dp");
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
    EXPECT_TRUE(state.map_markers.empty());
    EXPECT_EQ(state.map_status_text, "Current Position");
}

TEST(MapTabContentTest, BuildsObjectMarkersAndDefaultsSelectionToFirstPlaceMarker) {
    entt::registry registry;
    const entt::entity player = registry.create();
    registry.emplace<engine::component::TransformComponent>(player, glm::vec2{280.0F, 200.0F});
    game::world::WorldState world_state = loadWorld();
    game::data::ShopCatalog shop_catalog;
    game::data::RpgCatalog rpg_catalog;
    ASSERT_TRUE(shop_catalog.loadFromFile(
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "assets/data/shops.json").lexically_normal().string()));
    ASSERT_TRUE(rpg_catalog.loadActors(
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "assets/data/rpg/actors.json").lexically_normal().string()));

    const std::vector<MapObjectMarker> static_place_markers{
        MapObjectMarker{
            .kind = MapObjectMarkerKind::Shop,
            .object_id = 1,
            .object_name = "merchant",
            .shop_id = "shop.village.general",
            .recruit_actor_id = "",
            .map_position = {370.0F, 197.0F},
        },
        MapObjectMarker{
            .kind = MapObjectMarkerKind::Npc,
            .object_id = 3,
            .object_name = "lyria",
            .shop_id = "",
            .recruit_actor_id = "actor.lyria",
            .map_position = {95.0F, 274.5F},
        },
    };
    const std::vector<QuestRuntimeMarker> quest_markers{
        QuestRuntimeMarker{
            .kind = QuestRuntimeMarkerKind::Offer,
            .object_id = 2,
            .quest_id = "quest.village.goblin_cleanup",
            .objective_id = "",
            .map_position = {503.0F, 175.5F},
            .title = "Slime Cleanup",
            .type_label = "Quest Available",
            .description = "Defeat slimes near the village.",
        },
    };

    const int selected = defaultMapMarkerSelection(true, static_place_markers.size() + quest_markers.size());
    const MapTabViewState state = buildMapTabViewState(
        registry,
        player,
        &world_state,
        world_state.getCurrentMap(),
        MapTabPreviewInput{.source_uri = "generated://map-preview/home_exterior", .width = 560, .height = 400},
        static_place_markers,
        quest_markers,
        &shop_catalog,
        &rpg_catalog,
        selected);

    ASSERT_EQ(state.map_markers.size(), 4U);
    EXPECT_EQ(selected, 1);
    EXPECT_EQ(state.map_markers[0].kind, "player");
    EXPECT_EQ(state.map_markers[1].kind, "quest_offer");
    EXPECT_TRUE(state.map_markers[1].is_selected);
    EXPECT_EQ(state.map_markers[1].title, "Slime Cleanup");
    EXPECT_EQ(state.map_markers[1].description, "Defeat slimes near the village.");
    EXPECT_EQ(state.map_markers[2].title, "Village General Store");
    EXPECT_EQ(state.map_markers[2].description, "Welcome to the shop");
    EXPECT_EQ(state.map_markers[3].title, "Lyria");
    EXPECT_TRUE(state.has_map_markers);
    EXPECT_TRUE(state.has_place_markers);
    EXPECT_TRUE(state.has_map_detail);
    EXPECT_EQ(state.map_detail_title, "Slime Cleanup");
    EXPECT_EQ(state.map_detail_type, "Quest Available");
}

TEST(MapTabContentTest, PlayerOnlyMapShowsNoPlacesMarkedDetailAfterSelectionFallback) {
    entt::registry registry;
    const entt::entity player = registry.create();
    registry.emplace<engine::component::TransformComponent>(player, glm::vec2{120.0F, 120.0F});
    game::world::WorldState world_state = loadWorld();

    const int selected = defaultMapMarkerSelection(true, 0U);
    const MapTabViewState state = buildMapTabViewState(
        registry,
        player,
        &world_state,
        world_state.getCurrentMap(),
        MapTabPreviewInput{.source_uri = "generated://map-preview/home_exterior", .width = 560, .height = 400},
        {},
        {},
        nullptr,
        nullptr,
        selected);

    ASSERT_EQ(state.map_markers.size(), 1U);
    EXPECT_EQ(selected, 0);
    EXPECT_FALSE(state.has_place_markers);
    EXPECT_TRUE(state.has_map_detail);
    EXPECT_EQ(state.map_detail_title, "Current Position");
    EXPECT_EQ(state.map_detail_type, "Player");
}

TEST(MapTabContentTest, BuildsQuestRuntimeMarkerKindsAndOffsetsSamePositionMarkers) {
    entt::registry registry;
    const entt::entity player = registry.create();
    game::world::WorldState world_state = loadWorld("town"_hs);
    const std::vector<QuestRuntimeMarker> quest_markers{
        QuestRuntimeMarker{
            .kind = QuestRuntimeMarkerKind::TurnIn,
            .object_id = 1,
            .quest_id = "quest.alpha",
            .objective_id = "",
            .map_position = {120.0F, 64.0F},
            .title = "Alpha Quest",
            .type_label = "Ready to Turn In",
            .description = "Return to the quest giver.",
        },
        QuestRuntimeMarker{
            .kind = QuestRuntimeMarkerKind::Objective,
            .object_id = 0,
            .quest_id = "quest.beta",
            .objective_id = "find_bats",
            .map_position = {120.0F, 64.0F},
            .title = "Find Bats",
            .type_label = "Quest Objective",
            .description = "Bat 0/2",
        },
    };

    const MapTabViewState state = buildMapTabViewState(
        registry,
        player,
        &world_state,
        world_state.getCurrentMap(),
        MapTabPreviewInput{.source_uri = "generated://map-preview/town", .width = 560, .height = 400},
        {},
        quest_markers,
        nullptr,
        nullptr,
        0);

    ASSERT_EQ(state.map_markers.size(), 2U);
    EXPECT_EQ(state.map_markers[0].kind, "quest_turn_in");
    EXPECT_EQ(state.map_markers[1].kind, "quest_objective");
    EXPECT_FLOAT_EQ(state.map_markers[1].normal_top_left.x, state.map_markers[0].normal_top_left.x + 2.0F);
    EXPECT_EQ(state.map_detail_title, "Alpha Quest");
    EXPECT_EQ(state.map_detail_type, "Ready to Turn In");
}

TEST(MapTabContentTest, MissingCatalogsKeepMarkersWithStableFallbackText) {
    entt::registry registry;
    const entt::entity player = registry.create();
    registry.emplace<engine::component::TransformComponent>(player, glm::vec2{280.0F, 200.0F});
    game::world::WorldState world_state = loadWorld();

    const std::vector<MapObjectMarker> object_markers{
        MapObjectMarker{
            .kind = MapObjectMarkerKind::Shop,
            .object_id = 2,
            .object_name = "missing_shop",
            .shop_id = "shop.missing",
            .recruit_actor_id = "",
            .map_position = {503.0F, 175.5F},
        },
    };

    const MapTabViewState state = buildMapTabViewState(
        registry,
        player,
        &world_state,
        world_state.getCurrentMap(),
        MapTabPreviewInput{.source_uri = "generated://map-preview/home_exterior", .width = 560, .height = 400},
        object_markers,
        {},
        nullptr,
        nullptr,
        1);

    ASSERT_EQ(state.map_markers.size(), 2U);
    EXPECT_EQ(state.map_markers[1].title, "Missing Shop");
    EXPECT_EQ(state.map_markers[1].description, "Buy and sell supplies.");
    EXPECT_EQ(state.map_detail_title, "Missing Shop");
}

TEST(MapTabContentTest, InlineCatalogsResolveDetailsWithoutDependingOnProjectDataNames) {
    entt::registry registry;
    const entt::entity player = registry.create();
    registry.emplace<engine::component::TransformComponent>(player, glm::vec2{280.0F, 200.0F});
    game::world::WorldState world_state = loadWorld();
    MapMarkerCatalogFixture catalogs = loadInlineMarkerCatalogs();

    const std::vector<MapObjectMarker> object_markers{
        MapObjectMarker{
            .kind = MapObjectMarkerKind::Shop,
            .object_id = 2,
            .object_name = "shop_object",
            .shop_id = "shop.test.empty_greeting",
            .recruit_actor_id = "",
            .map_position = {220.0F, 180.0F},
        },
        MapObjectMarker{
            .kind = MapObjectMarkerKind::Npc,
            .object_id = 3,
            .object_name = "npc_object",
            .shop_id = "",
            .recruit_actor_id = "actor.inline",
            .map_position = {260.0F, 200.0F},
        },
        MapObjectMarker{
            .kind = MapObjectMarkerKind::Rest,
            .object_id = 4,
            .object_name = "",
            .shop_id = "",
            .recruit_actor_id = "",
            .map_position = {300.0F, 220.0F},
        },
    };

    const MapTabViewState state = buildMapTabViewState(
        registry,
        player,
        &world_state,
        world_state.getCurrentMap(),
        MapTabPreviewInput{.source_uri = "generated://map-preview/home_exterior", .width = 560, .height = 400},
        object_markers,
        {},
        &catalogs.shop_catalog,
        &catalogs.rpg_catalog,
        1);

    ASSERT_EQ(state.map_markers.size(), 4U);
    EXPECT_EQ(state.map_markers[1].title, "Inline Shop");
    EXPECT_EQ(state.map_markers[1].description, "Buy and sell supplies.");
    EXPECT_EQ(state.map_markers[2].title, "Inline Actor");
    EXPECT_EQ(state.map_markers[2].description, "Talk to this person.");
    EXPECT_EQ(state.map_markers[3].title, "Rest Point");
    EXPECT_EQ(state.map_markers[3].description, "Recover and pass time.");
    EXPECT_EQ(state.map_detail_title, "Inline Shop");
    EXPECT_EQ(state.map_detail_type, "Shop");
}

TEST(MapTabContentSourceTest, RegistersMapPreviewGeneratedTextureWithLinearSampling) {
    const auto source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/ui/map_tab_content.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(source.empty()) << "无法读取: " << source_path;

    EXPECT_NE(source.find("generated://map-preview/"), std::string::npos);
    EXPECT_NE(source.find("engine::ui::rmlui::RmlUiTextureFilterMode::Linear"), std::string::npos);
    EXPECT_NE(source.find("formatWholeDp(top_left.x)"), std::string::npos);
    EXPECT_NE(source.find("formatWholeDp(marker_size)"), std::string::npos);
}

} // namespace
} // namespace game::ui
