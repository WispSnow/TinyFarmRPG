#include <gtest/gtest.h>

#include "engine/component/animation_component.h"
#include "engine/component/layered_sprite_component.h"
#include "game/component/appearance_component.h"
#include "game/data/appearance_catalog.h"
#include "game/defs/commands.h"
#include "game/system/appearance_system.h"
#include "appearance_test_fixture_utils.h"

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <filesystem>
#include <string>
#include <unordered_map>

using namespace entt::literals;

namespace game::system {
namespace {

std::filesystem::path writeCatalog(const std::filesystem::path& temp_root, bool include_left_block) {
    const auto textures_root = temp_root / "textures";
    game::test::touchPng(textures_root / "Idle/Hair/Standard/Brown.png");

    const auto catalog_path = temp_root / "appearance_catalog.json";
    const std::string layout_order = include_left_block
                                         ? R"json(["down", "up", "right", "left"])json"
                                         : R"json(["down", "up", "right"])json";
    game::test::writeTextFile(
        catalog_path,
        std::string(R"json({
  "texture_root": "textures",
  "default_profile": "player_default",
  "layer_order": ["hair"],
  "slot_dirs": {
    "hair": "Hair"
  },
  "action_dirs": {
    "idle": "Idle"
  },
  "action_layouts": {
    "idle": {
      "frames_per_direction": 4,
      "direction_block_order": )json") +
            layout_order +
            R"json(,
      "left_fallback": "mirror_right"
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
    "hair": ["Standard/Brown"]
  },
  "runtime_switchable_slots": ["hair"]
})json");
    return catalog_path;
}

engine::component::Animation makeAnimationWithFrames(std::string_view name,
                                                     const std::vector<int>& source_frame_indices) {
    engine::component::Animation animation{};
    animation.name_ = std::string(name);
    animation.texture_id_ = entt::hashed_string{name.data()}.value();
    animation.frames_.reserve(source_frame_indices.size());
    for (const int source_index : source_frame_indices) {
        engine::utils::Rect src{};
        src.pos.x = static_cast<float>(source_index * 32);
        src.pos.y = 0.0f;
        src.size.x = 32.0f;
        src.size.y = 32.0f;
        animation.frames_.emplace_back(src, 100.0f);
    }
    return animation;
}

TEST(AppearanceLayeredDirectionMappingTest, RebuildsLayoutWithSourceFrameIndexMapping) {
    const auto temp_root = game::test::createUniqueTempDir("appearance_layered_mapping_explicit_left");

    game::data::AppearanceCatalog catalog;
    ASSERT_TRUE(catalog.loadFromFile(writeCatalog(temp_root, true).string()));

    entt::registry registry;
    entt::dispatcher dispatcher;
    const entt::entity entity = registry.create();

    std::unordered_map<entt::id_type, engine::component::Animation> animations;
    animations.emplace("idle_left"_hs, makeAnimationWithFrames("idle_left", {0, 2, 3}));
    registry.emplace<engine::component::AnimationComponent>(entity, std::move(animations), "idle_left"_hs);

    game::component::AppearanceComponent appearance{};
    appearance.gender_ = "male";
    appearance.slot_variants_ = {{"hair", "Standard/Brown"}};
    registry.emplace<game::component::AppearanceComponent>(entity, std::move(appearance));
    registry.emplace<engine::component::LayeredSpriteComponent>(entity);

    AppearanceSystem system(registry, dispatcher, catalog);
    dispatcher.trigger(game::defs::RefreshAppearanceCommand{entity});

    const auto* layer = registry.get<engine::component::LayeredSpriteComponent>(entity).findLayer("hair");
    ASSERT_NE(layer, nullptr);
    const auto* layout = layer->resolveLayout("idle_left"_hs);
    ASSERT_NE(layout, nullptr);
    EXPECT_EQ(layout->direction_block_index_, 3u);
    EXPECT_EQ(layout->frames_per_direction_, 4u);
    EXPECT_FALSE(layout->use_animation_flip_);
    ASSERT_EQ(layout->source_frame_index_by_runtime_frame_.size(), 3u);
    EXPECT_EQ(layout->source_frame_index_by_runtime_frame_[0], 0u);
    EXPECT_EQ(layout->source_frame_index_by_runtime_frame_[1], 2u);
    EXPECT_EQ(layout->source_frame_index_by_runtime_frame_[2], 3u);
}

TEST(AppearanceLayeredDirectionMappingTest, FallsBackToMirroredRightWhenLeftBlockMissing) {
    const auto temp_root = game::test::createUniqueTempDir("appearance_layered_mapping_fallback_left");

    game::data::AppearanceCatalog catalog;
    ASSERT_TRUE(catalog.loadFromFile(writeCatalog(temp_root, false).string()));

    entt::registry registry;
    entt::dispatcher dispatcher;
    const entt::entity entity = registry.create();

    std::unordered_map<entt::id_type, engine::component::Animation> animations;
    animations.emplace("idle_left"_hs, makeAnimationWithFrames("idle_left", {0, 1, 2, 3}));
    registry.emplace<engine::component::AnimationComponent>(entity, std::move(animations), "idle_left"_hs);

    game::component::AppearanceComponent appearance{};
    appearance.gender_ = "male";
    appearance.slot_variants_ = {{"hair", "Standard/Brown"}};
    registry.emplace<game::component::AppearanceComponent>(entity, std::move(appearance));
    registry.emplace<engine::component::LayeredSpriteComponent>(entity);

    AppearanceSystem system(registry, dispatcher, catalog);
    dispatcher.trigger(game::defs::RefreshAppearanceCommand{entity});

    const auto* layer = registry.get<engine::component::LayeredSpriteComponent>(entity).findLayer("hair");
    ASSERT_NE(layer, nullptr);
    const auto* layout = layer->resolveLayout("idle_left"_hs);
    ASSERT_NE(layout, nullptr);
    EXPECT_EQ(layout->direction_block_index_, 2u);
    EXPECT_TRUE(layout->use_animation_flip_);
}

} // namespace
} // namespace game::system
