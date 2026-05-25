#include "game/runtime/user_settings.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

namespace gr = game::runtime;

namespace {

TEST(UserSettingsTest, DefaultsHaveExpectedValues) {
    gr::UserSettings s{};
    EXPECT_FLOAT_EQ(s.music_volume, 0.5f);
    EXPECT_FLOAT_EQ(s.sound_volume, 0.3f);
    EXPECT_FLOAT_EQ(s.global_time_scale, 1.0f);
    EXPECT_FLOAT_EQ(s.battle_animation_speed, 1.0f);
    EXPECT_TRUE(s.show_damage_popup);
    EXPECT_TRUE(s.show_enemy_hp_bar);
    EXPECT_TRUE(s.cursor_memory);
    EXPECT_EQ(s.ui_font_scale, gr::UiFontScale::Normal);
    EXPECT_EQ(s.language_tag, "en-US");
}

TEST(UserSettingsTest, RoundTripPreservesAllFields) {
    gr::UserSettings original{};
    original.music_volume = 0.7f;
    original.sound_volume = 0.2f;
    original.global_time_scale = 1.5f;
    original.battle_animation_speed = 2.0f;
    original.show_damage_popup = false;
    original.show_enemy_hp_bar = false;
    original.cursor_memory = false;
    original.ui_font_scale = gr::UiFontScale::Large;
    original.language_tag = "zh-Hans";

    const nlohmann::json json = gr::serializeUserSettings(original);

    gr::UserSettings parsed{};
    ASSERT_TRUE(gr::parseUserSettingsJson(json, parsed));

    EXPECT_FLOAT_EQ(parsed.music_volume, original.music_volume);
    EXPECT_FLOAT_EQ(parsed.sound_volume, original.sound_volume);
    EXPECT_FLOAT_EQ(parsed.global_time_scale, original.global_time_scale);
    EXPECT_FLOAT_EQ(parsed.battle_animation_speed, original.battle_animation_speed);
    EXPECT_FALSE(parsed.show_damage_popup);
    EXPECT_FALSE(parsed.show_enemy_hp_bar);
    EXPECT_FALSE(parsed.cursor_memory);
    EXPECT_EQ(parsed.ui_font_scale, gr::UiFontScale::Large);
    EXPECT_EQ(parsed.language_tag, "zh-Hans");
}

TEST(UserSettingsTest, MissingFieldsFallBackToDefaults) {
    const auto json = nlohmann::json::parse(R"({"audio": {"music_volume": 0.9}})");

    gr::UserSettings parsed{};
    ASSERT_TRUE(gr::parseUserSettingsJson(json, parsed));

    EXPECT_FLOAT_EQ(parsed.music_volume, 0.9f);
    EXPECT_FLOAT_EQ(parsed.sound_volume, 0.3f);                 // fallback
    EXPECT_FLOAT_EQ(parsed.global_time_scale, 1.0f);            // fallback
    EXPECT_FLOAT_EQ(parsed.battle_animation_speed, 1.0f);       // fallback
    EXPECT_TRUE(parsed.show_damage_popup);                      // fallback
    EXPECT_EQ(parsed.ui_font_scale, gr::UiFontScale::Normal);   // fallback
    EXPECT_EQ(parsed.language_tag, "en-US");                    // fallback
}

TEST(UserSettingsTest, OutOfRangeFieldsAreClamped) {
    auto json = nlohmann::json::parse(R"({
        "audio": {"music_volume": 2.5, "sound_volume": -0.5},
        "time":  {"global_scale": 0.0},
        "battle": {"animation_speed": 1.7}
    })");

    gr::UserSettings parsed{};
    ASSERT_TRUE(gr::parseUserSettingsJson(json, parsed));

    EXPECT_FLOAT_EQ(parsed.music_volume, 1.0f);
    EXPECT_FLOAT_EQ(parsed.sound_volume, 0.0f);
    EXPECT_FLOAT_EQ(parsed.global_time_scale, 1.0f);          // 非法 0 → reset
    EXPECT_FLOAT_EQ(parsed.battle_animation_speed, 1.5f);     // 收敛到最近 choice
}

TEST(UserSettingsTest, UnknownFontScaleFallsBackToNormal) {
    const auto json = nlohmann::json::parse(R"({"ui": {"font_scale": "huge"}})");
    gr::UserSettings parsed{};
    ASSERT_TRUE(gr::parseUserSettingsJson(json, parsed));
    EXPECT_EQ(parsed.ui_font_scale, gr::UiFontScale::Normal);
}

TEST(UserSettingsTest, NonObjectJsonIsRejected) {
    gr::UserSettings parsed{};
    EXPECT_FALSE(gr::parseUserSettingsJson(nlohmann::json::array(), parsed));
    EXPECT_FALSE(gr::parseUserSettingsJson(nlohmann::json{}, parsed)); // null
}

TEST(UserSettingsTest, ClampToNearestSpeedChoiceCoversAllCases) {
    EXPECT_FLOAT_EQ(gr::clampToNearestSpeedChoice(0.5f), 1.0f);
    EXPECT_FLOAT_EQ(gr::clampToNearestSpeedChoice(1.0f), 1.0f);
    EXPECT_FLOAT_EQ(gr::clampToNearestSpeedChoice(1.2f), 1.0f);
    EXPECT_FLOAT_EQ(gr::clampToNearestSpeedChoice(1.4f), 1.5f);
    EXPECT_FLOAT_EQ(gr::clampToNearestSpeedChoice(1.8f), 2.0f);
    EXPECT_FLOAT_EQ(gr::clampToNearestSpeedChoice(2.4f), 2.0f);
    EXPECT_FLOAT_EQ(gr::clampToNearestSpeedChoice(2.6f), 3.0f);
    EXPECT_FLOAT_EQ(gr::clampToNearestSpeedChoice(10.0f), 3.0f);
}

TEST(UserSettingsTest, FontScaleStringRoundTrip) {
    EXPECT_EQ(gr::uiFontScaleFromString("small"), gr::UiFontScale::Small);
    EXPECT_EQ(gr::uiFontScaleFromString("normal"), gr::UiFontScale::Normal);
    EXPECT_EQ(gr::uiFontScaleFromString("large"), gr::UiFontScale::Large);
    EXPECT_EQ(gr::uiFontScaleFromString("nonsense"), gr::UiFontScale::Normal);

    EXPECT_EQ(gr::uiFontScaleToString(gr::UiFontScale::Small), "small");
    EXPECT_EQ(gr::uiFontScaleToString(gr::UiFontScale::Normal), "normal");
    EXPECT_EQ(gr::uiFontScaleToString(gr::UiFontScale::Large), "large");
}

TEST(UserSettingsTest, FontScaleClassNamesMatchRcss) {
    EXPECT_EQ(gr::uiFontScaleClassName(gr::UiFontScale::Small), "tf-font-small");
    EXPECT_EQ(gr::uiFontScaleClassName(gr::UiFontScale::Normal), "tf-font-normal");
    EXPECT_EQ(gr::uiFontScaleClassName(gr::UiFontScale::Large), "tf-font-large");
}

} // namespace
