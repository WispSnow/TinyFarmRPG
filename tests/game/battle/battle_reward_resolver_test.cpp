// NOLINTBEGIN
#include <gtest/gtest.h>

#include "appearance_test_fixture_utils.h"
#include "game/battle/battle_reward_resolver.h"
#include "game/data/rpg_catalog.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace game::battle {
namespace {

[[nodiscard]] std::filesystem::path writeEnemyCatalog(std::string_view prefix, std::string_view json) {
    const auto temp_root = game::test::createUniqueTempDir(prefix);
    const auto enemies_path = temp_root / "enemies.json";
    game::test::writeTextFile(enemies_path, json);
    return enemies_path;
}

[[nodiscard]] BattleUnit makeUnit(const BattleUnitId id,
                                  std::string_view name,
                                  const BattleSide side,
                                  const int hp,
                                  const int max_hp,
                                  std::optional<std::string> source_enemy_id = std::nullopt) {
    return BattleUnit{
        .id = id,
        .name = std::string{name},
        .side = side,
        .hp = hp,
        .max_hp = max_hp,
        .mp = 0,
        .max_mp = 0,
        .attack = 10,
        .defense = 10,
        .magic_attack = 10,
        .magic_defense = 10,
        .speed = 10,
        .luck = 10,
        .source_enemy_id = std::move(source_enemy_id)};
}

[[nodiscard]] const BattleRewardItemDrop* findDrop(const BattleRewardSummary& summary, const std::string_view item_id) {
    for (const auto& drop : summary.item_drops) {
        if (drop.item_id == item_id) {
            return &drop;
        }
    }
    return nullptr;
}

[[nodiscard]] BattleRewardResolver::DropRollFn makeRollSequence(std::vector<float> rolls) {
    return [rolls = std::move(rolls), index = std::size_t{0}]() mutable -> float {
        if (index >= rolls.size()) {
            return 0.999F;
        }
        return rolls[index++];
    };
}

TEST(BattleRewardResolverTest, VictoryAggregatesPerDefeatedUnitAndMergesDrops) {
    const auto enemies_path = writeEnemyCatalog(
        "battle_reward_resolver_victory",
        R"json({
  "enemies": [
    {
      "id": "enemy.goblin",
      "display_name": "Goblin",
      "params": { "mhp": 30, "mmp": 0, "atk": 10, "def": 6, "mat": 2, "mdf": 2, "agi": 8, "luk": 4 },
      "exp": 10,
      "gold": 5,
      "drops": [
        { "item_id": "item.herb", "chance": 1.0 },
        { "item_id": "item.fang", "chance": 0.5 }
      ]
    },
    {
      "id": "enemy.bat",
      "display_name": "Bat",
      "params": { "mhp": 18, "mmp": 0, "atk": 7, "def": 4, "mat": 1, "mdf": 1, "agi": 12, "luk": 5 },
      "exp": 3,
      "gold": 2,
      "drops": [
        { "item_id": "item.herb", "chance": 1.0 }
      ]
    }
  ]
})json");

    game::data::RpgCatalog catalog;
    ASSERT_TRUE(catalog.loadEnemies(enemies_path.string()));

    const std::vector<BattleUnit> final_units{
        makeUnit(1, "Hero", BattleSide::Player, 50, 100),
        makeUnit(101, "Goblin A", BattleSide::Enemy, 0, 30, std::string{"enemy.goblin"}),
        makeUnit(102, "Goblin B", BattleSide::Enemy, 0, 30, std::string{"enemy.goblin"}),
        makeUnit(103, "Bat", BattleSide::Enemy, 0, 18, std::string{"enemy.bat"})};

    BattleRewardResolver resolver{makeRollSequence({0.0F, 0.9F, 0.0F, 0.1F, 0.0F})};
    const BattleRewardSummary summary = resolver.resolve(BattleOutcome::Victory, final_units, catalog);

    EXPECT_EQ(summary.gold_total, 12);
    EXPECT_EQ(summary.exp_total, 23);
    ASSERT_EQ(summary.item_drops.size(), 2U);

