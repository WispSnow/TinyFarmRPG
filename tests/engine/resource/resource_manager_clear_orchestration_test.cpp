#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include <entt/core/hashed_string.hpp>
#include <entt/signal/dispatcher.hpp>

#include "engine/resource/resource_manager.h"
#include "engine/utils/events.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace engine::resource {
namespace {

struct FontClearProbe {
    int cleared_count{0};
    void onFontsCleared(const engine::utils::FontsClearedEvent&) {
        ++cleared_count;
    }
};

TEST(ResourceManagerClearOrchestrationTest, ClearRemovesAllCachesAndDispatchesFontClearImmediately) {
    entt::dispatcher dispatcher{};
    FontClearProbe probe{};
    dispatcher.sink<engine::utils::FontsClearedEvent>().connect<&FontClearProbe::onFontsCleared>(&probe);

    auto resource_manager = ResourceManager::create(&dispatcher);
    ASSERT_NE(resource_manager, nullptr);

    const std::filesystem::path sound_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "assets/audio/calf-and-cow.wav").lexically_normal();
    const std::filesystem::path music_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "assets/audio/01_spring_journey.ogg").lexically_normal();
    const std::filesystem::path font_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "assets/fonts/VonwaonBitmap-16px.ttf").lexically_normal();

    if (!std::filesystem::exists(sound_path) || !std::filesystem::exists(music_path) || !std::filesystem::exists(font_path)) {
        dispatcher.sink<engine::utils::FontsClearedEvent>().disconnect<&FontClearProbe::onFontsCleared>(&probe);
        GTEST_SKIP() << "测试资源文件缺失，无法验证 clear() 编排行为。";
    }

    const std::string sound_path_str = sound_path.string();
    const std::string music_path_str = music_path.string();
    const std::string font_path_str = font_path.string();

    const entt::id_type sound_id = entt::hashed_string{sound_path_str.c_str(), sound_path_str.size()}.value();
    const entt::id_type music_id = entt::hashed_string{music_path_str.c_str(), music_path_str.size()}.value();
    const entt::id_type font_id = entt::hashed_string{font_path_str.c_str(), font_path_str.size()}.value();
    constexpr int kFontPixelSize = 16;

    ASSERT_TRUE(resource_manager->loadSound(sound_id, sound_path_str));
    ASSERT_TRUE(resource_manager->loadMusic(music_id, music_path_str));
    ASSERT_NE(resource_manager->loadFont(font_id, kFontPixelSize, font_path_str), nullptr);

    ASSERT_TRUE(resource_manager->getSound(sound_id));
    ASSERT_TRUE(resource_manager->getMusic(music_id));
    ASSERT_NE(resource_manager->getFont(font_id, kFontPixelSize), nullptr);

    resource_manager->clear();

    // clear() 内部走 clearFonts()，因此无需 dispatcher.update() 即可收到 trigger 事件。
    EXPECT_EQ(probe.cleared_count, 1);
    EXPECT_FALSE(resource_manager->getSound(sound_id));
    EXPECT_FALSE(resource_manager->getMusic(music_id));
    EXPECT_EQ(resource_manager->getFont(font_id, kFontPixelSize), nullptr);

    dispatcher.sink<engine::utils::FontsClearedEvent>().disconnect<&FontClearProbe::onFontsCleared>(&probe);
}

} // namespace
} // namespace engine::resource
