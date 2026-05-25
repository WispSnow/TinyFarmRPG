// NOLINTBEGIN
#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "../engine/render/test_source_utils.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace game::system {
namespace {

TEST(MapTransitionSystemWarpSourceTest, WarpCommandUsesTransitionPipelineAndMapEvents) {
    const std::filesystem::path header_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/system/map_transition_system.h").lexically_normal();
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/system/map_transition_system.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(header_path)) << header_path;
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string header = test_source_utils::readTextFile(header_path);
    const std::string source = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(header.empty());
    ASSERT_FALSE(source.empty());

    EXPECT_NE(header.find("Warp"), std::string::npos);
    EXPECT_NE(header.find("onWarpToMapCommand"), std::string::npos);
    EXPECT_NE(source.find("sink<game::defs::WarpToMapCommand>()"), std::string::npos);
    EXPECT_NE(source.find("void MapTransitionSystem::onWarpToMapCommand"), std::string::npos);
    EXPECT_NE(source.find("pending.type = TransitionType::Warp;"), std::string::npos);
    EXPECT_NE(source.find("beginTransition(pending);"), std::string::npos);

    const std::string commit_block =
        test_source_utils::extractFunctionBlock(source, "bool MapTransitionSystem::commitPendingTransition()");
    ASSERT_FALSE(commit_block.empty());
    EXPECT_NE(commit_block.find("pending_.type == TransitionType::Warp"), std::string::npos);
    EXPECT_NE(commit_block.find("pending_.warp_spawn_pos"), std::string::npos);
    EXPECT_NE(commit_block.find("emitMapTransitionEvents(previous_map_id, pending_.target_map_id);"),
              std::string::npos);
}

} // namespace
} // namespace game::system
// NOLINTEND
