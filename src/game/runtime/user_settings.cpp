#include "game/runtime/user_settings.h"

#include <nlohmann/json.hpp>

#include <string>
#include <string_view>

namespace game::runtime {

namespace {

template <typename T>
[[nodiscard]] T readJson(const nlohmann::json& obj, std::string_view key, T fallback) {
    const auto it = obj.find(std::string{key});
    if (it == obj.end()) {
        return fallback;
    }
    if constexpr (std::is_same_v<T, bool>) {
        if (it->is_boolean()) {
            return it->get<bool>();
        }
    } else if constexpr (std::is_same_v<T, float>) {
        if (it->is_number()) {
            return static_cast<float>(it->get<double>());
        }
    } else if constexpr (std::is_same_v<T, std::string>) {
        if (it->is_string()) {
            return it->get<std::string>();
        }
    }
    return fallback;
}

} // namespace

bool parseUserSettingsJson(const nlohmann::json& root, UserSettings& out) {
    if (!root.is_object()) {
        return false;
    }

    if (auto it = root.find("audio"); it != root.end() && it->is_object()) {
        out.music_volume = readJson<float>(*it, "music_volume", out.music_volume);
        out.sound_volume = readJson<float>(*it, "sound_volume", out.sound_volume);
    }
    if (auto it = root.find("time"); it != root.end() && it->is_object()) {
        out.global_time_scale = readJson<float>(*it, "global_scale", out.global_time_scale);
    }
    if (auto it = root.find("battle"); it != root.end() && it->is_object()) {
        out.battle_animation_speed = readJson<float>(*it, "animation_speed", out.battle_animation_speed);
        out.show_damage_popup = readJson<bool>(*it, "show_damage_popup", out.show_damage_popup);
        out.show_enemy_hp_bar = readJson<bool>(*it, "show_enemy_hp_bar", out.show_enemy_hp_bar);
        out.cursor_memory = readJson<bool>(*it, "cursor_memory", out.cursor_memory);
    }
    if (auto it = root.find("ui"); it != root.end() && it->is_object()) {
        const std::string scale_text = readJson<std::string>(
            *it, "font_scale", std::string{uiFontScaleToString(out.ui_font_scale)});
        out.ui_font_scale = uiFontScaleFromString(scale_text);
    }

    normalizeUserSettings(out);
    return true;
}

nlohmann::ordered_json serializeUserSettings(const UserSettings& s) {
    nlohmann::ordered_json out;
    out["audio"]["music_volume"] = s.music_volume;
    out["audio"]["sound_volume"] = s.sound_volume;
    out["time"]["global_scale"] = s.global_time_scale;
    out["battle"]["animation_speed"] = s.battle_animation_speed;
    out["battle"]["show_damage_popup"] = s.show_damage_popup;
    out["battle"]["show_enemy_hp_bar"] = s.show_enemy_hp_bar;
    out["battle"]["cursor_memory"] = s.cursor_memory;
    out["ui"]["font_scale"] = std::string{uiFontScaleToString(s.ui_font_scale)};
    return out;
}

} // namespace game::runtime
