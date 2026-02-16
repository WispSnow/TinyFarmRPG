// NOLINTBEGIN
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace {

[[nodiscard]] std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    EXPECT_TRUE(file.is_open()) << path;
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

} // namespace

namespace engine::core {
namespace {

TEST(EcsSafetyTest, SceneCleanResetsRegistryContext) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/scene/scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("registry_ = entt::registry{}"), std::string::npos)
        << "Scene::clean should reset the registry so registry.ctx() does not retain stale values across clean/init cycles.";
}

TEST(EcsSafetyTest, WorldStateStoredAsPointerInRegistryContextOnly) {
    const std::filesystem::path factory_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/factory/entity_factory.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(factory_path)) << factory_path;
    const std::string factory_source = readTextFile(factory_path);
    ASSERT_FALSE(factory_source.empty());
    EXPECT_EQ(factory_source.find("find<game::world::WorldState>()"), std::string::npos)
        << "WorldState should be stored in registry.ctx() as a pointer only (WorldState*).";

    const std::filesystem::path builder_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/loader/entity_builder.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(builder_path)) << builder_path;
    const std::string builder_source = readTextFile(builder_path);
    ASSERT_FALSE(builder_source.empty());
    EXPECT_EQ(builder_source.find("find<game::world::WorldState>()"), std::string::npos)
        << "WorldState should be stored in registry.ctx() as a pointer only (WorldState*).";

    const std::filesystem::path daynight_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/system/day_night_system.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(daynight_path)) << daynight_path;
    const std::string daynight_source = readTextFile(daynight_path);
    ASSERT_FALSE(daynight_source.empty());
    EXPECT_EQ(daynight_source.find("find<game::world::WorldState>()"), std::string::npos)
        << "WorldState should be stored in registry.ctx() as a pointer only (WorldState*).";
}

TEST(EcsSafetyTest, SceneDebugPanelUsesRegistryAliveCount) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/debug/panels/scene_debug_panel.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("view.size()"), std::string::npos)
        << "SceneDebugPanel should use view.size() (O(1)) instead of counting entities via iterator distance (O(n)).";
}

TEST(EcsSafetyTest, SpatialIndexTagUsesCorrectSpelling) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/component/tags.h").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("SpatialIndexTag"), std::string::npos)
        << "Tags should expose SpatialIndexTag (correct spelling).";
    EXPECT_EQ(source.find("SpacialIndexTag"), std::string::npos)
        << "SpacialIndexTag is a misspelling and should not exist.";
}

TEST(EcsSafetyTest, RemoveEntitySystemAvoidsDestroyWhileIteratingView) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/system/remove_entity_system.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("to_destroy"), std::string::npos)
        << "RemoveEntitySystem should avoid destroying entities while iterating the view (collect first, destroy after).";
}

TEST(EcsSafetyTest, StateSystemAvoidsRemovingDirtyTagWhileIteratingView) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/system/state_system.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("dirty_entities"), std::string::npos)
        << "StateSystem::update should avoid removing StateDirtyTag while iterating the view (collect first, then process).";
}

} // namespace
} // namespace engine::core
// NOLINTEND
