#include <gtest/gtest.h>

#include "game/component/appearance_component.h"
#include "game/data/appearance_catalog.h"
#include "game/defs/commands.h"
#include "game/runtime/localization_service.h"
#include "game/scene/appearance_customize_types.h"
#include "game/ui/appearance_customize_view_model.h"
#include "appearance_test_fixture_utils.h"
#include "../engine/render/test_source_utils.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <algorithm>
#include <filesystem>
#include <random>
#include <utility>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace game::scene {
namespace {

std::filesystem::path projectPath(std::string_view relative_path) {
    return (std::filesystem::path{PROJECT_SOURCE_DIR} / relative_path).lexically_normal();
}

game::runtime::LocalizationService loadProjectLocalization(std::string_view language_tag) {
    game::runtime::LocalizationService localization;
    EXPECT_TRUE(localization.loadLanguageIndex(projectPath("assets/i18n/languages.json").string()));
    EXPECT_TRUE(localization.setLanguage(language_tag));
    return localization;
}

struct RefreshAppearanceProbe {
    int count{0};
    entt::entity last_target{entt::null};

    void onRefresh(const game::defs::RefreshAppearanceCommand& command) {
        ++count;
        last_target = command.target;
    }
};

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

    const auto view_models = game::ui::buildAppearanceSlotViewModels(catalog, selection, nullptr);
    ASSERT_EQ(view_models.size(), 2U);
    EXPECT_EQ(view_models[0].slot_id, "skin");
    EXPECT_EQ(view_models[0].label, "Skin");
    EXPECT_EQ(view_models[1].slot_id, "hair");
    EXPECT_EQ(view_models[1].variant_label, "Lyria Brown");
    EXPECT_EQ(view_models[1].index_label, "2/2");
}

TEST(AppearanceCustomizeViewModelTest, BuildsLocalizedProjectSlotViewModels) {
    game::data::AppearanceCatalog catalog;
    ASSERT_TRUE(catalog.loadFromFile(projectPath("assets/data/appearance_catalog.json").string()));
    auto localization = loadProjectLocalization("zh-Hans");

    const auto selection = makeDefaultAppearanceSelection(catalog);
    const auto view_models = game::ui::buildAppearanceSlotViewModels(catalog, selection, &localization);
    ASSERT_EQ(view_models.size(), 5U);

    EXPECT_EQ(view_models[0].slot_id, "skin");
    EXPECT_EQ(view_models[0].label, localization.tr("appearance.slot.skin"));
    EXPECT_EQ(view_models[0].variant_label, "1");

    EXPECT_EQ(view_models[1].slot_id, "eyes");
    EXPECT_EQ(view_models[1].label, localization.tr("appearance.slot.eyes"));
    EXPECT_EQ(view_models[1].variant_label, localization.tr("appearance.variant_part.blue"));

    EXPECT_EQ(view_models[2].slot_id, "clothes");
    EXPECT_EQ(view_models[2].label, localization.tr("appearance.slot.clothes"));
    EXPECT_EQ(view_models[2].variant_label,
              localization.format(
                  "appearance.variant.join",
                  {{"left", localization.tr("appearance.variant_part.farm")},
                   {"right", localization.tr("appearance.variant_part.blue")}}));

    EXPECT_EQ(view_models[3].slot_id, "hair");
    EXPECT_EQ(view_models[3].label, localization.tr("appearance.slot.hair"));
    EXPECT_EQ(view_models[3].variant_label,
              localization.format(
                  "appearance.variant.join",
                  {{"left", localization.tr("appearance.variant_part.standard")},
                   {"right", localization.tr("appearance.variant_part.brown")}}));

    EXPECT_EQ(view_models[4].slot_id, "acc");
    EXPECT_EQ(view_models[4].label, localization.tr("appearance.slot.acc"));
    EXPECT_EQ(view_models[4].variant_label, localization.tr("appearance.variant.none"));
}

TEST(AppearanceCustomizeViewModelTest, GenderControlOnlyAppearsWhenIncluded) {
    game::data::AppearanceCatalog catalog;
    ASSERT_TRUE(catalog.loadFromFile(projectPath("assets/data/appearance_catalog.json").string()));
    auto localization = loadProjectLocalization("zh-Hans");

    auto selection = makeDefaultAppearanceSelection(catalog);
    const auto closet_view_models = game::ui::buildAppearanceSlotViewModels(catalog, selection, &localization);
    EXPECT_EQ(std::find_if(closet_view_models.begin(), closet_view_models.end(), [](const auto& model) {
                  return model.slot_id == "gender";
              }),
              closet_view_models.end());

    const auto new_game_view_models = game::ui::buildAppearanceSlotViewModels(catalog, selection, &localization, true);
    ASSERT_FALSE(new_game_view_models.empty());
    EXPECT_EQ(new_game_view_models.front().slot_id, "gender");
    EXPECT_EQ(new_game_view_models.front().label, localization.tr("appearance.slot.gender"));
    EXPECT_EQ(new_game_view_models.front().variant_label, localization.tr("appearance.variant_part.male"));

    EXPECT_FALSE(stepAppearanceControl(selection, catalog, "gender", 1, false));
    EXPECT_EQ(selection.gender, "male");
    EXPECT_TRUE(stepAppearanceControl(selection, catalog, "gender", 1, true));
    EXPECT_EQ(selection.gender, "female");
}

