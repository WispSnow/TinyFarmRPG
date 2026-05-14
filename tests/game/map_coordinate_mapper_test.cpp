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

TEST(MapCoordinateMapperTest, FitsSchoolMapAndComputesMarkerTopLeft) {
    const MapPreviewLayout layout = computeMapPreviewLayout({480.0F, 272.0F}, {218.0F, 126.0F});
    const glm::vec2 top_left = mapMarkerTopLeft({240.0F, 136.0F}, layout, {8.0F, 8.0F});

    EXPECT_NEAR(layout.scale, 218.0F / 480.0F, 0.0001F);
    EXPECT_NEAR(layout.content_position.y, (126.0F - 272.0F * layout.scale) * 0.5F, 0.001F);
    EXPECT_NEAR(top_left.x, 105.0F, 0.01F);
    EXPECT_NEAR(top_left.y, 59.0F, 0.01F);
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

} // namespace
} // namespace game::ui
