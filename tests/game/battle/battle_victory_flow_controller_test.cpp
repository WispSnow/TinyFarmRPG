// NOLINTBEGIN
#include <gtest/gtest.h>

#include "game/scene/battle_victory_flow_controller.h"

namespace game::scene {
namespace {

[[nodiscard]] game::battle::BattleRewardSummary makeRewardSummary() {
    return game::battle::BattleRewardSummary{
        .gold_total = 12,
        .exp_total = 34,
        .item_drops = {
            game::battle::BattleRewardItemDrop{
                .item_id = "item.herb",
                .item_id_hash = 42,
                .count = 2
            }
        }
    };
}

TEST(BattleVictoryFlowControllerTest, ConfirmDuringIntroSkipsToWaitConfirm) {
    BattleVictoryFlowController controller;
    controller.begin(makeRewardSummary());

    ASSERT_TRUE(controller.active());
    EXPECT_EQ(controller.snapshot().phase, BattleVictoryFlowPhase::Intro);
    EXPECT_EQ(controller.snapshot().gold.display, 0);
    EXPECT_EQ(controller.snapshot().exp.display, 0);

    controller.confirm();

    const BattleVictoryFlowSnapshot snapshot = controller.snapshot();
    EXPECT_TRUE(snapshot.waiting_for_confirm);
    EXPECT_FALSE(snapshot.finished);
    EXPECT_EQ(snapshot.phase, BattleVictoryFlowPhase::WaitConfirm);
    EXPECT_EQ(snapshot.gold.display, 12);
    EXPECT_EQ(snapshot.exp.display, 34);
    ASSERT_EQ(snapshot.item_drops.size(), 1U);
    EXPECT_EQ(snapshot.item_drops[0].count, 2);
}

TEST(BattleVictoryFlowControllerTest, SecondConfirmFinishesFlow) {
    BattleVictoryFlowController controller;
    controller.begin(makeRewardSummary());

    controller.confirm();
    EXPECT_FALSE(controller.finished());

    controller.confirm();
    EXPECT_TRUE(controller.finished());
}

TEST(BattleVictoryFlowControllerTest, RewardsCountUpThenWaitForConfirm) {
    BattleVictoryFlowController controller;
    controller.begin(makeRewardSummary());

    controller.update(0.25F);
    controller.update(0.25F);
    controller.update(0.2F);
    EXPECT_EQ(controller.snapshot().phase, BattleVictoryFlowPhase::Rewards);

    controller.update(0.25F);
    const int mid_count = controller.snapshot().gold.display;
    EXPECT_GT(mid_count, 0);
    EXPECT_LT(mid_count, 12);
    EXPECT_GT(controller.snapshot().exp.display, 0);
    EXPECT_LT(controller.snapshot().exp.display, 34);

    controller.update(0.25F);
    controller.update(0.25F);
    controller.update(0.25F);
    const BattleVictoryFlowSnapshot snapshot = controller.snapshot();
    EXPECT_TRUE(snapshot.waiting_for_confirm);
    EXPECT_EQ(snapshot.gold.display, 12);
    EXPECT_EQ(snapshot.exp.display, 34);
    EXPECT_FALSE(snapshot.finished);
}

} // namespace
} // namespace game::scene
// NOLINTEND
