#include <gtest/gtest.h>

#include "game/component/appearance_component.h"
#include "game/data/appearance_catalog.h"
#include "game/scene/appearance_customize_types.h"
#include "game/ui/appearance_customize_view_model.h"
#include "appearance_test_fixture_utils.h"

#include <filesystem>
#include <random>
#include <utility>

namespace game::scene {
namespace {

std::filesystem::path createCatalogFixture() {
    const auto temp_root = game::test::createUniqueTempDir("appearance_customize_fixture");

    const auto textures_root = temp_root / "textures";
    game::test::touchPng(textures_root / "Idle/Skins/1.png");
    game::test::touchPng(textures_root / "Idle/Skins/2.png");
    game::test::touchPng(textures_root / "Idle/Hair/Standard/Brown.png");
    game::test::touchPng(textures_root / "Idle/Hair/Lyria/Brown.png");
    game::test::touchPng(textures_root / "Hoe/Weapons/Hoe/1.png");

    const auto catalog_path = temp_root / "appearance_catalog.json";
    game::test::writeTextFile(
        catalog_path,
        R"json({
  "texture_root": "textures",
  "default_profile": "player_default",
  "layer_order": ["skin", "hair", "weapon"],
  "runtime_switchable_slots": ["skin", "hair"],
  "slot_dirs": {
    "skin": "Skins",
    "hair": "Hair",
    "weapon": "Weapons"
  },
  "action_dirs": {
    "idle": "Idle",
    "hoe": "Hoe"
  },
  "action_layouts": {
    "idle": {
      "frames_per_direction": 4,
      "direction_block_order": ["down", "up", "right", "left"],
      "left_fallback": "none"
    },
    "hoe": {
      "frames_per_direction": 6,
      "direction_block_order": ["down", "up", "right", "left"],
      "left_fallback": "none"
    }
  },
  "weapon_action_variants": {
    "hoe": "Hoe/1"
  },
  "profiles": {
    "player_default": {
      "gender": "male",
      "slots": {
        "skin": "1",
        "hair": "Standard/Brown",
        "weapon": "auto"
      }
    }
  },
  "slot_variants": {
    "skin": ["1", "2"],
    "hair": ["Standard/Brown", "Lyria/Brown"]
  }
})json");
    return catalog_path;
}

TEST(AppearanceCustomizeViewModelTest, StepAndResetSelectionUseRuntimeSlotsOnly) {
    game::data::AppearanceCatalog catalog;
    ASSERT_TRUE(catalog.loadFromFile(createCatalogFixture().string()));

    auto selection = makeDefaultAppearanceSelection(catalog);
    EXPECT_EQ(selection.profile_id, "player_default");
    EXPECT_EQ(selection.slot_variants.at("hair"), "Standard/Brown");

    EXPECT_TRUE(stepAppearanceSlot(selection, catalog, "hair", 1));
    EXPECT_EQ(selection.slot_variants.at("hair"), "Lyria/Brown");

    EXPECT_FALSE(stepAppearanceSlot(selection, catalog, "weapon", 1));
    EXPECT_EQ(selection.slot_variants.at("weapon"), "auto");

    EXPECT_TRUE(resetSelectionToProfile(selection, catalog));
    EXPECT_EQ(selection.slot_variants.at("hair"), "Standard/Brown");
}

TEST(AppearanceCustomizeViewModelTest, BuildsSlotViewModelsInLayerOrder) {
    game::data::AppearanceCatalog catalog;
    ASSERT_TRUE(catalog.loadFromFile(createCatalogFixture().string()));

    auto selection = makeDefaultAppearanceSelection(catalog);
    EXPECT_TRUE(stepAppearanceSlot(selection, catalog, "hair", 1));

    const auto view_models = game::ui::buildAppearanceSlotViewModels(catalog, selection);
    ASSERT_EQ(view_models.size(), 2U);
    EXPECT_EQ(view_models[0].slot_id, "skin");
    EXPECT_EQ(view_models[0].label, "Skin");
    EXPECT_EQ(view_models[1].slot_id, "hair");
    EXPECT_EQ(view_models[1].variant_label, "Lyria Brown");
    EXPECT_EQ(view_models[1].index_label, "2/2");
}

TEST(AppearanceCustomizeViewModelTest, ApplySelectionUpdatesAppearanceComponent) {
    game::data::AppearanceCatalog catalog;
    ASSERT_TRUE(catalog.loadFromFile(createCatalogFixture().string()));

    auto selection = makeDefaultAppearanceSelection(catalog);
    selection.profile_id = "player_default";
    selection.gender = "female";
    selection.slot_variants["skin"] = "2";

    game::component::AppearanceComponent appearance{};
    applySelectionToComponent(selection, appearance);

    EXPECT_EQ(appearance.profile_id_, "player_default");
    EXPECT_EQ(appearance.gender_, "female");
    EXPECT_EQ(appearance.slot_variants_.at("skin"), "2");
    EXPECT_TRUE(appearance.dirty_);
}

TEST(AppearanceCustomizeViewModelTest, RandomizeTouchesRuntimeSlots) {
    game::data::AppearanceCatalog catalog;
    ASSERT_TRUE(catalog.loadFromFile(createCatalogFixture().string()));

    auto selection = makeDefaultAppearanceSelection(catalog);
    std::mt19937 rng{7U};

    EXPECT_TRUE(randomizeSelection(selection, catalog, rng));
    EXPECT_TRUE(selection.slot_variants.contains("skin"));
    EXPECT_TRUE(selection.slot_variants.contains("hair"));
    EXPECT_EQ(selection.slot_variants.at("weapon"), "auto");
}

} // namespace
} // namespace game::scene
