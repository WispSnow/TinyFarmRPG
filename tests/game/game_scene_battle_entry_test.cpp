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

namespace game::scene {
namespace {

TEST(GameSceneBattleEntryTest, ConnectsAndDisconnectsBattleDispatcherHooks) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/game_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("sink<game::defs::EnterBattleCommand>().connect<&GameScene::onEnterBattleCommand>(this);"),
              std::string::npos);
    EXPECT_NE(source.find("sink<game::defs::EnterBattleCommand>().disconnect<&GameScene::onEnterBattleCommand>(this);"),
              std::string::npos);
    EXPECT_NE(source.find("sink<game::defs::BattleEndedEvent>().connect<&GameScene::onBattleEnded>(this);"),
              std::string::npos);
    EXPECT_NE(source.find("sink<game::defs::BattleEndedEvent>().disconnect<&GameScene::onBattleEnded>(this);"),
              std::string::npos);
}

TEST(GameSceneBattleEntryTest, PushesBattleSceneOnEnterBattleCommand) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/game_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("requestPushScene(std::make_unique<game::scene::BattleScene>(\"BattleScene\""),
              std::string::npos);
}

TEST(GameSceneBattleEntryTest, SupportsCatalogFallbackWhenCommandUnitsAreEmpty) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/game_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("if (units.empty())"), std::string::npos);
    EXPECT_NE(source.find("buildBattleUnitsFromCatalog"), std::string::npos);
    EXPECT_NE(source.find("services_->rpg_catalog"), std::string::npos);
}

TEST(GameSceneBattleEntryTest, SupportsTroopAndActorSelectionInBattleCommand) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/game_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("cmd.troop_id"), std::string::npos);
    EXPECT_NE(source.find("cmd.actor_ids"), std::string::npos);
}

} // namespace
} // namespace game::scene
// NOLINTEND
