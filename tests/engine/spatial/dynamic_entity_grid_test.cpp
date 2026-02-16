#include <gtest/gtest.h>

#include "engine/spatial/dynamic_entity_grid.h"

#include <algorithm>

namespace engine::spatial {
namespace {

[[nodiscard]] entt::entity makeEntity(std::uint32_t value) {
    return static_cast<entt::entity>(value);
}

[[nodiscard]] bool contains(const std::vector<entt::entity>& entities, entt::entity target) {
    return std::find(entities.begin(), entities.end(), target) != entities.end();
}

} // namespace

TEST(DynamicEntityGridTest, RectExactlyOnCellBoundaryStaysInSingleCell) {
    DynamicEntityGrid grid;
    grid.initialize(/*world_bounds_min*/ glm::vec2(0.0f, 0.0f),
                    /*world_bounds_max*/ glm::vec2(128.0f, 128.0f),
                    /*cell_size*/ glm::vec2(64.0f, 64.0f));

    const entt::entity entity = makeEntity(1);
    grid.addEntity(entity, engine::utils::Rect(glm::vec2(0.0f, 0.0f), glm::vec2(64.0f, 64.0f)));

    EXPECT_TRUE(contains(grid.queryEntitiesAt(glm::vec2(1.0f, 1.0f)), entity));
    EXPECT_FALSE(contains(grid.queryEntitiesAt(glm::vec2(64.0f, 1.0f)), entity));
}

TEST(DynamicEntityGridTest, QueryEntitiesAtOutsideWorldBoundsReturnsEmpty) {
    DynamicEntityGrid grid;
    grid.initialize(/*world_bounds_min*/ glm::vec2(0.0f, 0.0f),
                    /*world_bounds_max*/ glm::vec2(128.0f, 128.0f),
                    /*cell_size*/ glm::vec2(64.0f, 64.0f));

    const entt::entity entity = makeEntity(1);
    grid.addEntity(entity, engine::utils::Rect(glm::vec2(0.0f, 0.0f), glm::vec2(10.0f, 10.0f)));

    EXPECT_TRUE(grid.queryEntitiesAt(glm::vec2(-1.0f, 1.0f)).empty());
    EXPECT_TRUE(grid.queryEntitiesAt(glm::vec2(1.0f, -1.0f)).empty());
    EXPECT_TRUE(grid.queryEntitiesAt(glm::vec2(128.0f, 1.0f)).empty());
    EXPECT_TRUE(grid.queryEntitiesAt(glm::vec2(1.0f, 128.0f)).empty());
}

TEST(DynamicEntityGridTest, RectCrossingBoundaryRegistersInBothCells) {
    DynamicEntityGrid grid;
    grid.initialize(/*world_bounds_min*/ glm::vec2(0.0f, 0.0f),
                    /*world_bounds_max*/ glm::vec2(128.0f, 64.0f),
                    /*cell_size*/ glm::vec2(64.0f, 64.0f));

    const entt::entity entity = makeEntity(7);
    grid.addEntity(entity, engine::utils::Rect(glm::vec2(63.0f, 0.0f), glm::vec2(2.0f, 64.0f)));

    EXPECT_TRUE(contains(grid.queryEntitiesAt(glm::vec2(63.5f, 1.0f)), entity));
    EXPECT_TRUE(contains(grid.queryEntitiesAt(glm::vec2(64.0f, 1.0f)), entity));
}

TEST(DynamicEntityGridTest, UpdateEntityMovesBetweenCells) {
    DynamicEntityGrid grid;
    grid.initialize(/*world_bounds_min*/ glm::vec2(0.0f, 0.0f),
                    /*world_bounds_max*/ glm::vec2(128.0f, 64.0f),
                    /*cell_size*/ glm::vec2(64.0f, 64.0f));

    const entt::entity entity = makeEntity(3);
    grid.addEntity(entity, engine::utils::Rect(glm::vec2(0.0f, 0.0f), glm::vec2(10.0f, 10.0f)));

    EXPECT_TRUE(contains(grid.queryEntitiesAt(glm::vec2(1.0f, 1.0f)), entity));
    EXPECT_FALSE(contains(grid.queryEntitiesAt(glm::vec2(65.0f, 1.0f)), entity));

    grid.updateEntity(entity, engine::utils::Rect(glm::vec2(64.0f, 0.0f), glm::vec2(10.0f, 10.0f)));

    EXPECT_FALSE(contains(grid.queryEntitiesAt(glm::vec2(1.0f, 1.0f)), entity));
    EXPECT_TRUE(contains(grid.queryEntitiesAt(glm::vec2(65.0f, 1.0f)), entity));
}

TEST(DynamicEntityGridTest, QueryEntitiesDeduplicatesAcrossCells) {
    DynamicEntityGrid grid;
    grid.initialize(/*world_bounds_min*/ glm::vec2(0.0f, 0.0f),
                    /*world_bounds_max*/ glm::vec2(128.0f, 64.0f),
                    /*cell_size*/ glm::vec2(64.0f, 64.0f));

    const entt::entity entity = makeEntity(9);
    grid.addEntity(entity, engine::utils::Rect(glm::vec2(63.0f, 0.0f), glm::vec2(2.0f, 64.0f)));

    const auto results = grid.queryEntities(engine::utils::Rect(glm::vec2(0.0f, 0.0f), glm::vec2(128.0f, 64.0f)));
    EXPECT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0], entity);
}

} // namespace engine::spatial
