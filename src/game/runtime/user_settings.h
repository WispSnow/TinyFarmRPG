#pragma once

#include <nlohmann/json_fwd.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>

namespace game::runtime {

/// @brief UI 字号档位。
enum class UiFontScale : std::uint8_t {
    Small = 0,
    Normal = 1,
    Large = 2,
};

/// @brief 玩家偏好设置 POD。
///
/// 所有字段都是裸值；clamp / 持久化 / apply 由 UserSettingsService 负责。
struct UserSettings {
    // Audio
    float music_volume{0.5f};         ///< [0, 1]
    float sound_volume{0.3f};         ///< [0, 1]

    // Time
    float global_time_scale{1.0f};    ///< (0, 100]，PauseMenu 调节的全局倍速

    // Battle
    float battle_animation_speed{1.0f}; ///< 取值集合：1.0/1.5/2.0/3.0
    bool show_damage_popup{true};
    bool show_enemy_hp_bar{true};
    bool cursor_memory{true};

    // UI
    UiFontScale ui_font_scale{UiFontScale::Normal};
    std::string language_tag{"en-US"};
};

/// @brief 战斗动画速度可选档位。
inline constexpr std::array<float, 4> BATTLE_ANIMATION_SPEED_CHOICES{1.0f, 1.5f, 2.0f, 3.0f};

/// @brief 时间倍速允许范围（与现有 PauseMenu clamp 一致）。
inline constexpr float GLOBAL_TIME_SCALE_MIN{0.01f};
inline constexpr float GLOBAL_TIME_SCALE_MAX{100.0f};

/// @brief 把任意 float 收敛到最接近的合法档位。
[[nodiscard]] inline float clampToNearestSpeedChoice(float value) noexcept {
    float best = BATTLE_ANIMATION_SPEED_CHOICES.front();
    float best_distance = std::fabs(value - best);
    for (const float choice : BATTLE_ANIMATION_SPEED_CHOICES) {
        const float distance = std::fabs(value - choice);
        if (distance < best_distance) {
            best = choice;
            best_distance = distance;
        }
    }
    return best;
}

/// @brief 对 UserSettings 中所有字段执行范围 clamp（修复越界输入）。
inline void normalizeUserSettings(UserSettings& s) noexcept {
    s.music_volume = std::clamp(s.music_volume, 0.0f, 1.0f);
    s.sound_volume = std::clamp(s.sound_volume, 0.0f, 1.0f);
    if (!std::isfinite(s.global_time_scale) || s.global_time_scale <= 0.0f) {
        s.global_time_scale = 1.0f;
    }
    s.global_time_scale = std::clamp(s.global_time_scale, GLOBAL_TIME_SCALE_MIN, GLOBAL_TIME_SCALE_MAX);
    s.battle_animation_speed = clampToNearestSpeedChoice(s.battle_animation_speed);
    if (static_cast<std::uint8_t>(s.ui_font_scale) > static_cast<std::uint8_t>(UiFontScale::Large)) {
        s.ui_font_scale = UiFontScale::Normal;
    }
}

/// @brief UiFontScale 与字符串互转。
[[nodiscard]] inline std::string_view uiFontScaleToString(UiFontScale scale) noexcept {
    switch (scale) {
        case UiFontScale::Small: return "small";
        case UiFontScale::Large: return "large";
        case UiFontScale::Normal:
        default:                 return "normal";
    }
}

[[nodiscard]] inline UiFontScale uiFontScaleFromString(std::string_view text) noexcept {
    if (text == "small") return UiFontScale::Small;
    if (text == "large") return UiFontScale::Large;
    return UiFontScale::Normal;
}

/// @brief body 上挂的 css class 名（用于 Phase 4 字号联动）。
[[nodiscard]] inline std::string_view uiFontScaleClassName(UiFontScale scale) noexcept {
    switch (scale) {
        case UiFontScale::Small: return "tf-font-small";
        case UiFontScale::Large: return "tf-font-large";
        case UiFontScale::Normal:
        default:                 return "tf-font-normal";
    }
}

/// @brief 解析 JSON 到 UserSettings；非法字段走 fallback；末尾自动 normalize。
/// @return true 表示输入是合法的 JSON object（即使部分字段缺失或非法）。
[[nodiscard]] bool parseUserSettingsJson(const nlohmann::json& root, UserSettings& out);

/// @brief 序列化为 JSON ordered object。
[[nodiscard]] nlohmann::ordered_json serializeUserSettings(const UserSettings& s);

} // namespace game::runtime