TEST(AppearanceCustomizeViewModelTest, ProjectAppearanceVariantsHaveLocalizedLabels) {
    game::data::AppearanceCatalog catalog;
    ASSERT_TRUE(catalog.loadFromFile(projectPath("assets/data/appearance_catalog.json").string()));
    auto localization = loadProjectLocalization("zh-Hans");

    auto selection = makeDefaultAppearanceSelection(catalog);
    for (const auto& slot : runtimeAppearanceSlots(catalog)) {
        for (const auto& variant : catalog.variantsForSlot(slot)) {
            selection.slot_variants[slot] = variant;
            const auto view_models = game::ui::buildAppearanceSlotViewModels(catalog, selection, &localization);
            const auto it = std::find_if(view_models.begin(), view_models.end(), [&slot](const auto& model) {
                return model.slot_id == slot;
            });
            ASSERT_NE(it, view_models.end()) << slot;
            EXPECT_EQ(it->label.find('!'), Rml::String::npos) << slot;
            EXPECT_EQ(it->variant_label.find('!'), Rml::String::npos) << slot << "=" << variant;
        }
    }
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

TEST(AppearanceCustomizeViewModelTest, ApplySelectionToEntityBatchWritesAndEmitsOneRefresh) {
    game::data::AppearanceCatalog catalog;
    ASSERT_TRUE(catalog.loadFromFile(createCatalogFixture().string()));

    entt::registry registry;
    entt::dispatcher dispatcher;
    RefreshAppearanceProbe probe{};
    dispatcher.sink<game::defs::RefreshAppearanceCommand>().connect<&RefreshAppearanceProbe::onRefresh>(&probe);

    const entt::entity entity = registry.create();
    auto& appearance = registry.emplace<game::component::AppearanceComponent>(entity);
    appearance.profile_id_ = "player_default";
    appearance.gender_ = "male";
    appearance.slot_variants_ = {
        {"skin", "1"},
        {"hair", "Standard/Brown"},
        {"weapon", "auto"},
    };

    auto selection = makeDefaultAppearanceSelection(catalog);
    selection.gender = "female";
    selection.slot_variants["skin"] = "2";
    selection.slot_variants["hair"] = "Lyria/Brown";

    EXPECT_TRUE(applySelectionToEntity(registry, dispatcher, entity, selection));
    dispatcher.sink<game::defs::RefreshAppearanceCommand>().disconnect<&RefreshAppearanceProbe::onRefresh>(&probe);

    const auto& updated = registry.get<game::component::AppearanceComponent>(entity);
    EXPECT_EQ(updated.gender_, "female");
    EXPECT_EQ(updated.slot_variants_.at("skin"), "2");
    EXPECT_EQ(updated.slot_variants_.at("hair"), "Lyria/Brown");
    EXPECT_EQ(updated.slot_variants_.at("weapon"), "auto");
    EXPECT_TRUE(updated.dirty_);
    EXPECT_EQ(probe.count, 1);
    EXPECT_EQ(probe.last_target, entity);
}

TEST(AppearanceCustomizeViewModelTest, ApplySelectionToEntityRejectsInvalidTargetsWithoutRefresh) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    RefreshAppearanceProbe probe{};
    dispatcher.sink<game::defs::RefreshAppearanceCommand>().connect<&RefreshAppearanceProbe::onRefresh>(&probe);

    AppearanceSelection selection{};
    selection.gender = "female";
    selection.slot_variants["skin"] = "2";

    EXPECT_FALSE(applySelectionToEntity(registry, dispatcher, entt::null, selection));

    const entt::entity missing_component = registry.create();
    EXPECT_FALSE(applySelectionToEntity(registry, dispatcher, missing_component, selection));
    dispatcher.sink<game::defs::RefreshAppearanceCommand>().disconnect<&RefreshAppearanceProbe::onRefresh>(&probe);

    EXPECT_EQ(probe.count, 0);
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

TEST(AppearanceCustomizeViewModelTest, RandomizeCanIncludeGenderWhenRequested) {
    game::data::AppearanceCatalog catalog;
    ASSERT_TRUE(catalog.loadFromFile(projectPath("assets/data/appearance_catalog.json").string()));

    auto selection = makeDefaultAppearanceSelection(catalog);
    std::mt19937 rng{11U};

    selection.gender = "unknown";
    EXPECT_TRUE(randomizeSelection(selection, catalog, rng, false));
    EXPECT_EQ(selection.gender, "unknown");

    EXPECT_TRUE(randomizeSelection(selection, catalog, rng, true));
    const auto& genders = catalog.genderVariants();
    EXPECT_NE(std::find(genders.begin(), genders.end(), selection.gender), genders.end());
}

TEST(AppearanceCustomizeViewModelTest, SceneSuspendsLightingForNewGameAndClosetModes) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/appearance_customize_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(source.empty());
    const std::string suspend_block =
        test_source_utils::extractFunctionBlock(source, "void AppearanceCustomizeScene::suspendSceneLighting()");
    ASSERT_FALSE(suspend_block.empty());

    EXPECT_EQ(suspend_block.find("mode_ != Mode::Closet"), std::string::npos)
        << "New-game appearance customization also renders through the world pass, so it must not inherit stale "
           "GameScene ambient lighting.";
    EXPECT_NE(suspend_block.find("renderer.setLightingEnabled(false);"), std::string::npos);
    EXPECT_NE(source.find("suspendSceneLighting();"), std::string::npos);
}

