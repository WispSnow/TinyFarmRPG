#include <gtest/gtest.h>

#include "engine/component/animation_component.h"
#include "engine/component/layered_sprite_component.h"
#include "game/component/appearance_component.h"
#include "game/data/appearance_catalog.h"
#include "game/defs/commands.h"
#include "game/system/appearance_system.h"

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <filesystem>
#include <string>
#include <unordered_map>

using namespace entt::literals;

namespace game::system {
namespace {

[[nodiscard]] std::string catalogPath() {
    return (std::filesystem::path(PROJECT_SOURCE_DIR) / "assets/data/appearance_catalog.json").lexically_normal().string();
}

[[nodiscard]] engine::component::Animation makeAnimation(std::string_view name) {
    engine::component::Animation animation{};
    animation.name_ = std::string(name);
    animation.texture_id_ = entt::hashed_string{name.data()}.value();
    return animation;
}

TEST(AppearanceSystemTest, SetHairSlotRebuildsLayeredSpriteMapping) {
    constexpr entt::id_type kNullTexture{};
    game::data::AppearanceCatalog catalog;
    ASSERT_TRUE(catalog.loadFromFile(catalogPath()));

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
        {"skin", "1"},
        {"eyes", "Blue"},
        {"clothes", "Farm/Blue"},
        {"hair", "Standard/Brown"},
        {"acc", "none"},
        {"weapon", "auto"},
    };
    registry.emplace<game::component::AppearanceComponent>(entity, std::move(appearance));
    registry.emplace<engine::component::LayeredSpriteComponent>(entity);

    AppearanceSystem system(registry, dispatcher, catalog);

    dispatcher.trigger(game::defs::RefreshAppearanceCommand{entity});
    const auto* before_layer = registry.get<engine::component::LayeredSpriteComponent>(entity).findLayer("hair");
    ASSERT_NE(before_layer, nullptr);
    const entt::id_type before_idle_texture = before_layer->resolveTexture("idle_down"_hs);
    EXPECT_NE(before_idle_texture, kNullTexture);

    dispatcher.trigger(game::defs::SetAppearanceSlotCommand{entity, "hair", "Lyria/Brown"});
    const auto* after_layer = registry.get<engine::component::LayeredSpriteComponent>(entity).findLayer("hair");
    ASSERT_NE(after_layer, nullptr);
    const entt::id_type after_idle_texture = after_layer->resolveTexture("idle_down"_hs);
    EXPECT_NE(after_idle_texture, kNullTexture);
    EXPECT_NE(after_idle_texture, before_idle_texture);
    EXPECT_EQ(registry.get<game::component::AppearanceComponent>(entity).slot_variants_.at("hair"), "Lyria/Brown");
}

TEST(AppearanceSystemTest, WeaponLayerVisibilityFollowsCurrentAction) {
    constexpr entt::id_type kNullTexture{};
    game::data::AppearanceCatalog catalog;
    ASSERT_TRUE(catalog.loadFromFile(catalogPath()));

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
        {"skin", "1"},
        {"eyes", "Blue"},
        {"clothes", "Farm/Blue"},
        {"hair", "Standard/Brown"},
        {"acc", "none"},
        {"weapon", "auto"},
    };
    registry.emplace<game::component::AppearanceComponent>(entity, std::move(appearance));
    registry.emplace<engine::component::LayeredSpriteComponent>(entity);

    AppearanceSystem system(registry, dispatcher, catalog);
    dispatcher.trigger(game::defs::RefreshAppearanceCommand{entity});

    const auto* weapon_layer = registry.get<engine::component::LayeredSpriteComponent>(entity).findLayer("weapon");
    ASSERT_NE(weapon_layer, nullptr);
    EXPECT_EQ(weapon_layer->resolveTexture("idle_down"_hs), kNullTexture);
    EXPECT_NE(weapon_layer->resolveTexture("hoe_down"_hs), kNullTexture);
}

TEST(AppearanceSystemTest, MissingVariantFallsBackToPreviousSelection) {
    constexpr entt::id_type kNullTexture{};
    game::data::AppearanceCatalog catalog;
    ASSERT_TRUE(catalog.loadFromFile(catalogPath()));

    entt::registry registry;
    entt::dispatcher dispatcher;

    const entt::entity entity = registry.create();
    std::unordered_map<entt::id_type, engine::component::Animation> animations;
    animations.emplace("idle_down"_hs, makeAnimation("idle_down"));
    registry.emplace<engine::component::AnimationComponent>(entity, std::move(animations), "idle_down"_hs);

    game::component::AppearanceComponent appearance{};
    appearance.gender_ = "male";
    appearance.slot_variants_ = {
        {"skin", "1"},
        {"eyes", "Blue"},
        {"clothes", "Farm/Blue"},
        {"hair", "Standard/Brown"},
        {"acc", "none"},
        {"weapon", "auto"},
    };
    registry.emplace<game::component::AppearanceComponent>(entity, std::move(appearance));
    registry.emplace<engine::component::LayeredSpriteComponent>(entity);

    AppearanceSystem system(registry, dispatcher, catalog);
    dispatcher.trigger(game::defs::RefreshAppearanceCommand{entity});
    const auto* before_layer = registry.get<engine::component::LayeredSpriteComponent>(entity).findLayer("hair");
    ASSERT_NE(before_layer, nullptr);
    const entt::id_type before_texture = before_layer->resolveTexture("idle_down"_hs);
    ASSERT_NE(before_texture, kNullTexture);

    dispatcher.trigger(game::defs::SetAppearanceSlotCommand{entity, "hair", "DoesNotExist"});
    const auto* after_layer = registry.get<engine::component::LayeredSpriteComponent>(entity).findLayer("hair");
    ASSERT_NE(after_layer, nullptr);
    const entt::id_type after_texture = after_layer->resolveTexture("idle_down"_hs);
    EXPECT_EQ(after_texture, before_texture);
    EXPECT_EQ(registry.get<game::component::AppearanceComponent>(entity).slot_variants_.at("hair"), "Standard/Brown");
}

} // namespace
} // namespace game::system
