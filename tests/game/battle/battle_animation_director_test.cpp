#include <gtest/gtest.h>

#include "game/scene/battle_animation_director.h"

#include <cmath>
#include <optional>
#include <vector>

namespace game::scene {
namespace {

[[nodiscard]] game::battle::BattleActionResult makeAttackResult(
    const std::optional<game::battle::BattleUnitId> target_id = game::battle::BattleUnitId{2}) {
    game::battle::BattleActionResult result{};
    result.status = game::battle::BattleActionStatus::Applied;
    result.action_type = game::battle::BattleActionType::Attack;
    result.actor_id = 1;
    result.target_id = target_id;
    result.damage = 24;
    return result;
}

[[nodiscard]] game::battle::BattleActionResult makeEnemySkillResult() {
    game::battle::BattleActionResult result{};
    result.status = game::battle::BattleActionStatus::Applied;
    result.action_type = game::battle::BattleActionType::Skill;
    result.actor_id = 2;
    result.target_id = game::battle::BattleUnitId{1};
    result.damage = 18;
    return result;
}

[[nodiscard]] game::battle::BattleActionResult makeEnemyAttackResult() {
    auto result = makeEnemySkillResult();
    result.action_type = game::battle::BattleActionType::Attack;
    return result;
}

[[nodiscard]] game::battle::BattleActionResult makePlayerMagicSkillResult() {
    game::battle::BattleActionResult result{};
    result.status = game::battle::BattleActionStatus::Applied;
    result.action_type = game::battle::BattleActionType::Skill;
    result.actor_id = 1;
    result.target_id = game::battle::BattleUnitId{2};
    result.damage = 18;
    return result;
}

[[nodiscard]] game::battle::BattleActionResult makeRecoverySkillResult() {
    game::battle::BattleActionResult result{};
    result.status = game::battle::BattleActionStatus::Applied;
    result.action_type = game::battle::BattleActionType::Skill;
    result.actor_id = 1;
    result.target_id = game::battle::BattleUnitId{2};
    result.hp_recovered = 18;
    return result;
}

[[nodiscard]] std::vector<BattlePresentationUnitAnchor> makeAnchors() {
    return {
        BattlePresentationUnitAnchor{
            .unit_id = 1,
            .side = game::battle::BattleSide::Player,
            .base_screen_position = glm::vec2{480.0f, 172.0f},
            .alive_after = true},
        BattlePresentationUnitAnchor{
            .unit_id = 2,
            .side = game::battle::BattleSide::Enemy,
            .base_screen_position = glm::vec2{160.0f, 172.0f},
            .alive_after = true}
    };
}

void advanceUntilFinished(BattleAnimationDirector& director) {
    for (int i = 0; i < 8 && !director.finished(); ++i) {
        director.update(0.12f);
    }
}

TEST(BattleAnimationDirectorTest, AttackStepsForwardThenReturnsToBase) {
    BattleAnimationDirector director;
    director.begin(makeAttackResult(), makeAnchors());

    director.update(0.16f);
    const auto lunge_pose = director.poseFor(1);
    ASSERT_TRUE(lunge_pose.has_value());
    EXPECT_LT(lunge_pose->offset.x, -1.0f);
    EXPECT_LT(lunge_pose->offset.y, 0.0f);

    director.update(0.20f);
    const auto returning_pose = director.poseFor(1);
    ASSERT_TRUE(returning_pose.has_value());
    EXPECT_LT(std::abs(returning_pose->offset.x), std::abs(lunge_pose->offset.x));

    advanceUntilFinished(director);
    EXPECT_TRUE(director.finished());
    EXPECT_FALSE(director.poseFor(1).has_value());
}

TEST(BattleAnimationDirectorTest, EnemyDamageSkillStepsTowardPlayerTarget) {
    BattleAnimationDirector director;
    director.begin(makeEnemySkillResult(), makeAnchors());

    director.update(0.16f);
    const auto lunge_pose = director.poseFor(2);
    ASSERT_TRUE(lunge_pose.has_value());
    EXPECT_GT(lunge_pose->offset.x, 1.0f);
    EXPECT_LT(lunge_pose->offset.y, 0.0f);
}

TEST(BattleAnimationDirectorTest, EnemyAttackStepsTowardPlayerTarget) {
    BattleAnimationDirector director;
    director.begin(makeEnemyAttackResult(), makeAnchors());

    director.update(0.16f);
    const auto lunge_pose = director.poseFor(2);
    ASSERT_TRUE(lunge_pose.has_value());
    EXPECT_GT(lunge_pose->offset.x, 1.0f);
}

TEST(BattleAnimationDirectorTest, CastSkillStartsFromActorOffsetAndOnlyStepsSlightlyForward) {
    BattleAnimationTimelineConfig config{};
    config.motion_style = BattleActionMotionStyle::Cast;
    config.actor_start_offset = glm::vec2{-12.0f, -2.0f};

    BattleAnimationDirector director;
    director.begin(makePlayerMagicSkillResult(), makeAnchors(), config);

    const auto start_pose = director.poseFor(1);
    ASSERT_TRUE(start_pose.has_value());
    EXPECT_NEAR(start_pose->offset.x, -12.0f, 0.001f);
    EXPECT_NEAR(start_pose->offset.y, -2.0f, 0.001f);

    director.update(0.16f);
    const auto cast_pose = director.poseFor(1);
    ASSERT_TRUE(cast_pose.has_value());
    EXPECT_LT(cast_pose->offset.x, start_pose->offset.x - 1.0f);
    EXPECT_GT(cast_pose->offset.x, -32.0f);
}

TEST(BattleAnimationDirectorTest, RecoveryTargetDoesNotUseHitShakePose) {
    BattleAnimationTimelineConfig config{};
    config.motion_style = BattleActionMotionStyle::Cast;

    BattleAnimationDirector director;
    director.begin(makeRecoverySkillResult(), makeAnchors(), config);

    director.update(0.24f);
    EXPECT_FALSE(director.poseFor(2).has_value());
}

TEST(BattleAnimationDirectorTest, MinimumDurationExtendsTimelineForTargetVfx) {
    BattleAnimationTimelineConfig config{};
    config.motion_style = BattleActionMotionStyle::Cast;
    config.minimum_duration_seconds = 1.05f;

    BattleAnimationDirector director;
    director.begin(makePlayerMagicSkillResult(), makeAnchors(), config);

    for (int i = 0; i < 5; ++i) {
        director.update(0.12f);
    }
    EXPECT_FALSE(director.finished());

    for (int i = 0; i < 5; ++i) {
        director.update(0.12f);
    }
    EXPECT_TRUE(director.finished());
}

TEST(BattleAnimationDirectorTest, HitFeedbackAppearsAfterImpactAndDecays) {
    BattleAnimationDirector director;
    director.begin(makeAttackResult(), makeAnchors());

    director.update(0.23f);
    const auto impact_pose = director.poseFor(2);
    ASSERT_TRUE(impact_pose.has_value());
    EXPECT_GT(std::abs(impact_pose->offset.x), 0.5f);
    EXPECT_GT(impact_pose->color_multiplier.r, 1.0f);
    EXPECT_LT(impact_pose->color_multiplier.g, 1.0f);

    director.update(0.28f);
    const auto decay_pose = director.poseFor(2);
    ASSERT_TRUE(decay_pose.has_value());
    EXPECT_LT(std::abs(decay_pose->offset.x), std::abs(impact_pose->offset.x));
}

TEST(BattleAnimationDirectorTest, DefeatedTargetKeepsPersistentKoPoseUntilReset) {
    auto result = makeAttackResult();
    result.target_defeated = true;

    BattleAnimationDirector director;
    director.begin(result, makeAnchors());
    advanceUntilFinished(director);

    EXPECT_TRUE(director.finished());
    const auto ko_pose = director.persistentPoseFor(2);
    ASSERT_TRUE(ko_pose.has_value());
    EXPECT_NEAR(ko_pose->rotation_radians, 1.5708f, 0.001f);
    EXPECT_LT(ko_pose->color_multiplier.a, 0.5f);

    director.reset();
    EXPECT_FALSE(director.persistentPoseFor(2).has_value());
}

TEST(BattleAnimationDirectorTest, DefeatedTargetKeepsKoPoseThroughTimelineTail) {
    auto result = makeAttackResult();
    result.target_defeated = true;

    BattleAnimationDirector director;
    director.begin(result, makeAnchors());
    director.update(0.25f);
    director.update(0.25f);
    director.update(0.10f);

    const auto ko_pose = director.poseFor(2);
    ASSERT_TRUE(ko_pose.has_value());
    EXPECT_GT(ko_pose->rotation_radians, 0.1f);
    EXPECT_LT(ko_pose->color_multiplier.a, 1.0f);
    EXPECT_FALSE(director.finished());
}

TEST(BattleAnimationDirectorTest, BeginKeepsPersistentKoPoseWithinSameBattle) {
    auto result = makeAttackResult();
    result.target_defeated = true;

    BattleAnimationDirector director;
    director.begin(result, makeAnchors());
    advanceUntilFinished(director);
    ASSERT_TRUE(director.persistentPoseFor(2).has_value());

    director.begin(makeAttackResult(2), makeAnchors());
    EXPECT_TRUE(director.persistentPoseFor(2).has_value());
}

TEST(BattleAnimationDirectorTest, MissingSingleTargetAttackAvoidsNaN) {
    BattleAnimationDirector director;
    director.begin(makeAttackResult(std::nullopt), makeAnchors());

    director.update(0.16f);
    const auto pose = director.poseFor(1);
    ASSERT_TRUE(pose.has_value());
    EXPECT_TRUE(std::isfinite(pose->offset.x));
    EXPECT_TRUE(std::isfinite(pose->offset.y));
    EXPECT_NEAR(pose->offset.x, 0.0f, 0.001f);
}

} // namespace
} // namespace game::scene
