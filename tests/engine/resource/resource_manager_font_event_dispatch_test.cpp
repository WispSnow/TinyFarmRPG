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

struct FontEventProbe {
    int unloaded_count{0};
    int cleared_count{0};
    entt::id_type last_unloaded_id{entt::null};
    int last_unloaded_size{0};

    void onFontUnloaded(const engine::utils::FontUnloadedEvent& event) {
        ++unloaded_count;
        last_unloaded_id = event.font_id;
        last_unloaded_size = event.font_size;
    }

    void onFontsCleared(const engine::utils::FontsClearedEvent&) {
        ++cleared_count;
    }
};

TEST(ResourceManagerFontEventDispatchTest, UnloadFontTriggersEventImmediatelyAndClearsCache) {
    entt::dispatcher dispatcher{};
    FontEventProbe probe{};
    dispatcher.sink<engine::utils::FontUnloadedEvent>().connect<&FontEventProbe::onFontUnloaded>(&probe);

    auto resource_manager = ResourceManager::create(&dispatcher);
    ASSERT_NE(resource_manager, nullptr);

    const std::filesystem::path font_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "assets/fonts/VonwaonBitmap-16px.ttf").lexically_normal();
    if (!std::filesystem::exists(font_path)) {
        dispatcher.sink<engine::utils::FontUnloadedEvent>().disconnect<&FontEventProbe::onFontUnloaded>(&probe);
        GTEST_SKIP() << "测试字体文件缺失: " << font_path;
    }

    const std::string path_str = font_path.string();
    const entt::id_type font_id = entt::hashed_string{path_str.c_str(), path_str.size()}.value();
    constexpr int kPixelSize = 16;

    ASSERT_NE(resource_manager->loadFont(font_id, kPixelSize, path_str), nullptr);
    ASSERT_NE(resource_manager->getFont(font_id, kPixelSize), nullptr);

    resource_manager->unloadFont(font_id, kPixelSize);

    // 不调用 dispatcher.update() 也应立即收到 trigger 分发的事件。
    EXPECT_EQ(probe.unloaded_count, 1);
    EXPECT_EQ(probe.last_unloaded_id, font_id);
    EXPECT_EQ(probe.last_unloaded_size, kPixelSize);
    EXPECT_EQ(resource_manager->getFont(font_id, kPixelSize), nullptr);

    dispatcher.sink<engine::utils::FontUnloadedEvent>().disconnect<&FontEventProbe::onFontUnloaded>(&probe);
}

TEST(ResourceManagerFontEventDispatchTest, ClearFontsTriggersEventImmediatelyAndClearsCache) {
    entt::dispatcher dispatcher{};
    FontEventProbe probe{};
    dispatcher.sink<engine::utils::FontsClearedEvent>().connect<&FontEventProbe::onFontsCleared>(&probe);

    auto resource_manager = ResourceManager::create(&dispatcher);
    ASSERT_NE(resource_manager, nullptr);

    const std::filesystem::path font_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "assets/fonts/VonwaonBitmap-16px.ttf").lexically_normal();
    if (!std::filesystem::exists(font_path)) {
        dispatcher.sink<engine::utils::FontsClearedEvent>().disconnect<&FontEventProbe::onFontsCleared>(&probe);
        GTEST_SKIP() << "测试字体文件缺失: " << font_path;
    }

    const std::string path_str = font_path.string();
    const entt::id_type font_id = entt::hashed_string{path_str.c_str(), path_str.size()}.value();
    constexpr int kPixelSize = 16;

    ASSERT_NE(resource_manager->loadFont(font_id, kPixelSize, path_str), nullptr);
    ASSERT_NE(resource_manager->getFont(font_id, kPixelSize), nullptr);

    resource_manager->clearFonts();

    // 不调用 dispatcher.update() 也应立即收到 trigger 分发的事件。
    EXPECT_EQ(probe.cleared_count, 1);
    EXPECT_EQ(resource_manager->getFont(font_id, kPixelSize), nullptr);

    dispatcher.sink<engine::utils::FontsClearedEvent>().disconnect<&FontEventProbe::onFontsCleared>(&probe);
}

} // namespace
} // namespace engine::resource
