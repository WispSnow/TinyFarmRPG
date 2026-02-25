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

std::filesystem::path createCatalogFixture() {
    const auto temp_root = game::test::createUniqueTempDir("appearance_system_fixture");

    const auto textures_root = temp_root / "textures";
    game::test::touchPng(textures_root / "Idle/Hair/Standard/Brown.png");
    game::test::touchPng(textures_root / "Idle/Hair/Lyria/Brown.png");
    game::test::touchPng(textures_root / "Walk/Hair/Standard/Brown.png");
    game::test::touchPng(textures_root / "Walk/Hair/Lyria/Brown.png");
    game::test::touchPng(textures_root / "Hoe/Hair/Standard/Brown.png");
    game::test::touchPng(textures_root / "Hoe/Weapons/Hoe/1.png");

    const auto catalog_path = temp_root / "appearance_catalog.json";
    game::test::writeTextFile(
        catalog_path,
        R"json({
  "texture_root": "textures",
  "default_profile": "player_default",
  "layer_order": ["hair", "weapon"],
  "runtime_switchable_slots": ["hair"],
  "slot_dirs": {
    "hair": "Hair",
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
        "weapon": "auto"
      }
    }
  },
  "slot_variants": {
    "hair": ["Standard/Brown", "Lyria/Brown"]
  }
})json");
    return catalog_path;
}

[[nodiscard]] engine::component::Animation makeAnimation(std::string_view name) {
    engine::component::Animation animation{};
    animation.name_ = std::string(name);
    animation.texture_id_ = entt::hashed_string{name.data()}.value();
    engine::utils::Rect src{};
    src.pos = {0.0f, 0.0f};
    src.size = {32.0f, 32.0f};
    animation.frames_.emplace_back(src, 100.0f);
    return animation;
}

[[nodiscard]] entt::id_type resolveLayerTextureId(const engine::component::LayeredSpriteLayer& layer,
                                                  entt::id_type animation_id) {
    if (const auto* layout = layer.resolveLayout(animation_id)) {
        return layout->texture_id_;
    }
    return engine::component::LayeredSpriteLayer::INVALID_TEXTURE_ID;
}

TEST(AppearanceSystemTest, SetHairSlotRebuildsLayeredSpriteMapping) {
    constexpr entt::id_type kNullTexture{};
    game::data::AppearanceCatalog catalog;
    ASSERT_TRUE(catalog.loadFromFile(createCatalogFixture().string()));

    entt::registry registry;
    entt::dispatcher dispatcher;

    const entt::entity entity = registry.create();
    std::unordered_map<entt::id_type, engine::component::Animation> animations;
    animations.emplace("idle_down"_hs, makeAnimation("idle_down"));
    animations.emplace("walk_down"_hs, makeAnimation("walk_down"));
    registry.emplace<engine::component::AnimationComponent>(entity, std::move(animations), "idle_down"_hs);

    game::component::AppearanceComponent appearance{};
    appearance.gender_ = "male";
    appearance.slot_variants_ = {
        {"hair", "Standard/Brown"},
        {"weapon", "auto"},
    };
    registry.emplace<game::component::AppearanceComponent>(entity, std::move(appearance));
    registry.emplace<engine::component::LayeredSpriteComponent>(entity);

    AppearanceSystem system(registry, dispatcher, catalog);

    dispatcher.trigger(game::defs::RefreshAppearanceCommand{entity});
    const auto* before_layer = registry.get<engine::component::LayeredSpriteComponent>(entity).findLayer("hair");
    ASSERT_NE(before_layer, nullptr);
    const entt::id_type before_idle_texture = resolveLayerTextureId(*before_layer, "idle_down"_hs);
    EXPECT_NE(before_idle_texture, kNullTexture);

    dispatcher.trigger(game::defs::SetAppearanceSlotCommand{entity, "hair", "Lyria/Brown"});
    const auto* after_layer = registry.get<engine::component::LayeredSpriteComponent>(entity).findLayer("hair");
    ASSERT_NE(after_layer, nullptr);
    const entt::id_type after_idle_texture = resolveLayerTextureId(*after_layer, "idle_down"_hs);
    EXPECT_NE(after_idle_texture, kNullTexture);
    EXPECT_NE(after_idle_texture, before_idle_texture);
    EXPECT_EQ(registry.get<game::component::AppearanceComponent>(entity).slot_variants_.at("hair"), "Lyria/Brown");
}

