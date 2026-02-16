#include <gtest/gtest.h>
#include "engine/render/nine_slice.h"
#include <glm/vec2.hpp>

namespace {

using engine::render::NineSlice;
using engine::render::NineSliceMargins;
using engine::render::NineSliceSection;
using engine::utils::Rect;

TEST(NineSliceTest, BuildsCorrectRects) {
    NineSliceMargins margins{10.0f, 8.0f, 12.0f, 6.0f};
    Rect source_rect{glm::vec2{0.0f, 0.0f}, glm::vec2{100.0f, 60.0f}};
    NineSlice slice(source_rect, margins);

    ASSERT_TRUE(slice.isValid());

    auto top_left = slice.getSliceRect(NineSliceSection::TopLeft);
    EXPECT_FLOAT_EQ(top_left.size.x, 10.0f);
    EXPECT_FLOAT_EQ(top_left.size.y, 8.0f);
    EXPECT_FLOAT_EQ(top_left.pos.x, 0.0f);
    EXPECT_FLOAT_EQ(top_left.pos.y, 0.0f);

    auto top_center = slice.getSliceRect(NineSliceSection::TopCenter);
    EXPECT_FLOAT_EQ(top_center.pos.x, 10.0f);
    EXPECT_FLOAT_EQ(top_center.pos.y, 0.0f);
    EXPECT_FLOAT_EQ(top_center.size.x, 78.0f);
    EXPECT_FLOAT_EQ(top_center.size.y, 8.0f);

    auto center = slice.getSliceRect(NineSliceSection::MiddleCenter);
    EXPECT_FLOAT_EQ(center.pos.x, 10.0f);
    EXPECT_FLOAT_EQ(center.pos.y, 8.0f);
    EXPECT_FLOAT_EQ(center.size.x, 78.0f);
    EXPECT_FLOAT_EQ(center.size.y, 46.0f);

    auto min_size = slice.getMinimumSize();
    EXPECT_FLOAT_EQ(min_size.x, 22.0f);
    EXPECT_FLOAT_EQ(min_size.y, 14.0f);
}

TEST(NineSliceTest, DetectsInvalidBorders) {
    NineSliceMargins margins{80.0f, 40.0f, 10.0f, 10.0f};
    Rect source_rect{glm::vec2{0.0f, 0.0f}, glm::vec2{100.0f, 20.0f}};
    NineSlice slice(source_rect, margins);
    EXPECT_FALSE(slice.isValid());
}

} // namespace


