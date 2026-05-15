#include <gtest/gtest.h>

#include "engine/loader/tiled_json_cache.h"
#include "game/ui/map_marker_provider.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace game::ui {
namespace {

[[nodiscard]] std::filesystem::path makeTempRoot() {
    const auto root = std::filesystem::temp_directory_path() /
                      ("map_marker_provider_" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    return root;
}

void writeTextFile(const std::filesystem::path& path, std::string_view content) {
    std::ofstream file(path);
    ASSERT_TRUE(file.is_open()) << path;
    file << content;
}

TEST(MapMarkerProviderTest, ScansSupportedInteractiveObjectsAndSortsBySelectionOrder) {
    engine::loader::tiled::clearJsonCache();
    const std::filesystem::path root = makeTempRoot();
    const std::filesystem::path map_path = root / "markers.tmj";
    writeTextFile(
        map_path,
        R"json({
  "type": "map",
  "width": 10,
  "height": 10,
  "tilewidth": 16,
  "tileheight": 16,
  "layers": [
    {
      "type": "objectgroup",
      "name": "main",
      "visible": true,
      "objects": [
        { "id": 7, "name": "ambient", "type": "actor", "point": true, "x": 2, "y": 3 },
        {
          "id": 3,
          "name": "merchant",
          "type": "actor",
          "point": true,
          "x": 20,
          "y": 30,
          "properties": [
            { "name": "shop_id", "type": "string", "value": "shop.test" }
          ]
        },
        {
          "id": 1,
          "name": "quest",
          "type": "actor",
          "point": true,
          "x": 40,
          "y": 50,
          "properties": [
            { "name": "quest_offer_id", "type": "string", "value": "quest.test" }
          ]
        },
        {
          "id": 5,
          "name": "bed",
          "type": "rest",
          "x": 80,
          "y": 90,
          "width": 20,
          "height": 10
        },
        {
          "id": 4,
          "name": "recruit",
          "type": "actor",
          "point": true,
          "x": 60,
          "y": 70,
          "properties": [
            { "name": "recruit_actor_id", "type": "string", "value": "actor.test" }
          ]
        }
      ]
    }
  ]
})json");

    MapMarkerProvider provider;
    const std::vector<MapObjectMarker> markers = provider.markersForMap(map_path.string());

    ASSERT_EQ(markers.size(), 4U);
    EXPECT_EQ(markers[0].kind, MapObjectMarkerKind::Quest);
    EXPECT_EQ(markers[0].object_id, 1);
    EXPECT_EQ(markers[0].quest_id, "quest.test");
    EXPECT_EQ(markers[1].kind, MapObjectMarkerKind::Shop);
    EXPECT_EQ(markers[1].object_id, 3);
    EXPECT_EQ(markers[1].shop_id, "shop.test");
    EXPECT_EQ(markers[2].kind, MapObjectMarkerKind::Rest);
    EXPECT_EQ(markers[2].object_id, 5);
    EXPECT_EQ(markers[2].map_position.x, 90.0F);
    EXPECT_EQ(markers[2].map_position.y, 100.0F);
    EXPECT_EQ(markers[3].kind, MapObjectMarkerKind::Npc);
    EXPECT_EQ(markers[3].recruit_actor_id, "actor.test");

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}

TEST(MapMarkerProviderTest, UsesShopMarkerWhenActorHasShopAndQuestToMatchRuntimeInteraction) {
    engine::loader::tiled::clearJsonCache();
    const std::filesystem::path root = makeTempRoot();
    const std::filesystem::path map_path = root / "priority.tmj";
    writeTextFile(
        map_path,
        R"json({
  "type": "map",
  "width": 10,
  "height": 10,
  "tilewidth": 16,
  "tileheight": 16,
  "layers": [
    {
      "type": "objectgroup",
      "name": "main",
      "visible": true,
      "objects": [
        {
          "id": 1,
          "name": "hybrid",
          "type": "actor",
          "point": true,
          "x": 12,
          "y": 18,
          "properties": [
            { "name": "shop_id", "type": "string", "value": "shop.hybrid" },
            { "name": "quest_offer_id", "type": "string", "value": "quest.hybrid" },
            { "name": "recruit_actor_id", "type": "string", "value": "actor.hybrid" }
          ]
        }
      ]
    }
  ]
})json");

    MapMarkerProvider provider;
    const std::vector<MapObjectMarker> markers = provider.markersForMap(map_path.string());

    ASSERT_EQ(markers.size(), 1U);
    EXPECT_EQ(markers[0].kind, MapObjectMarkerKind::Shop);
    EXPECT_EQ(markers[0].shop_id, "shop.hybrid");
    EXPECT_EQ(markers[0].quest_id, "quest.hybrid");
    EXPECT_EQ(markers[0].recruit_actor_id, "actor.hybrid");

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}

TEST(MapMarkerProviderTest, HandlesEllipsePolygonAndInvisibleObjects) {
    engine::loader::tiled::clearJsonCache();
    const std::filesystem::path root = makeTempRoot();
    const std::filesystem::path map_path = root / "shapes.tmj";
    writeTextFile(
        map_path,
        R"json({
  "type": "map",
  "width": 10,
  "height": 10,
  "tilewidth": 16,
  "tileheight": 16,
  "layers": [
    {
      "type": "objectgroup",
      "name": "main",
      "visible": true,
      "objects": [
        {
          "id": 1,
          "name": "ellipse_rest",
          "type": "rest",
          "ellipse": true,
          "x": 10,
          "y": 20,
          "width": 30,
          "height": 40
        },
        {
          "id": 2,
          "name": "poly_rest",
          "type": "rest",
          "x": 100,
          "y": 120,
          "polygon": [
            { "x": 0, "y": 0 },
            { "x": 20, "y": 10 },
            { "x": 10, "y": 30 }
          ]
        },
        {
          "id": 3,
          "name": "hidden_shop",
          "type": "actor",
          "point": true,
          "visible": false,
          "x": 8,
          "y": 9,
          "properties": [
            { "name": "shop_id", "type": "string", "value": "shop.hidden" }
          ]
        }
      ]
    }
  ]
})json");

    MapMarkerProvider provider;
    const std::vector<MapObjectMarker> markers = provider.markersForMap(map_path.string());

    ASSERT_EQ(markers.size(), 2U);
    EXPECT_EQ(markers[0].object_id, 1);
    EXPECT_EQ(markers[0].map_position.x, 25.0F);
    EXPECT_EQ(markers[0].map_position.y, 60.0F);
    EXPECT_EQ(markers[1].object_id, 2);
    EXPECT_EQ(markers[1].map_position.x, 110.0F);
    EXPECT_EQ(markers[1].map_position.y, 150.0F);

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}

} // namespace
} // namespace game::ui
