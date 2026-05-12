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

TEST(GameSceneBattleAudioTest, PlaysGameplayAndBattleMusicThroughAudioCueCatalog) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/game_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("#include \"game/data/audio_cue_catalog.h\""), std::string::npos);
    EXPECT_NE(source.find("void GameScene::playGameplayMusicCue()"), std::string::npos);
    EXPECT_NE(source.find("void GameScene::playBattleMusicCue()"), std::string::npos);
    EXPECT_NE(source.find("void GameScene::playMusicCue(const game::data::MusicCueData& cue)"), std::string::npos);
    EXPECT_NE(source.find("defaultMusicCue(game::data::SceneAudioContext::Gameplay)"), std::string::npos);
    EXPECT_NE(source.find("defaultMusicCue(game::data::SceneAudioContext::Battle)"), std::string::npos);
    EXPECT_NE(source.find("cue.music_id_hash_"), std::string::npos);
    EXPECT_NE(source.find("cue.volume_scale_"), std::string::npos);

    const auto init_pos = source.find("playGameplayMusicCue();");
    const auto legacy_scene_music_pos = source.find("SCENE_BG_MUSIC_ID");
    ASSERT_NE(init_pos, std::string::npos);
    EXPECT_EQ(legacy_scene_music_pos, std::string::npos);

    const auto battle_music_pos = source.find("playBattleMusicCue();");
    const auto push_scene_pos = source.find("requestPushScene(std::make_unique<game::scene::BattleScene>(");
    ASSERT_NE(battle_music_pos, std::string::npos);
    ASSERT_NE(push_scene_pos, std::string::npos);
    EXPECT_LT(battle_music_pos, push_scene_pos);
}

TEST(GameSceneBattleAudioTest, RestoresGameplayMusicByBattleOutcome) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/game_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    const auto battle_ended_pos = source.find("void GameScene::onBattleEnded");
    ASSERT_NE(battle_ended_pos, std::string::npos);
    const std::string snippet = source.substr(battle_ended_pos, 2200U);

    EXPECT_NE(snippet.find("switch (evt.outcome)"), std::string::npos);
    EXPECT_NE(snippet.find("case game::battle::BattleOutcome::Victory:"), std::string::npos);
    EXPECT_NE(snippet.find("case game::battle::BattleOutcome::Escaped:"), std::string::npos);
    EXPECT_NE(snippet.find("case game::battle::BattleOutcome::Defeat:"), std::string::npos);
    EXPECT_NE(snippet.find("playGameplayMusicCue();"), std::string::npos);
    EXPECT_NE(snippet.find("GameOver / Defeat scene"), std::string::npos);
}

} // namespace
} // namespace game::scene
// NOLINTEND
