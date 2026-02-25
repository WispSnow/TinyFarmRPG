#include <gtest/gtest.h>

#include "game/data/appearance_catalog.h"
#include "appearance_test_fixture_utils.h"

#include <filesystem>
#include <string>

namespace game::data {
namespace {

std::filesystem::path createCatalogFixture() {
    const auto temp_root = game::test::createUniqueTempDir("appearance_catalog_fixture");

    const auto textures_root = temp_root / "textures";
    game::test::touchPng(textures_root / "Idle/Hair/Standard/Brown.png");
    game::test::touchPng(textures_root / "Walk/Hair/Standard/Brown.png");
    game::test::touchPng(textures_root / "Hoe/Hair/Standard/Brown.png");
    game::test::touchPng(textures_root / "Walk/Eyes/Male/Blue.png");
    game::test::touchPng(textures_root / "Walk/Eyes/Female/Blue.png");
    game::test::touchPng(textures_root / "Hoe/Weapons/Hoe/1.png");

    const auto catalog_path = temp_root / "appearance_catalog.json";
    game::test::writeTextFile(
        catalog_path,
        R"json({
  "texture_root": "textures",
  "default_profile": "player_default",
  "layer_order": ["hair", "eyes", "weapon"],
  "runtime_switchable_slots": ["hair"],
  "slot_dirs": {
    "hair": "Hair",
    "eyes": "Eyes",
    "weapon": "Weapons"
  },
  "action_dirs": {
    "idle": "Idle",
    "walk": "Walk",
    "hoe": "Hoe"
  },
  "action_layouts": {
    "idle": {
      "frames_per_direction": 4,
      "direction_block_order": ["down", "up", "right", "left"],
      "left_fallback": "mirror_right"
    },
    "walk": {
      "frames_per_direction": 6,
      "direction_block_order": ["down", "up", "right", "left"],
      "left_fallback": "mirror_right"
    },
    "hoe": {
      "frames_per_direction": 6,
      "direction_block_order": ["down", "up", "right", "left"],
      "left_fallback": "mirror_right"
    }
  },
  "weapon_action_variants": {
    "hoe": "Hoe/1"
  },
  "profiles": {
    "player_default": {
      "gender": "male",
      "slots": {
        "hair": "Standard/Brown",
        "eyes": "Blue",
        "weapon": "auto"
      }
    }
  },
  "slot_variants": {
    "hair": ["Standard/Brown"]
  }
})json");
    return catalog_path;
}

TEST(AppearanceCatalogTest, ResolveActionSlotVariantToTexturePath) {
    constexpr entt::id_type kNullTexture{};
    AppearanceCatalog catalog;
    ASSERT_TRUE(catalog.loadFromFile(createCatalogFixture().string()));

    const auto idle_hair = catalog.resolveLayerTexture("idle", "hair", "Standard/Brown", "male");
    ASSERT_TRUE(idle_hair.has_value());
    EXPECT_NE(idle_hair->texture_id_, kNullTexture);
    EXPECT_NE(idle_hair->path_.find("Idle/Hair/Standard/Brown.png"), std::string::npos);
}

TEST(AppearanceCatalogTest, ResolvesGenderSpecificEyesPath) {
    AppearanceCatalog catalog;
    ASSERT_TRUE(catalog.loadFromFile(createCatalogFixture().string()));

    const auto male_eyes = catalog.resolveLayerTexture("walk", "eyes", "Blue", "male");
    const auto female_eyes = catalog.resolveLayerTexture("walk", "eyes", "Blue", "female");
    ASSERT_TRUE(male_eyes.has_value());
    ASSERT_TRUE(female_eyes.has_value());
    EXPECT_NE(male_eyes->path_.find("/Eyes/Male/Blue.png"), std::string::npos);
    EXPECT_NE(female_eyes->path_.find("/Eyes/Female/Blue.png"), std::string::npos);
}

TEST(AppearanceCatalogTest, BuildsActionAvailableSlotsMatrix) {
    AppearanceCatalog catalog;
    ASSERT_TRUE(catalog.loadFromFile(createCatalogFixture().string()));

    EXPECT_TRUE(catalog.isSlotAvailableForAction("idle", "hair"));
    EXPECT_FALSE(catalog.isSlotAvailableForAction("idle", "weapon"));
    EXPECT_TRUE(catalog.isSlotAvailableForAction("hoe", "weapon"));
}

TEST(AppearanceCatalogTest, RejectsUnknownActionOrVariant) {
    AppearanceCatalog catalog;
    ASSERT_TRUE(catalog.loadFromFile(createCatalogFixture().string()));

    EXPECT_FALSE(catalog.resolveLayerTexture("unknown_action", "hair", "Standard/Brown", "male").has_value());
    EXPECT_FALSE(catalog.resolveLayerTexture("idle", "hair", "NotExists/Variant", "male").has_value());
}

} // namespace
} // namespace game::data
