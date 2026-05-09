// NOLINTBEGIN
#include "game/scene/battle_scene_entry.h"

#include "game/component/appearance_component.h"
#include "game/component/tags.h"
#include "game/data/appearance_catalog.h"

#include <entt/entity/registry.hpp>
#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace game::scene {
namespace {

[[nodiscard]] game::battle::BattleUnit makePlayerUnit(game::battle::BattleUnitId id, std::string actor_id) {
    game::battle::BattleUnit unit{};
    unit.id = id;
    unit.side = game::battle::BattleSide::Player;
    unit.source_actor_id = std::move(actor_id);
    return unit;
}

[[nodiscard]] game::battle::BattleUnit makeEnemyUnit(game::battle::BattleUnitId id, std::string enemy_id) {
    game::battle::BattleUnit unit{};
    unit.id = id;
    unit.side = game::battle::BattleSide::Enemy;
    unit.source_enemy_id = std::move(enemy_id);
    return unit;
}

TEST(BattleSceneEntryTest, CapturesPlayerBattleAppearanceFromRegistry) {
    entt::registry registry;
    const entt::entity player = registry.create();
    registry.emplace<game::component::PlayerTag>(player);

    game::component::AppearanceComponent appearance{};
    appearance.profile_id_ = "player_default";
    appearance.gender_ = "female";
    appearance.slot_variants_ = {
        {"hair", "Standard/Brown"},
        {"clothes", "Farm/Blue"},
    };
    registry.emplace<game::component::AppearanceComponent>(player, std::move(appearance));

    const auto snapshot = capturePlayerBattleAppearance(registry);
    ASSERT_TRUE(snapshot.has_value());
    EXPECT_TRUE(snapshot->valid);
    EXPECT_EQ(snapshot->profile_id, "player_default");
    EXPECT_EQ(snapshot->gender, "female");
    EXPECT_EQ(snapshot->slot_variants.at("hair"), "Standard/Brown");
    EXPECT_EQ(snapshot->slot_variants.at("clothes"), "Farm/Blue");
}

TEST(BattleSceneEntryTest, BuildsDefaultAppearanceSnapshotFromProfile) {
    game::data::AppearanceProfile profile{};
    profile.id_ = "player_default";
    profile.gender_ = "male";
    profile.slots_ = {
        {"skin", "1"},
        {"eyes", "Blue"},
    };

    const AppearanceSnapshot snapshot = makeBattleAppearanceSnapshot(profile);
    EXPECT_TRUE(snapshot.valid);
    EXPECT_EQ(snapshot.profile_id, "player_default");
    EXPECT_EQ(snapshot.gender, "male");
    EXPECT_EQ(snapshot.slot_variants.at("skin"), "1");
    EXPECT_EQ(snapshot.slot_variants.at("eyes"), "Blue");
}

TEST(BattleSceneEntryTest, AppliesPlayerAppearanceOnlyToDefaultPlayerActor) {
    AppearanceSnapshot player_appearance{};
    player_appearance.profile_id = "player_default";
    player_appearance.gender = "male";
    player_appearance.slot_variants = {{"hair", "Standard/Brown"}};
    player_appearance.valid = true;

    const std::vector<game::battle::BattleUnit> units{
        makePlayerUnit(1, "actor.player"),
        makePlayerUnit(2, "actor.lyria"),
        makeEnemyUnit(101, "enemy.goblin"),
    };

    const auto seeds = buildBattleSpriteSeeds(units, player_appearance);
    ASSERT_EQ(seeds.size(), 3U);

    EXPECT_EQ(seeds[0].unit_id, 1U);
    ASSERT_TRUE(seeds[0].appearance.has_value());
    EXPECT_EQ(seeds[0].appearance->slot_variants.at("hair"), "Standard/Brown");

    EXPECT_EQ(seeds[1].unit_id, 2U);
    EXPECT_FALSE(seeds[1].appearance.has_value());
    EXPECT_EQ(seeds[1].source_actor_id, std::optional<std::string>("actor.lyria"));

    EXPECT_EQ(seeds[2].unit_id, 101U);
    EXPECT_FALSE(seeds[2].appearance.has_value());
    EXPECT_EQ(seeds[2].source_enemy_id, std::optional<std::string>("enemy.goblin"));
}

} // namespace
} // namespace game::scene
// NOLINTEND
