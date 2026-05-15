#include <gtest/gtest.h>

#include "game/ui/map_coordinate_mapper.h"

namespace game::ui {
namespace {

TEST(MapCoordinateMapperTest, FitsWideMapWithVerticalLetterbox) {
    const MapPreviewLayout layout = computeMapPreviewLayout({560.0F, 400.0F}, {218.0F, 126.0F});

    EXPECT_NEAR(layout.scale, 0.315F, 0.0001F);
    EXPECT_NEAR(layout.content_size.x, 176.4F, 0.001F);
    EXPECT_NEAR(layout.content_size.y, 126.0F, 0.001F);
    EXPECT_NEAR(layout.content_position.x, 20.8F, 0.001F);
    EXPECT_NEAR(layout.content_position.y, 0.0F, 0.001F);
}

TEST(MapCoordinateMapperTest, FitsInteriorSquareMapWithHorizontalLetterbox) {
    const MapPreviewLayout layout = computeMapPreviewLayout({240.0F, 240.0F}, {218.0F, 126.0F});

    EXPECT_NEAR(layout.scale, 0.525F, 0.0001F);
    EXPECT_NEAR(layout.content_size.x, 126.0F, 0.001F);
    EXPECT_NEAR(layout.content_size.y, 126.0F, 0.001F);
    EXPECT_NEAR(layout.content_position.x, 46.0F, 0.001F);
    EXPECT_NEAR(layout.content_position.y, 0.0F, 0.001F);
}

TEST(MapCoordinateMapperTest, FitsSchoolMapAndComputesBottomCenterMarkerTopLeft) {
    const MapPreviewLayout layout = computeMapPreviewLayout({480.0F, 272.0F}, {218.0F, 126.0F});
    const glm::vec2 top_left = mapMarkerBottomCenterTopLeft({240.0F, 136.0F}, layout, {218.0F, 126.0F}, {12.0F, 12.0F});

    EXPECT_NEAR(layout.scale, 218.0F / 480.0F, 0.0001F);
    EXPECT_NEAR(layout.content_position.y, (126.0F - 272.0F * layout.scale) * 0.5F, 0.001F);
    EXPECT_NEAR(top_left.x, 103.0F, 0.01F);
    EXPECT_NEAR(top_left.y, 51.0F, 0.01F);
}

TEST(MapCoordinateMapperTest, HandlesExtremeAspectRatios) {
    const MapPreviewLayout wide = computeMapPreviewLayout({1000.0F, 100.0F}, {218.0F, 126.0F});
    EXPECT_NEAR(wide.content_size.x, 218.0F, 0.001F);
    EXPECT_NEAR(wide.content_size.y, 21.8F, 0.001F);
    EXPECT_GT(wide.content_position.y, 0.0F);

    const MapPreviewLayout tall = computeMapPreviewLayout({100.0F, 1000.0F}, {218.0F, 126.0F});
    EXPECT_NEAR(tall.content_size.x, 12.6F, 0.001F);
    EXPECT_NEAR(tall.content_size.y, 126.0F, 0.001F);
    EXPECT_GT(tall.content_position.x, 0.0F);
}

TEST(MapCoordinateMapperTest, ComputesBottomCenterAnchorWithoutDriftingWhenMarkerSizeChanges) {
    const MapPreviewLayout layout = computeMapPreviewLayout({560.0F, 400.0F}, {218.0F, 126.0F});
    const glm::vec2 small = mapMarkerBottomCenterTopLeft({280.0F, 200.0F}, layout, {218.0F, 126.0F}, {12.0F, 12.0F});
    const glm::vec2 large = mapMarkerBottomCenterTopLeft({280.0F, 200.0F}, layout, {218.0F, 126.0F}, {14.0F, 14.0F});

    EXPECT_NEAR(small.x + 6.0F, large.x + 7.0F, 0.001F);
    EXPECT_NEAR(small.y + 12.0F, large.y + 14.0F, 0.001F);
}

TEST(MapCoordinateMapperTest, ClampsBottomCenterMarkerInsidePreviewFrame) {
    const MapPreviewLayout layout = computeMapPreviewLayout({560.0F, 400.0F}, {218.0F, 126.0F});

    const glm::vec2 top_left = mapMarkerBottomCenterTopLeft({0.0F, 0.0F}, layout, {218.0F, 126.0F}, {12.0F, 12.0F});
    const glm::vec2 bottom_right =
        mapMarkerBottomCenterTopLeft({560.0F, 400.0F}, layout, {218.0F, 126.0F}, {12.0F, 12.0F});

    EXPECT_GE(top_left.x, 0.0F);
    EXPECT_GE(top_left.y, 0.0F);
    EXPECT_LE(bottom_right.x, 206.0F);
    EXPECT_LE(bottom_right.y, 114.0F);
}

} // namespace
} // namespace game::ui
