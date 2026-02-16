#include <gtest/gtest.h>

#include <entt/core/hashed_string.hpp>
#include <string>

#include "engine/ui/ui_preset_manager.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace engine::ui {

TEST(UIPresetManagerTest, ButtonPresetSoundEventsTriState) {
    UIPresetManager presets;

    const std::string path = std::string(PROJECT_SOURCE_DIR) + "/tests/data/ui_button_presets_sounds_test.json";
    ASSERT_TRUE(presets.loadButtonPresets(path));

    const entt::id_type test_id = entt::hashed_string{"test"}.value();
    const auto* test_preset = presets.getButtonPreset(test_id);
    ASSERT_NE(test_preset, nullptr);

    const entt::id_type hover_event = entt::hashed_string{"hover"}.value();
    const entt::id_type click_event = entt::hashed_string{"click"}.value();
    const entt::id_type drag_begin_event = entt::hashed_string{"drag_begin"}.value();

    ASSERT_TRUE(test_preset->sound_events.contains(hover_event));
    EXPECT_TRUE(test_preset->sound_events.at(hover_event).empty());

    ASSERT_TRUE(test_preset->sound_events.contains(click_event));
    EXPECT_EQ(test_preset->sound_events.at(click_event), "assets/audio/UI_button08.wav");

    ASSERT_TRUE(test_preset->sound_events.contains(drag_begin_event));
    EXPECT_EQ(test_preset->sound_events.at(drag_begin_event), "assets/audio/UI_button11.wav");

    const entt::id_type default_id = entt::hashed_string{"default"}.value();
    const auto* default_preset = presets.getButtonPreset(default_id);
    ASSERT_NE(default_preset, nullptr);
    EXPECT_TRUE(default_preset->sound_events.empty());

    const entt::id_type disabled_id = entt::hashed_string{"disable_by_empty"}.value();
    const auto* disabled_preset = presets.getButtonPreset(disabled_id);
    ASSERT_NE(disabled_preset, nullptr);

    ASSERT_TRUE(disabled_preset->sound_events.contains(click_event));
    EXPECT_TRUE(disabled_preset->sound_events.at(click_event).empty());
}

} // namespace engine::ui
