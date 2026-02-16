#include <gtest/gtest.h>

#include "engine/spatial/static_tile_grid.h"

namespace engine::spatial {

TEST(StaticTileGridRectQueryTest, HasSolidInRectDoesNotIncludeAdjacentTileOnBoundary) {
    StaticTileGrid grid;
    grid.initialize(glm::ivec2(2, 1), glm::ivec2(32, 32));
    grid.setTileType(glm::ivec2(1, 0), engine::component::TileType::SOLID);

    engine::utils::Rect rect(glm::vec2(0.0F, 0.0F), glm::vec2(32.0F, 32.0F));
    EXPECT_FALSE(grid.hasSolidInRect(rect));
    EXPECT_TRUE(grid.getSolidTilesInRect(rect).empty());
}

TEST(StaticTileGridRectQueryTest, HasSolidInRectIncludesTileWhenRectOverlapsBeyondBoundary) {
    StaticTileGrid grid;
    grid.initialize(glm::ivec2(2, 1), glm::ivec2(32, 32));
    grid.setTileType(glm::ivec2(1, 0), engine::component::TileType::SOLID);

    engine::utils::Rect rect(glm::vec2(0.0F, 0.0F), glm::vec2(32.01F, 32.0F));
    EXPECT_TRUE(grid.hasSolidInRect(rect));
}

} // namespace engine::spatial

