#include <gtest/gtest.h>

#include "game/data/appearance_catalog.h"
#include "game/scene/appearance_customize_types.h"
#include "game/ui/appearance_portrait_builder.h"
#include "game/ui/player_portrait_service.h"

#include "engine/ui/rmlui/rml_generated_image_registry.h"
#include "game/component/appearance_component.h"
#include "game/defs/events.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <filesystem>
#include <string>
#include <string_view>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace game::ui {
namespace {

std::filesystem::path projectPath(std::string_view relative_path) {
    return (std::filesystem::path{PROJECT_SOURCE_DIR} / relative_path).lexically_normal();
}

TEST(AppearancePortraitBuilderTest, BuildsStandardAndBattlePortraitCrops) {
    game::data::AppearanceCatalog catalog;
    ASSERT_TRUE(catalog.loadFromFile(projectPath("assets/data/appearance_catalog.json").string()));

    auto selection = game::scene::makeDefaultAppearanceSelection(catalog);
    selection.slot_variants["acc"] = "Santa Hat";

    AppearancePortraitBuilder builder;
    const auto images = builder.build(catalog, selection);

    ASSERT_TRUE(images.valid());
    EXPECT_EQ(images.standard64.width, 64);
    EXPECT_EQ(images.standard64.height, 64);
    EXPECT_EQ(images.standard64.channels, 4);
    EXPECT_EQ(images.battle48.width, 48);
    EXPECT_EQ(images.battle48.height, 48);
    EXPECT_EQ(images.battle48.channels, 4);
}

TEST(AppearancePortraitBuilderTest, SelectionKeyUsesStableCatalogOrder) {
    game::data::AppearanceCatalog catalog;
    ASSERT_TRUE(catalog.loadFromFile(projectPath("assets/data/appearance_catalog.json").string()));

    game::scene::AppearanceSelection first{};
    first.gender = "female";
    first.slot_variants.emplace("skin", "2");
    first.slot_variants.emplace("eyes", "Green");
    first.slot_variants.emplace("clothes", "Farm/Pink");
    first.slot_variants.emplace("hair", "Standard/Black");
    first.slot_variants.emplace("acc", "Pirate eyepatch");

    game::scene::AppearanceSelection second{};
    second.gender = "female";
    second.slot_variants.emplace("acc", "Pirate eyepatch");
    second.slot_variants.emplace("hair", "Standard/Black");
    second.slot_variants.emplace("clothes", "Farm/Pink");
    second.slot_variants.emplace("eyes", "Green");
    second.slot_variants.emplace("skin", "2");

    EXPECT_EQ(AppearancePortraitBuilder::stableSelectionKey(catalog, first),
              AppearancePortraitBuilder::stableSelectionKey(catalog, second));
}

TEST(PlayerPortraitServiceTest, RegistersImagesAndRefreshesOnAppearanceChangedEvent) {
    game::data::AppearanceCatalog catalog;
    ASSERT_TRUE(catalog.loadFromFile(projectPath("assets/data/appearance_catalog.json").string()));

    entt::registry registry;
    entt::dispatcher dispatcher;
    engine::ui::rmlui::RmlGeneratedImageRegistry generated_images;

    const auto* profile = catalog.defaultProfile();
    ASSERT_NE(profile, nullptr);

    const entt::entity player = registry.create();
    game::component::AppearanceComponent appearance{};
    appearance.profile_id_ = profile->id_;
    appearance.gender_ = profile->gender_;
    appearance.slot_variants_ = profile->slots_;
    registry.emplace<game::component::AppearanceComponent>(player, std::move(appearance));

    PlayerPortraitService service(
        dispatcher,
        registry,
        generated_images,
        catalog,
        player,
        "generated://player-portrait-test");

    ASSERT_TRUE(service.ready());
    const std::string initial_standard_uri{service.sourceUri(PortraitImageKind::Standard64)};
    const std::string initial_battle_uri{service.sourceUri(PortraitImageKind::Battle48)};
    EXPECT_FALSE(initial_standard_uri.empty());
    EXPECT_FALSE(initial_battle_uri.empty());
    EXPECT_EQ(service.decoratorString(PortraitImageKind::Standard64), "image(" + initial_standard_uri + ")");
    ASSERT_NE(generated_images.find(initial_standard_uri), nullptr);
    ASSERT_NE(generated_images.find(initial_battle_uri), nullptr);
    EXPECT_EQ(generated_images.find(initial_standard_uri)->width, 64);
    EXPECT_EQ(generated_images.find(initial_battle_uri)->width, 48);

    dispatcher.trigger(game::defs::AppearanceChangedEvent{registry.create()});
    EXPECT_EQ(service.sourceUri(PortraitImageKind::Standard64), initial_standard_uri);

    registry.get<game::component::AppearanceComponent>(player).slot_variants_["hair"] = "Fawn/Black";
    dispatcher.trigger(game::defs::AppearanceChangedEvent{player});

    const std::string refreshed_standard_uri{service.sourceUri(PortraitImageKind::Standard64)};
    EXPECT_NE(refreshed_standard_uri, initial_standard_uri);
    EXPECT_EQ(generated_images.find(initial_standard_uri), nullptr);
    EXPECT_NE(generated_images.find(refreshed_standard_uri), nullptr);

    auto& refreshed = registry.get<game::component::AppearanceComponent>(player);
    refreshed.gender_ = "unknown";
    refreshed.slot_variants_["skin"] = "missing";
    refreshed.slot_variants_["eyes"] = "missing";
    refreshed.slot_variants_["clothes"] = "missing";
    refreshed.slot_variants_["hair"] = "missing";
    refreshed.slot_variants_["acc"] = "missing";
    dispatcher.trigger(game::defs::AppearanceChangedEvent{player});

    EXPECT_EQ(service.sourceUri(PortraitImageKind::Standard64), refreshed_standard_uri);
    EXPECT_NE(generated_images.find(refreshed_standard_uri), nullptr);
}

} // namespace
} // namespace game::ui
