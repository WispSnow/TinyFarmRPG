#include <gtest/gtest.h>

#include <algorithm>
#include <string_view>
#include <type_traits>
#include <utility>

#include <entt/core/hashed_string.hpp>
#include <entt/signal/dispatcher.hpp>

#include "engine/resource/resource_manager.h"

namespace engine::resource {

TEST(ResourceManagerAudioHandleApiTest, AudioApisReturnAudioBufferHandle) {
    using LoadSoundByIdReturn = decltype(std::declval<ResourceManager&>().loadSound(entt::id_type{}, std::string_view{}));
    using LoadSoundByHashReturn = decltype(std::declval<ResourceManager&>().loadSound(std::declval<entt::hashed_string>()));
    using GetSoundByIdReturn = decltype(std::declval<ResourceManager&>().getSound(entt::id_type{}, std::string_view{}));
    using GetSoundByHashReturn = decltype(std::declval<ResourceManager&>().getSound(std::declval<entt::hashed_string>()));

    using LoadMusicByIdReturn = decltype(std::declval<ResourceManager&>().loadMusic(entt::id_type{}, std::string_view{}));
    using LoadMusicByHashReturn = decltype(std::declval<ResourceManager&>().loadMusic(std::declval<entt::hashed_string>()));
    using GetMusicByIdReturn = decltype(std::declval<ResourceManager&>().getMusic(entt::id_type{}, std::string_view{}));
    using GetMusicByHashReturn = decltype(std::declval<ResourceManager&>().getMusic(std::declval<entt::hashed_string>()));

    static_assert(std::is_same_v<LoadSoundByIdReturn, AudioBufferHandle>);
    static_assert(std::is_same_v<LoadSoundByHashReturn, AudioBufferHandle>);
    static_assert(std::is_same_v<GetSoundByIdReturn, AudioBufferHandle>);
    static_assert(std::is_same_v<GetSoundByHashReturn, AudioBufferHandle>);

    static_assert(std::is_same_v<LoadMusicByIdReturn, AudioBufferHandle>);
    static_assert(std::is_same_v<LoadMusicByHashReturn, AudioBufferHandle>);
    static_assert(std::is_same_v<GetMusicByIdReturn, AudioBufferHandle>);
    static_assert(std::is_same_v<GetMusicByHashReturn, AudioBufferHandle>);
    SUCCEED();
}

TEST(ResourceManagerAudioHandleApiTest, FailedAudioLoadDoesNotLeaveDebugEntry) {
    entt::dispatcher dispatcher;
    auto resource_manager = ResourceManager::create(&dispatcher);
    ASSERT_NE(resource_manager, nullptr);

    constexpr std::string_view kMissingSoundPath = "tests/data/audio_missing_for_handle_api_test_sound.wav";
    constexpr std::string_view kMissingMusicPath = "tests/data/audio_missing_for_handle_api_test_music.ogg";
    const entt::id_type sound_id = entt::hashed_string{kMissingSoundPath.data(), kMissingSoundPath.size()};
    const entt::id_type music_id = entt::hashed_string{kMissingMusicPath.data(), kMissingMusicPath.size()};

    const AudioBufferHandle loaded_sound = resource_manager->loadSound(sound_id, kMissingSoundPath);
    const AudioBufferHandle loaded_music = resource_manager->loadMusic(music_id, kMissingMusicPath);
    EXPECT_FALSE(loaded_sound);
    EXPECT_FALSE(loaded_music);

    EXPECT_FALSE(resource_manager->getSound(sound_id));
    EXPECT_FALSE(resource_manager->getMusic(music_id));

    const auto sounds = resource_manager->getSoundDebugInfo();
    const auto music = resource_manager->getMusicDebugInfo();

    const auto sound_it = std::find_if(sounds.begin(), sounds.end(), [](const AudioDebugInfo& info) {
        return info.id == sound_id;
    });
    const auto music_it = std::find_if(music.begin(), music.end(), [](const AudioDebugInfo& info) {
        return info.id == music_id;
    });

    EXPECT_EQ(sound_it, sounds.end());
    EXPECT_EQ(music_it, music.end());
}

} // namespace engine::resource
