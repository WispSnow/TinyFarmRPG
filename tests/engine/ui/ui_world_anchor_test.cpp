#include <gtest/gtest.h>

#include "engine/render/camera.h"
#include "game/ui/world_anchor_state.h"

namespace game::ui {
namespace {

constexpr float kEpsilon = 0.001F;

void expectVec2Near(const glm::vec2& actual, const glm::vec2& expected) {
    EXPECT_NEAR(actual.x, expected.x, kEpsilon);
    EXPECT_NEAR(actual.y, expected.y, kEpsilon);
}

TEST(UIWorldAnchorTest, SetWorldAnchorFirstCallSnapshotsPreviousAsCurrent) {
    WorldAnchorState anchor{};
    anchor.setWorldAnchor({12.0F, 34.0F}, {1.0F, 2.0F});

    EXPECT_TRUE(anchor.hasWorldAnchor());
    EXPECT_EQ(anchor.mode(), WorldAnchorMode::WorldAnchor);
    EXPECT_EQ(anchor.worldAnchor(), glm::vec2(12.0F, 34.0F));
    EXPECT_EQ(anchor.previousWorldAnchor(), glm::vec2(12.0F, 34.0F));
    EXPECT_EQ(anchor.screenOffset(), glm::vec2(1.0F, 2.0F));
}

TEST(UIWorldAnchorTest, SetWorldAnchorSubsequentCallSnapshotsPreviousValue) {
    WorldAnchorState anchor{};
    anchor.setWorldAnchor({10.0F, 20.0F});
    anchor.setWorldAnchor({30.0F, 40.0F}, {3.0F, 4.0F});

    EXPECT_EQ(anchor.worldAnchor(), glm::vec2(30.0F, 40.0F));
    EXPECT_EQ(anchor.previousWorldAnchor(), glm::vec2(10.0F, 20.0F));
    EXPECT_EQ(anchor.screenOffset(), glm::vec2(3.0F, 4.0F));
}

TEST(UIWorldAnchorTest, ClearWorldAnchorRestoresScreenModeAndClearsState) {
    WorldAnchorState anchor{};
    anchor.setWorldAnchor({10.0F, 20.0F}, {3.0F, 4.0F});
    anchor.clearWorldAnchor();

    EXPECT_FALSE(anchor.hasWorldAnchor());
    EXPECT_EQ(anchor.mode(), WorldAnchorMode::Screen);
    EXPECT_EQ(anchor.worldAnchor(), glm::vec2(0.0F, 0.0F));
    EXPECT_EQ(anchor.previousWorldAnchor(), glm::vec2(0.0F, 0.0F));
    EXPECT_EQ(anchor.screenOffset(), glm::vec2(0.0F, 0.0F));
}

TEST(UIWorldAnchorTest, ResolveScreenAnchorPositionInterpolatesThroughCamera) {
    engine::render::Camera camera({160.0F, 120.0F});
    camera.setPosition({0.0F, 0.0F});
    camera.setZoom(2.0F);

    WorldAnchorState anchor{};
    anchor.setWorldAnchor({10.0F, 20.0F});
    anchor.setWorldAnchor({30.0F, 50.0F}, {3.0F, -4.0F});

    const glm::vec2 resolved = anchor.resolveScreenAnchorPosition(camera, 0.25F);
    expectVec2Near(resolved, {113.0F, 111.0F});
}

TEST(UIWorldAnchorTest, ResolveScreenAnchorPositionClampsInterpolationAlpha) {
    engine::render::Camera camera({160.0F, 120.0F});
    camera.setPosition({0.0F, 0.0F});

    WorldAnchorState anchor{};
    anchor.setWorldAnchor({10.0F, 20.0F});
    anchor.setWorldAnchor({30.0F, 40.0F});

    expectVec2Near(anchor.resolveScreenAnchorPosition(camera, -1.0F), {90.0F, 80.0F});
    expectVec2Near(anchor.resolveScreenAnchorPosition(camera, 2.0F), {110.0F, 100.0F});
}

} // namespace
} // namespace game::ui
