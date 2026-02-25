#include <gtest/gtest.h>

#include "game/data/appearance_catalog.h"
#include "appearance_test_fixture_utils.h"

#include <filesystem>
#include <string>

namespace game::data {
namespace {

TEST(AppearanceCatalogLayoutTest, ParsesActionLayoutsAndDirectionFallback) {
    const auto temp_root = game::test::createUniqueTempDir("appearance_catalog_layout_test");

    const auto textures_root = temp_root / "textures";
    game::test::touchPng(textures_root / "Idle/Hair/Standard/Brown.png");
    game::test::touchPng(textures_root / "Idle/Hair/Standard/Black.png");
    game::test::touchPng(textures_root / "Idle/Hair/Standard/Blonde.png");
    game::test::touchPng(textures_root / "Walk/Hair/Standard/Brown.png");
    game::test::touchPng(textures_root / "Walk/Hair/Standard/Black.png");
    game::test::touchPng(textures_root / "Walk/Hair/Standard/Blonde.png");

    const auto catalog_path = temp_root / "appearance_catalog.json";
    game::test::writeTextFile(
        catalog_path,
        R"json({
  "texture_root": "textures",
  "default_profile": "player_default",
  "layer_order": ["hair"],
  "runtime_switchable_slots": ["hair"],
  "slot_dirs": {
    "hair": "Hair"
  },
  "action_dirs": {
    "idle": "Idle",
    "walk": "Walk"
  },
  "action_layouts": {
    "idle": {
      "frames_per_direction": 4,
      "direction_block_order": ["down", "up", "right"],
      "left_fallback": "mirror_right"
    },
    "walk": {
      "frames_per_direction": 6,
      "direction_block_order": ["down", "up", "right", "left"],
      "left_fallback": "none"
    }
  },
  "profiles": {
    "player_default": {
      "gender": "male",
      "slots": {
        "hair": "Standard/Brown"
      }
    }
  },
  "slot_variants": {
    "hair": ["Standard/Brown", "Standard/Black", "Standard/Blonde"]
  }
})json");

    AppearanceCatalog catalog;
    ASSERT_TRUE(catalog.loadFromFile(catalog_path.string()));

    const auto fallback_left = catalog.resolveLayerLayout("idle", "left");
    ASSERT_TRUE(fallback_left.has_value());
    EXPECT_EQ(fallback_left->direction_block_index_, 2u);
    EXPECT_EQ(fallback_left->frames_per_direction_, 4u);
    EXPECT_TRUE(fallback_left->use_animation_flip_);

    const auto explicit_left = catalog.resolveLayerLayout("walk", "left");
    ASSERT_TRUE(explicit_left.has_value());
    EXPECT_EQ(explicit_left->direction_block_index_, 3u);
    EXPECT_EQ(explicit_left->frames_per_direction_, 6u);
    EXPECT_FALSE(explicit_left->use_animation_flip_);

    const auto direction_key = catalog.directionKeyFromAnimationName("idle_left");
    ASSERT_TRUE(direction_key.has_value());
    EXPECT_EQ(*direction_key, "left");

    const auto* profile = catalog.defaultProfile();
    ASSERT_NE(profile, nullptr);
    const auto preload_paths = catalog.collectPreloadTexturePaths(*profile, 1);
    EXPECT_FALSE(preload_paths.empty());
    EXPECT_EQ(preload_paths.size(), 2u);
}

} // namespace
} // namespace game::data
