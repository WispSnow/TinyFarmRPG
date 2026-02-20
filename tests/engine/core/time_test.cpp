#include <gtest/gtest.h>

#include "engine/core/time.h"
#include <SDL3/SDL_timer.h>

namespace engine::core {

TEST(TimeTest, NegativeTimeScaleClampsToZero) {
    Time time;
    time.setTimeScale(-3.0F);

    EXPECT_FLOAT_EQ(time.getTimeScale(), 0.0F);
}

TEST(TimeTest, SetTimeScaleStoresPositiveValue) {
    Time time;
    constexpr float expected_scale = 1.5F;

    time.setTimeScale(expected_scale);

    EXPECT_FLOAT_EQ(time.getTimeScale(), expected_scale);
}

TEST(TimeTest, NegativeFpsDisablesFrameLimiting) {
    Time time;

    time.setTargetFps(-60);

    EXPECT_EQ(time.getTargetFps(), 0);
}

TEST(TimeTest, PositiveFpsIsStored) {
    Time time;
    constexpr int target_fps = 144;

    time.setTargetFps(target_fps);

    EXPECT_EQ(time.getTargetFps(), target_fps);
}

TEST(TimeTest, InvalidFixedDeltaIsIgnored) {
    Time time;
    constexpr float initial_fixed_delta = 1.0F / 60.0F;

    EXPECT_FLOAT_EQ(time.getFixedDeltaTime(), initial_fixed_delta);

    time.setFixedDeltaTime(0.0F);
    EXPECT_FLOAT_EQ(time.getFixedDeltaTime(), initial_fixed_delta);

    time.setFixedDeltaTime(-0.1F);
    EXPECT_FLOAT_EQ(time.getFixedDeltaTime(), initial_fixed_delta);
}

TEST(TimeTest, MaxTicksPerFrameIsClampedToAtLeastOne) {
    Time time;

    time.setMaxTicksPerFrame(0);

    EXPECT_EQ(time.getMaxTicksPerFrame(), 1);
}

TEST(TimeTest, TryConsumeFixedTickRespectsPerFrameLimitAndReportsClamp) {
    Time time;
    time.setFixedDeltaTime(0.001F); // 1ms
    time.setMaxTicksPerFrame(2);
    time.clearAccumulator();

    SDL_Delay(30); // 保证累计时间足够超过 max tick 上限
    time.update();

    int consumed_ticks = 0;
    while (time.tryConsumeFixedTick()) {
        ++consumed_ticks;
    }

    EXPECT_EQ(consumed_ticks, 2);
    EXPECT_TRUE(time.wasCatchUpClampedThisFrame());
    EXPECT_GT(time.getDroppedFixedTicksTotal(), 0U);
}

} // namespace engine::core
