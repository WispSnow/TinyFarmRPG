#include <gtest/gtest.h>

#include "game/scene/battle_enemy_hp_bar_controller.h"

#include <optional>
#include <utility>
#include <vector>

namespace game::scene {
namespace {

[[nodiscard]] game::battle::BattleUnit makeUnit(const game::battle::BattleUnitId id,
                                                const game::battle::BattleSide side,
                                                const int hp,
                                                const int max_hp = 100) {
    game::battle::BattleUnit unit{};
    unit.id = id;
    unit.side = side;
    unit.name = side == game::battle::BattleSide::Enemy ? "Slime" : "Hero";
    unit.hp = hp;
    unit.max_hp = max_hp;
    return unit;
}

[[nodiscard]] game::battle::BattleSnapshot makeSnapshot(
    std::vector<game::battle::BattleUnit> units = {
        makeUnit(1, game::battle::BattleSide::Player, 100),
        makeUnit(2, game::battle::BattleSide::Enemy, 100),
        makeUnit(3, game::battle::BattleSide::Enemy, 75)}) {
    game::battle::BattleSnapshot snapshot{};
    snapshot.units = std::move(units);
    snapshot.outcome = game::battle::BattleOutcome::Ongoing;
    return snapshot;
}

[[nodiscard]] game::battle::BattleActionResult makeDamageResult(
    const std::optional<game::battle::BattleUnitId> target_id = game::battle::BattleUnitId{2}) {
    game::battle::BattleActionResult result{};
    result.status = game::battle::BattleActionStatus::Applied;
    result.action_type = game::battle::BattleActionType::Attack;
    result.actor_id = 1;
    result.target_id = target_id;
    result.damage = 24;
    return result;
}

TEST(BattleEnemyHpBarControllerTest, SyncFromSnapshotTracksEnemiesOnly) {
    BattleEnemyHpBarController controller;
    controller.syncFromSnapshot(makeSnapshot());

    ASSERT_EQ(controller.activeBars().size(), 2U);
    EXPECT_EQ(controller.findBar(1), nullptr);
    EXPECT_NE(controller.findBar(2), nullptr);
    EXPECT_NE(controller.findBar(3), nullptr);
}

TEST(BattleEnemyHpBarControllerTest, InitialSyncSetsDisplayRatioWithoutSmoothing) {
    BattleEnemyHpBarController controller;
    controller.syncFromSnapshot(makeSnapshot({
        makeUnit(1, game::battle::BattleSide::Player, 100),
        makeUnit(2, game::battle::BattleSide::Enemy, 75)}));

    const auto* bar = controller.findBar(2);
    ASSERT_NE(bar, nullptr);
    EXPECT_FLOAT_EQ(bar->target_ratio, 0.75f);
    EXPECT_FLOAT_EQ(bar->display_ratio, 0.75f);
    EXPECT_FLOAT_EQ(bar->ratio_change_delay_seconds_remaining, 0.0f);
}

TEST(BattleEnemyHpBarControllerTest, RevealFromResultShowsEnemyHpBar) {
    BattleEnemyHpBarController controller;
    controller.syncFromSnapshot(makeSnapshot());
    controller.revealFromResult(makeDamageResult(2));

    const auto* bar = controller.findBar(2);
    ASSERT_NE(bar, nullptr);
    EXPECT_TRUE(bar->visible);
    EXPECT_FLOAT_EQ(bar->alpha, 1.0f);
    EXPECT_FLOAT_EQ(bar->visible_seconds_remaining, controller.config().reveal_seconds);
}

TEST(BattleEnemyHpBarControllerTest, RejectedOrNonEnemyResultDoesNotReveal) {
    BattleEnemyHpBarController controller;
    controller.syncFromSnapshot(makeSnapshot());

    auto rejected = makeDamageResult(2);
    rejected.status = game::battle::BattleActionStatus::Rejected;
    controller.revealFromResult(rejected);
    EXPECT_FALSE(controller.findBar(2)->visible);

    controller.revealFromResult(makeDamageResult(1));
    EXPECT_FALSE(controller.findBar(2)->visible);
}

TEST(BattleEnemyHpBarControllerTest, ChangeDelayHoldsDisplayRatioUntilImpactFrame) {
    BattleEnemyHpBarConfig config{};
    config.change_delay_seconds = 0.22f;
    config.ratio_lerp_speed = 1.0f;
    BattleEnemyHpBarController controller{config};
    controller.syncFromSnapshot(makeSnapshot({makeUnit(2, game::battle::BattleSide::Enemy, 100)}));

    controller.syncFromSnapshot(makeSnapshot({makeUnit(2, game::battle::BattleSide::Enemy, 50)}));
    ASSERT_NE(controller.findBar(2), nullptr);
    EXPECT_FLOAT_EQ(controller.findBar(2)->display_ratio, 1.0f);
    EXPECT_FLOAT_EQ(controller.findBar(2)->target_ratio, 0.5f);

    controller.update(0.10f);
    EXPECT_FLOAT_EQ(controller.findBar(2)->display_ratio, 1.0f);

    controller.update(0.20f);
    EXPECT_LT(controller.findBar(2)->display_ratio, 1.0f);
    EXPECT_GT(controller.findBar(2)->display_ratio, 0.5f);
}

TEST(BattleEnemyHpBarControllerTest, StagedSnapshotDoesNotChangeBarUntilReveal) {
    BattleEnemyHpBarConfig config{};
    config.change_delay_seconds = 0.0f;
    config.ratio_lerp_speed = 10.0f;
    BattleEnemyHpBarController controller{config};
    controller.syncFromSnapshot(makeSnapshot({makeUnit(2, game::battle::BattleSide::Enemy, 100)}));

    auto result = makeDamageResult(2);
    result.snapshot = makeSnapshot({makeUnit(2, game::battle::BattleSide::Enemy, 50)});
    controller.stageSnapshot(result.snapshot);

    ASSERT_NE(controller.findBar(2), nullptr);
    EXPECT_FLOAT_EQ(controller.findBar(2)->target_ratio, 1.0f);
    EXPECT_FLOAT_EQ(controller.findBar(2)->display_ratio, 1.0f);

    controller.applyStagedSnapshotAndReveal(result);
    controller.update(0.10f);

    EXPECT_FLOAT_EQ(controller.findBar(2)->target_ratio, 0.5f);
    EXPECT_TRUE(controller.findBar(2)->visible);
    EXPECT_LT(controller.findBar(2)->display_ratio, 1.0f);
}

TEST(BattleEnemyHpBarControllerTest, HiddenEnemiesStillTrackRatioAfterSnapshotChange) {
    BattleEnemyHpBarConfig config{};
    config.change_delay_seconds = 0.0f;
    config.ratio_lerp_speed = 4.0f;
    BattleEnemyHpBarController controller{config};
    controller.syncFromSnapshot(makeSnapshot({
        makeUnit(2, game::battle::BattleSide::Enemy, 100),
        makeUnit(3, game::battle::BattleSide::Enemy, 100)}));

    controller.syncFromSnapshot(makeSnapshot({
        makeUnit(2, game::battle::BattleSide::Enemy, 50),
        makeUnit(3, game::battle::BattleSide::Enemy, 25)}));
    controller.update(1.0f);

    ASSERT_NE(controller.findBar(2), nullptr);
    ASSERT_NE(controller.findBar(3), nullptr);
    EXPECT_FLOAT_EQ(controller.findBar(2)->display_ratio, 0.5f);
    EXPECT_FLOAT_EQ(controller.findBar(3)->display_ratio, 0.25f);
    EXPECT_FALSE(controller.findBar(2)->visible);
    EXPECT_FALSE(controller.findBar(3)->visible);
}

TEST(BattleEnemyHpBarControllerTest, RepeatedRevealRefreshesVisibleCountdown) {
    BattleEnemyHpBarController controller;
    controller.syncFromSnapshot(makeSnapshot());
    controller.revealFromResult(makeDamageResult(2));
    controller.update(1.0f);
    ASSERT_LT(controller.findBar(2)->visible_seconds_remaining, controller.config().reveal_seconds);

    controller.revealFromResult(makeDamageResult(2));

    EXPECT_FLOAT_EQ(controller.findBar(2)->visible_seconds_remaining, controller.config().reveal_seconds);
}

TEST(BattleEnemyHpBarControllerTest, RevealedBarExpiresAfterRevealAndFade) {
    BattleEnemyHpBarConfig config{};
    config.reveal_seconds = 0.20f;
    config.fade_seconds = 0.10f;
    BattleEnemyHpBarController controller{config};
    controller.syncFromSnapshot(makeSnapshot());
    controller.revealFromResult(makeDamageResult(2));

    controller.update(0.20f);
    EXPECT_TRUE(controller.findBar(2)->visible);
    controller.update(0.10f);

    EXPECT_FALSE(controller.findBar(2)->visible);
    EXPECT_FLOAT_EQ(controller.findBar(2)->alpha, 0.0f);
}

TEST(BattleEnemyHpBarControllerTest, HighlightForcesVisibleWithoutRevealCountdown) {
    BattleEnemyHpBarController controller;
    controller.syncFromSnapshot(makeSnapshot());

    controller.setHighlightedTarget(3);

    EXPECT_FALSE(controller.findBar(2)->visible);
    EXPECT_TRUE(controller.findBar(3)->visible);
    EXPECT_TRUE(controller.findBar(3)->highlighted);
    EXPECT_FLOAT_EQ(controller.findBar(3)->visible_seconds_remaining, 0.0f);
}

TEST(BattleEnemyHpBarControllerTest, LeavingTargetSelectFadesOutNonHitEnemiesImmediately) {
    BattleEnemyHpBarConfig config{};
    config.fade_seconds = 0.20f;
    BattleEnemyHpBarController controller{config};
    controller.syncFromSnapshot(makeSnapshot());
    controller.setHighlightedTarget(3);
    controller.setHighlightedTarget(std::nullopt);

    ASSERT_TRUE(controller.findBar(3)->visible);
    EXPECT_FALSE(controller.findBar(3)->highlighted);
    EXPECT_FLOAT_EQ(controller.findBar(3)->visible_seconds_remaining, 0.0f);

    controller.update(0.20f);

    EXPECT_FALSE(controller.findBar(3)->visible);
    EXPECT_FLOAT_EQ(controller.findBar(3)->alpha, 0.0f);
}

TEST(BattleEnemyHpBarControllerTest, DisabledControllerDoesNotRevealOnHit) {
    BattleEnemyHpBarConfig config{};
    BattleEnemyHpBarController controller{config};
    controller.setEnabled(false);
    EXPECT_FALSE(controller.isEnabled());

    controller.syncFromSnapshot(makeSnapshot());
    controller.revealFromResult(makeDamageResult(2));

    const auto* bar = controller.findBar(2);
    ASSERT_NE(bar, nullptr);
    EXPECT_FLOAT_EQ(bar->visible_seconds_remaining, 0.0f);
    EXPECT_FLOAT_EQ(bar->alpha, 0.0f);
    EXPECT_FALSE(bar->visible);
}

TEST(BattleEnemyHpBarControllerTest, DisablingClearsHighlightSoHighlightedBarAlsoFades) {
    BattleEnemyHpBarConfig config{};
    config.fade_seconds = 0.10f;
    BattleEnemyHpBarController controller{config};
    controller.syncFromSnapshot(makeSnapshot());
    controller.setHighlightedTarget(2);
    controller.update(0.05f);
    ASSERT_TRUE(controller.findBar(2)->highlighted);
    EXPECT_FLOAT_EQ(controller.findBar(2)->alpha, 1.0f);

    // 关闭后 highlight 应当被立即清掉，alpha 在 fade_seconds 内淡到 0。
    controller.setEnabled(false);
    EXPECT_FALSE(controller.findBar(2)->highlighted);
    controller.update(0.20f);  // 越过 fade_seconds
    EXPECT_FLOAT_EQ(controller.findBar(2)->alpha, 0.0f);
    EXPECT_FALSE(controller.findBar(2)->visible);
}

TEST(BattleEnemyHpBarControllerTest, DisablingMidFlightFadesExistingBarsToZero) {
    BattleEnemyHpBarConfig config{};
    config.reveal_seconds = 0.20f;
    config.fade_seconds = 0.10f;
    BattleEnemyHpBarController controller{config};
    controller.syncFromSnapshot(makeSnapshot());
    controller.revealFromResult(makeDamageResult(2));

    ASSERT_TRUE(controller.findBar(2)->visible);
    EXPECT_FLOAT_EQ(controller.findBar(2)->alpha, 1.0f);

    controller.setEnabled(false);
    // 已显示的血条立刻被新 spawn 拒绝（已有的不清空）；但 reveal 计时器走完 + 一次 update 后进入 fade 分支。
    controller.update(0.30f);  // 越过 reveal_seconds (0.20f)
    controller.update(0.20f);  // 越过 fade_seconds (0.10f)
    EXPECT_FLOAT_EQ(controller.findBar(2)->alpha, 0.0f);
    EXPECT_FALSE(controller.findBar(2)->visible);
}

TEST(BattleEnemyHpBarControllerTest, KoEnemySyncsToEmptyBarAndCanBeRevealed) {
    BattleEnemyHpBarConfig config{};
    config.change_delay_seconds = 0.0f;
    config.ratio_lerp_speed = 10.0f;
    BattleEnemyHpBarController controller{config};
    controller.syncFromSnapshot(makeSnapshot({makeUnit(2, game::battle::BattleSide::Enemy, 100)}));

    auto result = makeDamageResult(2);
    result.target_defeated = true;
    result.snapshot = makeSnapshot({makeUnit(2, game::battle::BattleSide::Enemy, 0)});
    controller.syncFromSnapshot(result.snapshot);
    controller.revealFromResult(result);
    controller.update(0.20f);

    const auto* bar = controller.findBar(2);
    ASSERT_NE(bar, nullptr);
    EXPECT_FLOAT_EQ(bar->target_ratio, 0.0f);
    EXPECT_FLOAT_EQ(bar->display_ratio, 0.0f);
    EXPECT_TRUE(bar->visible);
}

} // namespace
} // namespace game::scene
