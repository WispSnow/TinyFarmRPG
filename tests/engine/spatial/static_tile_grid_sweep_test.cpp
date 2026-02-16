#include <gtest/gtest.h>

#include "engine/spatial/static_tile_grid.h"

namespace engine::spatial {

TEST(StaticTileGridSweepTest, HorizontalSweepEastDoesNotSkipBoundaryWhenNearEpsilon) {
    StaticTileGrid grid;
    grid.initialize(glm::ivec2(2, 1), glm::ivec2(32, 32));
    grid.setTileType(glm::ivec2(0, 0), engine::component::TileType::BLOCK_E);

    engine::utils::Rect start_rect(glm::vec2(21.9995F, 0.0F), glm::vec2(10.0F, 10.0F)); // right edge ~= 32 - 0.0005
    engine::utils::Rect end_rect = start_rect;
    end_rect.pos.x += 2.0F; // cross x=32 boundary

    auto [hit, resolved_x] = grid.sweepHorizontal(start_rect, end_rect, true);

    EXPECT_TRUE(hit);
    EXPECT_LT(resolved_x, end_rect.pos.x);
    EXPECT_LE(resolved_x + start_rect.size.x, 32.0F);
}

TEST(StaticTileGridSweepTest, HorizontalSweepWestDoesNotSkipBoundaryWhenNearEpsilon) {
    StaticTileGrid grid;
    grid.initialize(glm::ivec2(2, 1), glm::ivec2(32, 32));
    grid.setTileType(glm::ivec2(0, 0), engine::component::TileType::BLOCK_E);

    engine::utils::Rect start_rect(glm::vec2(32.0005F, 0.0F), glm::vec2(10.0F, 10.0F)); // left edge ~= 32 + 0.0005
    engine::utils::Rect end_rect = start_rect;
    end_rect.pos.x -= 2.0F; // cross x=32 boundary

    auto [hit, resolved_x] = grid.sweepHorizontal(start_rect, end_rect, false);

    EXPECT_TRUE(hit);
    EXPECT_GT(resolved_x, end_rect.pos.x);
    EXPECT_GE(resolved_x, 32.0F);
}

TEST(StaticTileGridSweepTest, VerticalSweepDownDoesNotSkipBoundaryWhenNearEpsilon) {
    StaticTileGrid grid;
    grid.initialize(glm::ivec2(1, 2), glm::ivec2(32, 32));
    grid.setTileType(glm::ivec2(0, 0), engine::component::TileType::BLOCK_S);

    engine::utils::Rect start_rect(glm::vec2(0.0F, 21.9995F), glm::vec2(10.0F, 10.0F)); // bottom edge ~= 32 - 0.0005
    engine::utils::Rect end_rect = start_rect;
    end_rect.pos.y += 2.0F; // cross y=32 boundary

    auto [hit, resolved_y] = grid.sweepVertical(start_rect, end_rect, true);

    EXPECT_TRUE(hit);
    EXPECT_LT(resolved_y, end_rect.pos.y);
    EXPECT_LE(resolved_y + start_rect.size.y, 32.0F);
}

TEST(StaticTileGridSweepTest, VerticalSweepUpDoesNotSkipBoundaryWhenNearEpsilon) {
    StaticTileGrid grid;
    grid.initialize(glm::ivec2(1, 2), glm::ivec2(32, 32));
    grid.setTileType(glm::ivec2(0, 0), engine::component::TileType::BLOCK_S);

    engine::utils::Rect start_rect(glm::vec2(0.0F, 32.0005F), glm::vec2(10.0F, 10.0F)); // top edge ~= 32 + 0.0005
    engine::utils::Rect end_rect = start_rect;
    end_rect.pos.y -= 2.0F; // cross y=32 boundary

    auto [hit, resolved_y] = grid.sweepVertical(start_rect, end_rect, false);

    EXPECT_TRUE(hit);
    EXPECT_GT(resolved_y, end_rect.pos.y);
    EXPECT_GE(resolved_y, 32.0F);
}

TEST(StaticTileGridSweepTest, ThinWallBlockedBetweenIsBidirectional) {
    StaticTileGrid grid;
    grid.initialize(glm::ivec2(2, 1), glm::ivec2(32, 32));
    grid.setTileType(glm::ivec2(0, 0), engine::component::TileType::BLOCK_E);

    EXPECT_TRUE(grid.isThinWallBlockedBetween(glm::ivec2(0, 0), glm::ivec2(1, 0)));
    EXPECT_TRUE(grid.isThinWallBlockedBetween(glm::ivec2(1, 0), glm::ivec2(0, 0)));
}

} // namespace engine::spatial

