#include <gtest/gtest.h>

#include "engine/spatial/spatial_index_manager.h"

namespace engine::spatial {

TEST(SpatialIndexManagerSweepResultTest, SolidCollisionIsNotReportedAsThinWallHit) {
    entt::registry registry;

    SpatialIndexManager spatial_index;
    spatial_index.initialize(registry,
                             glm::ivec2(2, 1),
                             glm::ivec2(32, 32),
                             glm::vec2(0.0f),
                             glm::vec2(64.0f, 32.0f),
                             glm::vec2(32.0f));

    spatial_index.setTileType(glm::ivec2(1, 0), engine::component::TileType::SOLID);

    engine::utils::Rect start_rect(glm::vec2(0.0f, 0.0f), glm::vec2(10.0f, 10.0f));
    engine::utils::Rect end_rect = start_rect;
    end_rect.pos.x += 40.0f;

    const auto result = spatial_index.resolveStaticSweep(start_rect, end_rect, Direction::East);

    EXPECT_LT(result.rect.pos.x, end_rect.pos.x);
    EXPECT_FALSE(result.hit_thin_wall);
}

TEST(SpatialIndexManagerSweepResultTest, ThinWallCollisionIsReportedAsThinWallHit) {
    entt::registry registry;

    SpatialIndexManager spatial_index;
    spatial_index.initialize(registry,
                             glm::ivec2(2, 1),
                             glm::ivec2(32, 32),
                             glm::vec2(0.0f),
                             glm::vec2(64.0f, 32.0f),
                             glm::vec2(32.0f));

    spatial_index.setTileType(glm::ivec2(0, 0), engine::component::TileType::BLOCK_E);

    engine::utils::Rect start_rect(glm::vec2(0.0f, 0.0f), glm::vec2(10.0f, 10.0f));
    engine::utils::Rect end_rect = start_rect;
    end_rect.pos.x += 40.0f;

    const auto result = spatial_index.resolveStaticSweep(start_rect, end_rect, Direction::East);

    EXPECT_LT(result.rect.pos.x, end_rect.pos.x);
    EXPECT_TRUE(result.hit_thin_wall);
}

} // namespace engine::spatial

