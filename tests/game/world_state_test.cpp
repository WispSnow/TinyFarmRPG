// NOLINTBEGIN
#include <gtest/gtest.h>

#include "game/world/world_state.h"

#include <filesystem>
#include <fstream>

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

TEST(WorldStateTest, FailedReloadKeepsExistingWorldData) {
    WorldState world_state{};

    const std::filesystem::path world_path = std::filesystem::path(PROJECT_SOURCE_DIR) / "assets/maps/farm-rpg.world";
    ASSERT_TRUE(world_state.loadFromWorldFile(world_path.string(), entt::null));
    ASSERT_NE(world_state.getMapState("home_exterior"), nullptr);

    const auto invalid_path = std::filesystem::temp_directory_path() / "tinyfarm_invalid_world_state.world";
    {
        std::ofstream file(invalid_path);
        ASSERT_TRUE(file.is_open()) << invalid_path;
        file << R"({"maps": )";
    }

    EXPECT_FALSE(world_state.loadFromWorldFile(invalid_path.string(), entt::null));
    EXPECT_NE(world_state.getMapState("home_exterior"), nullptr);
    EXPECT_FALSE(world_state.maps().empty());

    std::error_code ec;
    std::filesystem::remove(invalid_path, ec);
}

} // namespace
} // namespace game::world
// NOLINTEND
