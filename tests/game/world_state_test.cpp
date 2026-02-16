// NOLINTBEGIN
#include <gtest/gtest.h>

#include "game/world/world_state.h"

#include <filesystem>

namespace game::world {
namespace {

constexpr entt::id_type NULL_MAP_ID = entt::null;

TEST(WorldStateTest, NeighborsOfReturnsExpectedIds) {
    WorldState world_state{};

    const std::filesystem::path world_path = std::filesystem::path(PROJECT_SOURCE_DIR) / "assets/maps/farm-rpg.world";
    ASSERT_TRUE(world_state.loadFromWorldFile(world_path.string(), entt::null));

    const auto* home = world_state.getMapState("home_exterior");
    const auto* town = world_state.getMapState("town");
    ASSERT_NE(home, nullptr);
    ASSERT_NE(town, nullptr);

    const NeighborInfo home_neighbors = world_state.neighborsOf(home->info.id);
    EXPECT_EQ(home_neighbors.east, town->info.id);
    EXPECT_EQ(home_neighbors.west, NULL_MAP_ID);
    EXPECT_EQ(home_neighbors.north, NULL_MAP_ID);
    EXPECT_EQ(home_neighbors.south, NULL_MAP_ID);

    const NeighborInfo town_neighbors = world_state.neighborsOf(town->info.id);
    EXPECT_EQ(town_neighbors.west, home->info.id);
    EXPECT_EQ(town_neighbors.east, NULL_MAP_ID);
    EXPECT_EQ(town_neighbors.north, NULL_MAP_ID);
    EXPECT_EQ(town_neighbors.south, NULL_MAP_ID);
}

TEST(WorldStateTest, OutgoingTriggersIsEmptyByDefault) {
    WorldState world_state{};

    const std::filesystem::path world_path = std::filesystem::path(PROJECT_SOURCE_DIR) / "assets/maps/farm-rpg.world";
    ASSERT_TRUE(world_state.loadFromWorldFile(world_path.string(), entt::null));

    const auto* home = world_state.getMapState("home_exterior");
    ASSERT_NE(home, nullptr);

    const auto triggers = world_state.outgoingTriggers(home->info.id);
    EXPECT_TRUE(triggers.empty());
}

} // namespace
} // namespace game::world
// NOLINTEND
