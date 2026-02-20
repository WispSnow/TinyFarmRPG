#include <gtest/gtest.h>

#include <algorithm>
#include <concepts>
#include <filesystem>
#include <string_view>

#include <entt/core/hashed_string.hpp>
#include <entt/signal/dispatcher.hpp>

#include "engine/resource/asset_registry.h"
#include "engine/resource/resource_manager.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace engine::resource {

template <typename T>
concept SupportsGetSoundByIdPath = requires(T& rm, entt::id_type id, std::string_view path) {
    rm.getSound(id, path);
};

template <typename T>
concept SupportsGetMusicByIdPath = requires(T& rm, entt::id_type id, std::string_view path) {
    rm.getMusic(id, path);
};

template <typename T>
concept SupportsGetTextureByIdPath = requires(T& rm, entt::id_type id, std::string_view path) {
    rm.getTexture(id, path);
};

template <typename T>
concept SupportsGetFontByIdSizePath = requires(T& rm, entt::id_type id, int font_size, std::string_view path) {
    rm.getFont(id, font_size, path);
};

TEST(ResourceManagerStrictPreloadApiTest, LegacyGetByIdPathOverloadsAreRemoved) {
    static_assert(!SupportsGetSoundByIdPath<ResourceManager>);
    static_assert(!SupportsGetMusicByIdPath<ResourceManager>);
    static_assert(!SupportsGetTextureByIdPath<ResourceManager>);
    static_assert(!SupportsGetFontByIdSizePath<ResourceManager>);
    SUCCEED();
}

TEST(ResourceManagerStrictPreloadApiTest, GetSoundDoesNotImplicitlyLoadRegisteredAsset) {
    entt::dispatcher dispatcher{};
    auto resource_manager = ResourceManager::create(&dispatcher);
    ASSERT_NE(resource_manager, nullptr);

    const std::filesystem::path sound_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "assets/audio/calf-and-cow.wav").lexically_normal();
    if (!std::filesystem::exists(sound_path)) {
        GTEST_SKIP() << "测试音频文件缺失: " << sound_path;
    }

    const std::string path_str = sound_path.string();
    const entt::id_type sound_id = entt::hashed_string{path_str.c_str(), path_str.size()}.value();

    auto& registry = resource_manager->getAssetRegistry();
    registry.registerSound(sound_id, path_str);

    // 严格预加载语义：只注册不预加载时，getSound 必须返回空句柄。
    EXPECT_FALSE(resource_manager->getSound(sound_id));

    const auto before_infos = resource_manager->getSoundDebugInfo();
    const auto before_it = std::find_if(before_infos.begin(), before_infos.end(), [sound_id](const AudioDebugInfo& info) {
        return info.id == sound_id;
    });
    EXPECT_EQ(before_it, before_infos.end());

    resource_manager->preloadRegisteredResources();
    EXPECT_TRUE(resource_manager->getSound(sound_id));
}

TEST(ResourceManagerStrictPreloadApiTest, GetTextureDoesNotImplicitlyLoadRegisteredAsset) {
    entt::dispatcher dispatcher{};
    auto resource_manager = ResourceManager::create(&dispatcher);
    ASSERT_NE(resource_manager, nullptr);

    constexpr std::string_view path_str = "tests/data/texture_missing_for_strict_preload_api_test.png";
    const entt::id_type texture_id = entt::hashed_string{path_str.data(), path_str.size()}.value();

    auto& registry = resource_manager->getAssetRegistry();
    registry.registerTexture(texture_id, path_str);

    EXPECT_FALSE(resource_manager->getTexture(texture_id));
    EXPECT_EQ(resource_manager->getTextureSize(texture_id), glm::vec2(0.0f, 0.0f));

    const auto before_infos = resource_manager->getTextureDebugInfo();
    const auto before_it = std::find_if(before_infos.begin(), before_infos.end(), [id = texture_id](const TextureDebugInfo& info) {
        return info.id == id;
    });
    EXPECT_EQ(before_it, before_infos.end());

    resource_manager->preloadRegisteredResources();
    EXPECT_FALSE(resource_manager->getTexture(texture_id));
}

TEST(ResourceManagerStrictPreloadApiTest, GetMusicDoesNotImplicitlyLoadRegisteredAsset) {
    entt::dispatcher dispatcher{};
    auto resource_manager = ResourceManager::create(&dispatcher);
    ASSERT_NE(resource_manager, nullptr);

    const std::filesystem::path music_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "assets/audio/01_spring_journey.ogg").lexically_normal();
    if (!std::filesystem::exists(music_path)) {
        GTEST_SKIP() << "测试音乐文件缺失: " << music_path;
    }

    const std::string path_str = music_path.string();
    const entt::id_type music_id = entt::hashed_string{path_str.c_str(), path_str.size()}.value();

    auto& registry = resource_manager->getAssetRegistry();
    registry.registerMusic(music_id, path_str);

    EXPECT_FALSE(resource_manager->getMusic(music_id));

    const auto before_infos = resource_manager->getMusicDebugInfo();
    const auto before_it = std::find_if(before_infos.begin(), before_infos.end(), [music_id](const AudioDebugInfo& info) {
        return info.id == music_id;
    });
    EXPECT_EQ(before_it, before_infos.end());

    resource_manager->preloadRegisteredResources();
    EXPECT_TRUE(resource_manager->getMusic(music_id));
}

TEST(ResourceManagerStrictPreloadApiTest, GetFontDoesNotImplicitlyLoadRegisteredAsset) {
    entt::dispatcher dispatcher{};
    auto resource_manager = ResourceManager::create(&dispatcher);
    ASSERT_NE(resource_manager, nullptr);

    const std::filesystem::path font_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "assets/fonts/VonwaonBitmap-16px.ttf").lexically_normal();
    if (!std::filesystem::exists(font_path)) {
        GTEST_SKIP() << "测试字体文件缺失: " << font_path;
    }

    const std::string path_str = font_path.string();
    const entt::id_type font_id = entt::hashed_string{path_str.c_str(), path_str.size()}.value();
    constexpr int kPixelSize = 16;

    auto& registry = resource_manager->getAssetRegistry();
    registry.registerFont(font_id, kPixelSize, path_str);

    EXPECT_EQ(resource_manager->getFont(font_id, kPixelSize), nullptr);

    resource_manager->preloadRegisteredResources();
    EXPECT_NE(resource_manager->getFont(font_id, kPixelSize), nullptr);
}

} // namespace engine::resource