    const auto* herb = findDrop(summary, "item.herb");
    ASSERT_NE(herb, nullptr);
    EXPECT_EQ(herb->item_id_hash, game::data::RpgCatalog::hashId("item.herb"));
    EXPECT_EQ(herb->count, 3);

    const auto* fang = findDrop(summary, "item.fang");
    ASSERT_NE(fang, nullptr);
    EXPECT_EQ(fang->item_id_hash, game::data::RpgCatalog::hashId("item.fang"));
    EXPECT_EQ(fang->count, 1);
}

TEST(BattleRewardResolverTest, SkipsDefeatedEnemiesWithoutResolvableSource) {
    const auto enemies_path = writeEnemyCatalog(
        "battle_reward_resolver_skip_invalid_source",
        R"json({
  "enemies": [
    {
      "id": "enemy.slime",
      "display_name": "Slime",
      "params": { "mhp": 22, "mmp": 0, "atk": 6, "def": 4, "mat": 1, "mdf": 1, "agi": 5, "luk": 3 },
      "exp": 4,
      "gold": 2,
      "drops": [
        { "item_id": "item.slime_gel", "chance": 1.0 }
      ]
    }
  ]
})json");

    game::data::RpgCatalog catalog;
    ASSERT_TRUE(catalog.loadEnemies(enemies_path.string()));

    const std::vector<BattleUnit> final_units{
        makeUnit(1, "Hero", BattleSide::Player, 80, 100),
        makeUnit(101, "Nameless", BattleSide::Enemy, 0, 20),
        makeUnit(102, "Unknown", BattleSide::Enemy, 0, 20, std::string{"enemy.unknown"}),
        makeUnit(103, "Slime", BattleSide::Enemy, 0, 22, std::string{"enemy.slime"})};

    BattleRewardResolver resolver{makeRollSequence({0.0F})};
    const BattleRewardSummary summary = resolver.resolve(BattleOutcome::Victory, final_units, catalog);

    EXPECT_EQ(summary.gold_total, 2);
    EXPECT_EQ(summary.exp_total, 4);
    ASSERT_EQ(summary.item_drops.size(), 1U);

    const auto* slime_gel = findDrop(summary, "item.slime_gel");
    ASSERT_NE(slime_gel, nullptr);
    EXPECT_EQ(slime_gel->count, 1);
}

TEST(BattleRewardResolverTest, ReturnsEmptySummaryWhenOutcomeIsNotVictory) {
    const auto enemies_path = writeEnemyCatalog(
        "battle_reward_resolver_non_victory",
        R"json({
  "enemies": [
    {
      "id": "enemy.slime",
      "display_name": "Slime",
      "params": { "mhp": 22, "mmp": 0, "atk": 6, "def": 4, "mat": 1, "mdf": 1, "agi": 5, "luk": 3 },
      "exp": 4,
      "gold": 2,
      "drops": [
        { "item_id": "item.slime_gel", "chance": 1.0 }
      ]
    }
  ]
})json");

    game::data::RpgCatalog catalog;
    ASSERT_TRUE(catalog.loadEnemies(enemies_path.string()));

    const std::vector<BattleUnit> final_units{
        makeUnit(1, "Hero", BattleSide::Player, 0, 100),
        makeUnit(101, "Slime", BattleSide::Enemy, 0, 22, std::string{"enemy.slime"})};

    BattleRewardResolver resolver{makeRollSequence({0.0F})};

    const BattleRewardSummary defeat_summary = resolver.resolve(BattleOutcome::Defeat, final_units, catalog);
    EXPECT_TRUE(defeat_summary.empty());

    const BattleRewardSummary escaped_summary = resolver.resolve(BattleOutcome::Escaped, final_units, catalog);
    EXPECT_TRUE(escaped_summary.empty());

    const BattleRewardSummary ongoing_summary = resolver.resolve(BattleOutcome::Ongoing, final_units, catalog);
    EXPECT_TRUE(ongoing_summary.empty());
}

} // namespace
} // namespace game::battle
// NOLINTEND