TEST(AppearanceSystemTest, WeaponLayerVisibilityFollowsCurrentAction) {
    constexpr entt::id_type kNullTexture{};
    game::data::AppearanceCatalog catalog;
    ASSERT_TRUE(catalog.loadFromFile(createCatalogFixture().string()));

    entt::registry registry;
    entt::dispatcher dispatcher;

    const entt::entity entity = registry.create();
    std::unordered_map<entt::id_type, engine::component::Animation> animations;
    animations.emplace("idle_down"_hs, makeAnimation("idle_down"));
    animations.emplace("hoe_down"_hs, makeAnimation("hoe_down"));
    registry.emplace<engine::component::AnimationComponent>(entity, std::move(animations), "idle_down"_hs);

    game::component::AppearanceComponent appearance{};
    appearance.gender_ = "male";
    appearance.slot_variants_ = {
        {"hair", "Standard/Brown"},
        {"weapon", "auto"},
    };
    registry.emplace<game::component::AppearanceComponent>(entity, std::move(appearance));
    registry.emplace<engine::component::LayeredSpriteComponent>(entity);

    AppearanceSystem system(registry, dispatcher, catalog);
    dispatcher.trigger(game::defs::RefreshAppearanceCommand{entity});

    const auto* weapon_layer = registry.get<engine::component::LayeredSpriteComponent>(entity).findLayer("weapon");
    ASSERT_NE(weapon_layer, nullptr);
    EXPECT_EQ(resolveLayerTextureId(*weapon_layer, "idle_down"_hs), kNullTexture);
    EXPECT_NE(resolveLayerTextureId(*weapon_layer, "hoe_down"_hs), kNullTexture);
}

TEST(AppearanceSystemTest, MissingVariantFallsBackToPreviousSelection) {
    constexpr entt::id_type kNullTexture{};
    game::data::AppearanceCatalog catalog;
    ASSERT_TRUE(catalog.loadFromFile(createCatalogFixture().string()));

    entt::registry registry;
    entt::dispatcher dispatcher;

    const entt::entity entity = registry.create();
    std::unordered_map<entt::id_type, engine::component::Animation> animations;
    animations.emplace("idle_down"_hs, makeAnimation("idle_down"));
    registry.emplace<engine::component::AnimationComponent>(entity, std::move(animations), "idle_down"_hs);

    game::component::AppearanceComponent appearance{};
    appearance.gender_ = "male";
    appearance.slot_variants_ = {
        {"hair", "Standard/Brown"},
        {"weapon", "auto"},
    };
    registry.emplace<game::component::AppearanceComponent>(entity, std::move(appearance));
    registry.emplace<engine::component::LayeredSpriteComponent>(entity);

    AppearanceSystem system(registry, dispatcher, catalog);
    dispatcher.trigger(game::defs::RefreshAppearanceCommand{entity});
    const auto* before_layer = registry.get<engine::component::LayeredSpriteComponent>(entity).findLayer("hair");
    ASSERT_NE(before_layer, nullptr);
    const entt::id_type before_texture = resolveLayerTextureId(*before_layer, "idle_down"_hs);
    ASSERT_NE(before_texture, kNullTexture);

    dispatcher.trigger(game::defs::SetAppearanceSlotCommand{entity, "hair", "DoesNotExist"});
    const auto* after_layer = registry.get<engine::component::LayeredSpriteComponent>(entity).findLayer("hair");
    ASSERT_NE(after_layer, nullptr);
    const entt::id_type after_texture = resolveLayerTextureId(*after_layer, "idle_down"_hs);
    EXPECT_EQ(after_texture, before_texture);
    EXPECT_EQ(registry.get<game::component::AppearanceComponent>(entity).slot_variants_.at("hair"), "Standard/Brown");
}

} // namespace
} // namespace game::system
