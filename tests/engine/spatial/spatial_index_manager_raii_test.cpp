#include <gtest/gtest.h>

#include "engine/spatial/spatial_index_manager.h"

#include <entt/entity/registry.hpp>

namespace engine::spatial {

TEST(SpatialIndexManagerRaiiTest, DefaultConstructedIsSafeToQuery) {
    SpatialIndexManager spatial;

    EXPECT_FALSE(spatial.isInitialized());

    const auto map_size = spatial.getStaticGrid().getMapSize();
    EXPECT_EQ(map_size, glm::ivec2(0, 0));

    const auto tile_size = spatial.getStaticGrid().getTileSize();
    EXPECT_GT(tile_size.x, 0);
    EXPECT_GT(tile_size.y, 0);

    EXPECT_FALSE(spatial.isSolidAt(glm::vec2(0.0f, 0.0f)));
    EXPECT_TRUE(spatial.queryColliderCandidates(engine::utils::Rect(glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 1.0f))).empty());
}

TEST(SpatialIndexManagerRaiiTest, InitializeMarksManagerReady) {
    entt::registry registry;
    SpatialIndexManager spatial;

    spatial.initialize(registry,
                       /*map_size*/ glm::ivec2(2, 2),
                       /*tile_size*/ glm::ivec2(16, 16),
                       /*world_bounds_min*/ glm::vec2(0.0f, 0.0f),
                       /*world_bounds_max*/ glm::vec2(32.0f, 32.0f),
                       /*dynamic_cell_size*/ glm::vec2(16.0f, 16.0f));

    EXPECT_TRUE(spatial.isInitialized());
}

TEST(SpatialIndexManagerRaiiTest, ResetIfUsingRegistryDetachesManager) {
    entt::registry registry_a;
    entt::registry registry_b;
    SpatialIndexManager spatial;

    spatial.initialize(registry_a,
                       /*map_size*/ glm::ivec2(2, 2),
                       /*tile_size*/ glm::ivec2(16, 16),
                       /*world_bounds_min*/ glm::vec2(0.0f, 0.0f),
                       /*world_bounds_max*/ glm::vec2(32.0f, 32.0f),
                       /*dynamic_cell_size*/ glm::vec2(16.0f, 16.0f));

    EXPECT_TRUE(spatial.isInitialized());

    spatial.resetIfUsingRegistry(&registry_b);
    EXPECT_TRUE(spatial.isInitialized());

    spatial.resetIfUsingRegistry(&registry_a);
    EXPECT_FALSE(spatial.isInitialized());
}

} // namespace engine::spatial
