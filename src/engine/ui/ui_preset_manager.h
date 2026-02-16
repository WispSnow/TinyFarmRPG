#pragma once

#include "ui_button.h"
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <nlohmann/json_fwd.hpp>

namespace engine::ui {

class UIPresetManager final {
    std::unordered_map<entt::id_type, UIButtonSkin> button_presets_{};
    std::unordered_map<entt::id_type, engine::render::Image> image_presets_{};
    std::unordered_map<entt::id_type, std::string> button_preset_keys_{};
    std::unordered_map<entt::id_type, std::string> image_preset_keys_{};

    static std::optional<engine::render::Image> parseImageDefinition(const nlohmann::json& json_value);
    static std::optional<engine::render::NineSliceMargins> parseNineSlice(const nlohmann::json& json_value);
    static std::optional<UIButtonLabelStyle> parseLabelStyle(const nlohmann::json& json_value);
    static std::optional<UIButtonLabelOverrides> parseLabelOverrides(const nlohmann::json& json_value);

public:
    UIPresetManager() = default;

    bool loadButtonPresets(std::string_view file_path);
    bool loadImagePresets(std::string_view file_path);
    void clearButtonPresets();
    void clearImagePresets();

    [[nodiscard]] const UIButtonSkin* getButtonPreset(entt::id_type preset_id) const;
    [[nodiscard]] const engine::render::Image* getImagePreset(entt::id_type preset_id) const;
    [[nodiscard]] UIButtonSkin* getButtonPresetMutable(entt::id_type preset_id);
    [[nodiscard]] engine::render::Image* getImagePresetMutable(entt::id_type preset_id);
    [[nodiscard]] std::vector<entt::id_type> listButtonPresetIds() const;
    [[nodiscard]] std::vector<entt::id_type> listImagePresetIds() const;
    [[nodiscard]] std::string_view getButtonPresetKey(entt::id_type preset_id) const;
    [[nodiscard]] std::string_view getImagePresetKey(entt::id_type preset_id) const;
    bool registerButtonPreset(entt::id_type preset_id, UIButtonSkin skin, bool overwrite = true);
    bool registerImagePreset(entt::id_type preset_id, engine::render::Image image, bool overwrite = true);
};

} // namespace engine::ui
