#include <gtest/gtest.h>

#include "game/data/appearance_catalog.h"
#include "appearance_test_fixture_utils.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <algorithm>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace game::data {
namespace {

std::filesystem::path projectPath(std::string_view relative_path) {
    return (std::filesystem::path{PROJECT_SOURCE_DIR} / relative_path).lexically_normal();
}

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

TEST(AppearanceCatalogTest, ProjectCatalogResolvesNewMappedAccessoryVariants) {
    AppearanceCatalog catalog;
    ASSERT_TRUE(catalog.loadFromFile(projectPath("assets/data/appearance_catalog.json").string()));

    const auto& acc_variants = catalog.variantsForSlot("acc");
    EXPECT_NE(std::find(acc_variants.begin(), acc_variants.end(), "Pirate eyepatch"), acc_variants.end());
    EXPECT_NE(std::find(acc_variants.begin(), acc_variants.end(), "Santa Hat"), acc_variants.end());
    EXPECT_EQ(std::find(acc_variants.begin(), acc_variants.end(), "Beret"), acc_variants.end());

    for (const auto& [action_key, _] : catalog.actionDirs()) {
        if (!catalog.isSlotAvailableForAction(action_key, "acc")) {
            continue;
        }
        EXPECT_TRUE(catalog.resolveLayerTexture(action_key, "acc", "Pirate eyepatch", "male").has_value())
            << action_key;
        EXPECT_TRUE(catalog.resolveLayerTexture(action_key, "acc", "Santa Hat", "male").has_value())
            << action_key;
    }
}

TEST(AppearanceCatalogTest, ProjectCatalogResolvesEveryRuntimeVariantForCharacterActions) {
    AppearanceCatalog catalog;
    ASSERT_TRUE(catalog.loadFromFile(projectPath("assets/data/appearance_catalog.json").string()));

    for (const auto& slot : catalog.layerOrder()) {
        if (!catalog.isRuntimeSwitchableSlot(slot)) {
            continue;
        }
        for (const auto& variant : catalog.variantsForSlot(slot)) {
            if (variant == "none") {
                continue;
            }
            for (const auto& gender : catalog.genderVariants()) {
                for (const auto& [action_key, _] : catalog.actionDirs()) {
                    if (!catalog.isSlotAvailableForAction(action_key, slot)) {
                        continue;
                    }
                    EXPECT_TRUE(catalog.resolveLayerTexture(action_key, slot, variant, gender).has_value())
                        << "action=" << action_key << " slot=" << slot << " variant=" << variant
                        << " gender=" << gender;
                }
            }
        }
    }
}

TEST(AppearanceCatalogTest, ProjectCatalogResolvesPortraitLayerMappings) {
    AppearanceCatalog catalog;
    ASSERT_TRUE(catalog.loadFromFile(projectPath("assets/data/appearance_catalog.json").string()));

    EXPECT_TRUE(catalog.resolvePortraitLayerTexture("skin", "1", "male").has_value());
    EXPECT_TRUE(catalog.resolvePortraitLayerTexture("skin", "1", "female").has_value());
    EXPECT_TRUE(catalog.resolvePortraitLayerTexture("ears", "Human/1", "male").has_value());
    EXPECT_TRUE(catalog.resolvePortraitLayerTexture("ears", "Elf/1", "male").has_value());
    EXPECT_TRUE(catalog.resolvePortraitLayerTexture("clothes", "Farm/Blue", "female").has_value());
    EXPECT_TRUE(catalog.resolvePortraitLayerTexture("hair", "Standard/Brown", "male").has_value());
    EXPECT_TRUE(catalog.resolvePortraitLayerTexture("acc", "Pirate eyepatch", "male").has_value());
    EXPECT_TRUE(catalog.resolvePortraitLayerTexture("acc", "Santa Hat", "male").has_value());
}

TEST(AppearanceCatalogTest, ProjectCatalogResolvesEveryRuntimeVariantForPortraitLayers) {
    AppearanceCatalog catalog;
    ASSERT_TRUE(catalog.loadFromFile(projectPath("assets/data/appearance_catalog.json").string()));

    for (const auto& gender : catalog.genderVariants()) {
        for (const auto& variant : catalog.variantsForSlot("skin")) {
            EXPECT_TRUE(catalog.resolvePortraitLayerTexture("skin", variant, gender).has_value())
                << "skin=" << variant << " gender=" << gender;
            EXPECT_TRUE(catalog.resolvePortraitLayerTexture("ears", "Human/" + variant, gender).has_value())
                << "human ears for skin=" << variant << " gender=" << gender;
        }
        for (const auto& variant : catalog.variantsForSlot("clothes")) {
            EXPECT_TRUE(catalog.resolvePortraitLayerTexture("clothes", variant, gender).has_value())
                << "clothes=" << variant << " gender=" << gender;
        }
        for (const auto& variant : catalog.variantsForSlot("eyes")) {
            EXPECT_TRUE(catalog.resolvePortraitLayerTexture("eyes", variant, gender).has_value())
                << "eyes=" << variant << " gender=" << gender;
        }
        for (const auto& variant : catalog.variantsForSlot("hair")) {
            EXPECT_TRUE(catalog.resolvePortraitLayerTexture("hair", variant, gender).has_value())
                << "hair=" << variant << " gender=" << gender;
        }
        for (const auto& variant : catalog.variantsForSlot("acc")) {
            if (variant == "none") {
                continue;
            }
            if (variant.rfind("Elf/", 0) == 0) {
                EXPECT_TRUE(catalog.resolvePortraitLayerTexture("ears", variant, gender).has_value())
                    << "elf ears=" << variant << " gender=" << gender;
                continue;
            }
            EXPECT_TRUE(catalog.resolvePortraitLayerTexture("acc", variant, gender).has_value())
                << "acc=" << variant << " gender=" << gender;
        }
    }
}

} // namespace
} // namespace game::data