TEST(AppearanceCustomizeViewModelTest, PreviewIdleAnimationUsesGameplayFrameDuration) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/appearance_customize_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(source.empty());
    const std::string preview_block =
        test_source_utils::extractFunctionBlock(source, "[[nodiscard]] engine::component::Animation makePreviewAnimation()");
    ASSERT_FALSE(preview_block.empty());

    EXPECT_NE(source.find("constexpr float PREVIEW_IDLE_FRAME_DURATION_MS = 200.0f;"), std::string::npos)
        << "The player idle animation in actor_blueprint.json uses 200ms per frame.";
    EXPECT_NE(preview_block.find("PREVIEW_IDLE_FRAME_DURATION_MS"), std::string::npos);
    EXPECT_EQ(preview_block.find("140.0f"), std::string::npos);
}

TEST(AppearanceCustomizeViewModelTest, SceneLocalizesDynamicTitleBindings) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/appearance_customize_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("appearance.title.new_game"), std::string::npos);
    EXPECT_NE(source.find("appearance.title.closet"), std::string::npos);
    EXPECT_NE(source.find("appearance.subtitle.new_game"), std::string::npos);
    EXPECT_NE(source.find("appearance.subtitle.closet"), std::string::npos);
    EXPECT_NE(source.find("sink<game::defs::LanguageChangedEvent>()"), std::string::npos);
    const std::string language_block =
        test_source_utils::extractFunctionBlock(source, "void AppearanceCustomizeScene::onLanguageChanged");
    ASSERT_FALSE(language_block.empty());
    EXPECT_NE(language_block.find("syncSlotViewModels(true);"), std::string::npos);
    EXPECT_NE(language_block.find("syncDefaultPlayerName(true);"), std::string::npos);
    EXPECT_NE(source.find("current_name.empty() || current_name == previous_default"), std::string::npos);
    EXPECT_EQ(source.find("makeRmlString(\"Create Hero\")"), std::string::npos);
    EXPECT_EQ(source.find("makeRmlString(\"Wardrobe\")"), std::string::npos);
}

TEST(AppearanceCustomizeViewModelTest, NewGameSceneBindsPlayerNameInputWithoutAffectingClosetSubtitle) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/appearance_customize_scene.cpp").lexically_normal();
    const std::filesystem::path rml_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "ui/rmlui/scenes/appearance_customize.rml").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;
    ASSERT_TRUE(std::filesystem::exists(rml_path)) << rml_path;

    const std::string source = test_source_utils::readTextFile(source_path);
    const std::string rml = test_source_utils::readTextFile(rml_path);
    ASSERT_FALSE(source.empty());
    ASSERT_FALSE(rml.empty());

    EXPECT_NE(source.find("show_name_input_ = mode_ == Mode::NewGame"), std::string::npos);
    EXPECT_NE(source.find("constructor.Bind(\"player_name\", &player_name_)"), std::string::npos);
    EXPECT_NE(source.find("constructor.Bind(\"show_name_input\", &show_name_input_)"), std::string::npos);
    EXPECT_EQ(source.find("\"player_name_changed\""), std::string::npos);
    EXPECT_NE(source.find("focused->GetId() == \"appearance-name-input\""), std::string::npos);
    EXPECT_NE(rml.find("id=\"appearance-name-label\""), std::string::npos);
    EXPECT_NE(rml.find("data-i18n=\"appearance.name_label\""), std::string::npos);
    EXPECT_NE(rml.find("id=\"appearance-name-input\""), std::string::npos);
    EXPECT_NE(rml.find("data-if=\"show_name_input\""), std::string::npos);
    EXPECT_NE(rml.find("data-if=\"!show_name_input\""), std::string::npos);
}

} // namespace
} // namespace game::scene
