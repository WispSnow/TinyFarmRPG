#include <gtest/gtest.h>

#include <entt/core/hashed_string.hpp>
#include <entt/signal/dispatcher.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <glm/vec2.hpp>
#include <memory>
#include <string>
#include <string_view>

#include "engine/audio/audio_player.h"
#include "engine/resource/resource_manager.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace engine::audio {

namespace {

[[nodiscard]] entt::id_type makeResourceId(const std::string& path) {
    return entt::hashed_string{path.c_str()}.value();
}

} // namespace

class AudioPlayerTest : public ::testing::Test {
protected:
    AudioPlayerTest()
        : project_root_(std::filesystem::path{PROJECT_SOURCE_DIR}.lexically_normal()) {}

    void SetUp() override {
        resource_manager_ = engine::resource::ResourceManager::create(&dispatcher_);
        audio_player_ = AudioPlayer::create(resource_manager_.get());
        if (!resource_manager_ || !audio_player_) {
            resource_manager_.reset();
            audio_player_.reset();
            GTEST_SKIP() << "跳过 AudioPlayer 测试：初始化失败";
        }
    }

    void TearDown() override {
        if (resource_manager_) {
            resource_manager_->clear();
        }
        audio_player_.reset();
        resource_manager_.reset();
    }

    [[nodiscard]] std::filesystem::path resolveAsset(std::string_view relative) const {
        return (project_root_ / relative).lexically_normal();
    }

    entt::dispatcher dispatcher_{};
    std::unique_ptr<engine::resource::ResourceManager> resource_manager_{};
    std::unique_ptr<AudioPlayer> audio_player_{};
    std::filesystem::path project_root_;
};

TEST(AudioPlayerStandaloneTest, ConstructionFailsWithNullResourceManager) {
    EXPECT_EQ(AudioPlayer::create(nullptr), nullptr);
}

TEST_F(AudioPlayerTest, PlaySoundWithoutPathReturnsError) {
    ASSERT_NE(audio_player_, nullptr);

    constexpr entt::id_type missing_id = 0xDEADBEEF;
    const bool result = audio_player_->playSound(missing_id);

    EXPECT_FALSE(result);
}

TEST_F(AudioPlayerTest, PlaySoundWithPathReturnsSuccess) {
    ASSERT_NE(audio_player_, nullptr);

    const std::filesystem::path sound_path = resolveAsset("assets/audio/level-win.mp3");
    if (!std::filesystem::exists(sound_path)) {
        GTEST_SKIP() << "测试音频文件缺失: " << sound_path;
    }

    const std::string sound_path_str = sound_path.string();
    const bool result = audio_player_->playSound(makeResourceId(sound_path_str), sound_path_str);

    EXPECT_TRUE(result);
}

TEST_F(AudioPlayerTest, PlaySound2DWithPositionsReturnsSuccess) {
    ASSERT_NE(audio_player_, nullptr);

    const std::filesystem::path sound_path = resolveAsset("assets/audio/Bow Impact Hit 1.ogg");
    if (!std::filesystem::exists(sound_path)) {
        GTEST_SKIP() << "测试音频文件缺失: " << sound_path;
    }

    const std::string sound_path_str = sound_path.string();
    const glm::vec2 source{150.0F, 25.0F};
    const glm::vec2 listener{0.0F, 0.0F};

    const bool result = audio_player_->playSound2D(makeResourceId(sound_path_str), source, listener, sound_path_str);

    EXPECT_TRUE(result);
}

TEST_F(AudioPlayerTest, PlayMusicCanStopAndReplay) {
    ASSERT_NE(audio_player_, nullptr);

    const std::filesystem::path music_path = resolveAsset("assets/audio/4 Battle Track INTRO TomMusic.ogg");
    if (!std::filesystem::exists(music_path)) {
        GTEST_SKIP() << "测试音乐文件缺失: " << music_path;
    }

    const std::string music_path_str = music_path.string();
    const entt::id_type music_id = makeResourceId(music_path_str);

    ASSERT_TRUE(audio_player_->playMusic(music_id, true, 0, music_path_str));

    audio_player_->stopMusic();

    EXPECT_TRUE(audio_player_->playMusic(music_id, false, 0, music_path_str));
}

TEST_F(AudioPlayerTest, SoundVolumeClampsIntoRange) {
    ASSERT_NE(audio_player_, nullptr);

    audio_player_->setSoundVolume(1.5F);
    EXPECT_FLOAT_EQ(audio_player_->getSoundVolume(), 1.0F);

    audio_player_->setSoundVolume(-0.25F);
    EXPECT_FLOAT_EQ(audio_player_->getSoundVolume(), 0.0F);

    constexpr float expected_volume = 0.65F;
    audio_player_->setSoundVolume(expected_volume);
    EXPECT_FLOAT_EQ(audio_player_->getSoundVolume(), expected_volume);
}

TEST_F(AudioPlayerTest, MusicVolumeClampsIntoRange) {
    ASSERT_NE(audio_player_, nullptr);

    audio_player_->setMusicVolume(2.0F);
    EXPECT_FLOAT_EQ(audio_player_->getMusicVolume(), 1.0F);

    audio_player_->setMusicVolume(-1.0F);
    EXPECT_FLOAT_EQ(audio_player_->getMusicVolume(), 0.0F);

    constexpr float expected_volume = 0.4F;
    audio_player_->setMusicVolume(expected_volume);
    EXPECT_FLOAT_EQ(audio_player_->getMusicVolume(), expected_volume);
}

TEST_F(AudioPlayerTest, LoadConfigReadsSpatialSettings) {
    ASSERT_NE(audio_player_, nullptr);

    const auto timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const std::filesystem::path config_path =
        std::filesystem::temp_directory_path() / ("audio_player_config_test_" + std::to_string(timestamp) + ".json");

    std::ofstream config_file(config_path);
    ASSERT_TRUE(config_file.is_open());
    config_file << R"({"audio":{"music_volume":0.12,"sound_volume":0.34,"spatial":{"falloff_distance":123.0,"pan_range":456.0}}})";
    config_file.close();

    const bool loaded = audio_player_->loadConfig(config_path.string());
    std::error_code error_code;
    std::filesystem::remove(config_path, error_code);

    ASSERT_TRUE(loaded);
    EXPECT_FLOAT_EQ(audio_player_->getMusicVolume(), 0.12F);
    EXPECT_FLOAT_EQ(audio_player_->getSoundVolume(), 0.34F);
    EXPECT_FLOAT_EQ(audio_player_->getSpatialFalloffDistance(), 123.0F);
    EXPECT_FLOAT_EQ(audio_player_->getSpatialPanRange(), 456.0F);
}

} // namespace engine::audio
