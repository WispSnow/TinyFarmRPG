#include <gtest/gtest.h>

#include "game/scene/battle_animation_director.h"

namespace game::scene {
namespace {

[[nodiscard]] BattleAnimationTimelineConfig makeReferenceConfig() {
    BattleAnimationTimelineConfig config{};
    config.attack_duration_seconds = 0.72f;
    config.action_hold_seconds = 0.34f;
    config.cast_duration_seconds = 0.46f;
    config.minimum_duration_seconds = 0.10f;
    config.duration_seconds = 0.80f;
    config.impact_time_seconds = 0.22f;
    config.hit_feedback_duration_seconds = 0.30f;
    config.weapon_windup_seconds = 0.08f;
    config.weapon_lunge_seconds = 0.14f;
    config.weapon_return_seconds = 0.20f;
    config.actor_start_offset = glm::vec2{12.0f, -4.0f};
    config.motion_style = BattleActionMotionStyle::WeaponAttack;
    return config;
}

TEST(BattleAnimationSpeedTest, SpeedOneIsNoOp) {
    auto config = makeReferenceConfig();
    const auto original = config;
    scaleAnimationTimeline(config, 1.0f);

    EXPECT_FLOAT_EQ(config.attack_duration_seconds, original.attack_duration_seconds);
    EXPECT_FLOAT_EQ(config.cast_duration_seconds, original.cast_duration_seconds);
    EXPECT_FLOAT_EQ(config.duration_seconds, original.duration_seconds);
    EXPECT_FLOAT_EQ(config.weapon_lunge_seconds, original.weapon_lunge_seconds);
}

TEST(BattleAnimationSpeedTest, SpeedTwoHalvesAllSecondsFields) {
    auto config = makeReferenceConfig();
    scaleAnimationTimeline(config, 2.0f);

    EXPECT_FLOAT_EQ(config.attack_duration_seconds, 0.72f / 2.0f);
    EXPECT_FLOAT_EQ(config.action_hold_seconds, 0.34f / 2.0f);
    EXPECT_FLOAT_EQ(config.cast_duration_seconds, 0.46f / 2.0f);
    EXPECT_FLOAT_EQ(config.minimum_duration_seconds, 0.10f / 2.0f);
    EXPECT_FLOAT_EQ(config.duration_seconds, 0.80f / 2.0f);
    EXPECT_FLOAT_EQ(config.impact_time_seconds, 0.22f / 2.0f);
    EXPECT_FLOAT_EQ(config.hit_feedback_duration_seconds, 0.30f / 2.0f);
    EXPECT_FLOAT_EQ(config.weapon_windup_seconds, 0.08f / 2.0f);
    EXPECT_FLOAT_EQ(config.weapon_lunge_seconds, 0.14f / 2.0f);
    EXPECT_FLOAT_EQ(config.weapon_return_seconds, 0.20f / 2.0f);
}

TEST(BattleAnimationSpeedTest, NonScalarFieldsAreUntouched) {
    auto config = makeReferenceConfig();
    const auto offset_before = config.actor_start_offset;
    const auto motion_before = config.motion_style;

    scaleAnimationTimeline(config, 3.0f);

    EXPECT_EQ(config.actor_start_offset, offset_before);
    EXPECT_EQ(config.motion_style, motion_before);
}

TEST(BattleAnimationSpeedTest, NonFiniteAndNonPositiveSpeedNoOp) {
    auto config = makeReferenceConfig();
    const auto original = config;

    scaleAnimationTimeline(config, 0.0f);
    EXPECT_FLOAT_EQ(config.attack_duration_seconds, original.attack_duration_seconds);

    scaleAnimationTimeline(config, -2.0f);
    EXPECT_FLOAT_EQ(config.attack_duration_seconds, original.attack_duration_seconds);

    scaleAnimationTimeline(config, std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(config.attack_duration_seconds, original.attack_duration_seconds);
}

} // namespace
} // namespace game::scene
