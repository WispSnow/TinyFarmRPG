#include <gtest/gtest.h>

#include "engine/core/time.h"

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

} // namespace engine::core
