#include "ui_preset_manager.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <fstream>
#include <limits>
#include <utility>

namespace engine::ui {

namespace {

[[nodiscard]] float jsonToFloat(const nlohmann::json& value, float fallback) {
    if (const auto* v = value.get_ptr<const nlohmann::json::number_float_t*>()) {
        return static_cast<float>(*v);
    }
    if (const auto* v = value.get_ptr<const nlohmann::json::number_integer_t*>()) {
        return static_cast<float>(*v);
    }
    if (const auto* v = value.get_ptr<const nlohmann::json::number_unsigned_t*>()) {
        return static_cast<float>(*v);
    }
    return fallback;
}

[[nodiscard]] int jsonToInt(const nlohmann::json& value, int fallback) {
    if (const auto* v = value.get_ptr<const nlohmann::json::number_integer_t*>()) {
        return static_cast<int>(*v);
    }
    if (const auto* v = value.get_ptr<const nlohmann::json::number_unsigned_t*>()) {
        if (*v <= static_cast<nlohmann::json::number_unsigned_t>(std::numeric_limits<int>::max())) {
            return static_cast<int>(*v);
        }
    }
    return fallback;
}

[[nodiscard]] bool jsonToBool(const nlohmann::json& value, bool fallback) {
    if (const auto* v = value.get_ptr<const nlohmann::json::boolean_t*>()) {
        return *v;
    }
    if (const auto* v = value.get_ptr<const nlohmann::json::number_integer_t*>()) {
        return *v != 0;
    }
    if (const auto* v = value.get_ptr<const nlohmann::json::number_unsigned_t*>()) {
        return *v != 0;
    }
    return fallback;
}

[[nodiscard]] std::optional<std::string> jsonToString(const nlohmann::json& value) {
    if (const auto* v = value.get_ptr<const nlohmann::json::string_t*>()) {
        return std::string(*v);
    }
    return std::nullopt;
}

std::optional<engine::utils::FColor> parseColor(const nlohmann::json& value) {
    if (value.is_array() && value.size() == 4) {
        return engine::utils::FColor{
            jsonToFloat(value[0], 1.0f),
            jsonToFloat(value[1], 1.0f),
            jsonToFloat(value[2], 1.0f),
            jsonToFloat(value[3], 1.0f)
        };
    }
    if (value.is_object() && value.contains("r") && value.contains("g") && value.contains("b")) {
        const float a = value.contains("a") ? jsonToFloat(value["a"], 1.0f) : 1.0f;
        return engine::utils::FColor{
            jsonToFloat(value["r"], 1.0f),
            jsonToFloat(value["g"], 1.0f),
            jsonToFloat(value["b"], 1.0f),
            a
        };
    }
    return std::nullopt;
}

std::optional<glm::vec2> parseVec2(const nlohmann::json& value) {
    if (value.is_array() && value.size() == 2) {
        return glm::vec2{jsonToFloat(value[0], 0.0f), jsonToFloat(value[1], 0.0f)};
    }
    return std::nullopt;
}

} // namespace

bool UIPresetManager::loadButtonPresets(std::string_view file_path) {
    std::ifstream file{std::string(file_path)};
    if (!file.is_open()) {
        spdlog::warn("UIPresetManager: 无法打开按钮预设文件 {}", file_path);
        return false;
    }

    const std::string file_content(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

    const nlohmann::json root = nlohmann::json::parse(file_content, nullptr, false);
    if (root.is_discarded()) {
        spdlog::error("UIPresetManager: 解析按钮预设 JSON 失败: {}", file_path);
        return false;
    }

    if (!root.is_object()) {
        spdlog::warn("UIPresetManager: 预设文件格式无效 (根节点不是对象)。");
        return false;
    }

    button_presets_.clear();
    button_preset_keys_.clear();

    std::size_t loaded_count = 0;
    for (const auto& [key, value] : root.items()) {
        if (!value.is_object()) {
            spdlog::warn("UIPresetManager: 预设 '{}' 格式无效 (应为对象)。", key);
            continue;
        }

        UIButtonSkin skin{};
        if (value.contains("images")) {
            const auto& images = value["images"];
            if (images.is_object()) {
                if (images.contains("normal")) {
                    skin.normal_image = parseImageDefinition(images["normal"]);
                }
                if (images.contains("hover")) {
                    skin.hover_image = parseImageDefinition(images["hover"]);
                }
                if (images.contains("pressed")) {
                    skin.pressed_image = parseImageDefinition(images["pressed"]);
                }
                if (images.contains("disabled")) {
                    skin.disabled_image = parseImageDefinition(images["disabled"]);
                }
            }
        }

        if (!skin.normal_image) {
            spdlog::warn("UIPresetManager: 预设 '{}' 缺少 normal 图像定义，已跳过。", key);
            continue;
        }

        if (value.contains("nine_slice")) {
            skin.nine_slice_margins = parseNineSlice(value["nine_slice"]);
        }

        // nine_slice_margins 的传播由 registerButtonPreset 统一处理

        if (value.contains("label")) {
            skin.normal_label = parseLabelStyle(value["label"]);
        }

        if (value.contains("overrides")) {
            const auto& overrides = value["overrides"];
            if (overrides.is_object()) {
                if (overrides.contains("hover")) {
                    skin.hover_label = parseLabelOverrides(overrides["hover"]);
                }
                if (overrides.contains("pressed")) {
                    skin.pressed_label = parseLabelOverrides(overrides["pressed"]);
                }
                if (overrides.contains("disabled")) {
                    skin.disabled_label = parseLabelOverrides(overrides["disabled"]);
                }
            }
        }

        if (value.contains("sounds")) {
            const auto& sounds = value["sounds"];
            if (sounds.is_object()) {
                for (const auto& [sound_key, sound_value] : sounds.items()) {
                    const entt::id_type event_id = entt::hashed_string{sound_key.c_str(), sound_key.size()};
                    if (event_id == entt::null) {
                        continue;
                    }

                    if (sound_value.is_null()) {
                        skin.sound_events.insert_or_assign(event_id, std::string{});
                    } else if (sound_value.is_string()) {
                        skin.sound_events.insert_or_assign(event_id, sound_value.get<std::string>());
                    } else {
                        spdlog::warn(
                            "UIPresetManager: 预设 '{}' 的 sounds.{} 格式无效 (应为 string 或 null)。",
                            key,
                            sound_key
                        );
                    }
                }
            }
        }

        const entt::id_type preset_id = entt::hashed_string{key.c_str(), key.size()};
        button_preset_keys_.insert_or_assign(preset_id, key);
        registerButtonPreset(preset_id, std::move(skin));
        ++loaded_count;
    }

    spdlog::info("UIPresetManager: 已加载 {} 个按钮预设。", loaded_count);
    return loaded_count > 0;
}

void UIPresetManager::clearButtonPresets() {
    button_presets_.clear();
    button_preset_keys_.clear();
}

bool UIPresetManager::loadImagePresets(std::string_view file_path) {
    std::ifstream file{std::string(file_path)};
    if (!file.is_open()) {
        spdlog::warn("UIPresetManager: 无法打开图片预设文件 {}", file_path);
        return false;
    }

    const std::string file_content(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

    const nlohmann::json root = nlohmann::json::parse(file_content, nullptr, false);
    if (root.is_discarded()) {
        spdlog::error("UIPresetManager: 解析图片预设 JSON 失败: {}", file_path);
        return false;
    }

    if (!root.is_object()) {
        spdlog::warn("UIPresetManager: 图片预设文件格式无效 (根节点不是对象)。");
        return false;
    }

    image_presets_.clear();
    image_preset_keys_.clear();

    std::size_t loaded_count = 0;
    for (const auto& [key, value] : root.items()) {
        if (!value.is_object()) {
            spdlog::warn("UIPresetManager: 图片预设 '{}' 格式无效 (应为对象)。", key);
            continue;
        }

        auto image = parseImageDefinition(value);
        if (!image) {
            spdlog::warn("UIPresetManager: 图片预设 '{}' 缺少图像定义，已跳过。", key);
            continue;
        }

        if (value.contains("nine_slice")) {
            if (auto margins = parseNineSlice(value["nine_slice"])) {
                image->setNineSliceMargins(*margins);
            }
        }

        const entt::id_type preset_id = entt::hashed_string{key.c_str(), key.size()};
        image_preset_keys_.insert_or_assign(preset_id, key);
        registerImagePreset(preset_id, std::move(*image));
        ++loaded_count;
    }

    spdlog::info("UIPresetManager: 已加载 {} 个图片预设。", loaded_count);
    return loaded_count > 0;
}

void UIPresetManager::clearImagePresets() {
    image_presets_.clear();
    image_preset_keys_.clear();
}

const UIButtonSkin* UIPresetManager::getButtonPreset(entt::id_type preset_id) const {
    if (auto it = button_presets_.find(preset_id); it != button_presets_.end()) {
        return &it->second;
    }
    return nullptr;
}

const engine::render::Image* UIPresetManager::getImagePreset(entt::id_type preset_id) const {
    if (auto it = image_presets_.find(preset_id); it != image_presets_.end()) {
        return &it->second;
    }
    spdlog::warn("UIPresetManager: 未找到图片预设 {}", entt::to_integral(preset_id));
    return nullptr;
}

UIButtonSkin* UIPresetManager::getButtonPresetMutable(entt::id_type preset_id) {
    if (auto it = button_presets_.find(preset_id); it != button_presets_.end()) {
        return &it->second;
    }
    return nullptr;
}

engine::render::Image* UIPresetManager::getImagePresetMutable(entt::id_type preset_id) {
    if (auto it = image_presets_.find(preset_id); it != image_presets_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<entt::id_type> UIPresetManager::listButtonPresetIds() const {
    std::vector<entt::id_type> ids;
    ids.reserve(button_presets_.size());
    for (const auto& [preset_id, _] : button_presets_) {
        ids.push_back(preset_id);
    }
    std::ranges::sort(ids);
    return ids;
}

std::vector<entt::id_type> UIPresetManager::listImagePresetIds() const {
    std::vector<entt::id_type> ids;
    ids.reserve(image_presets_.size());
    for (const auto& [preset_id, _] : image_presets_) {
        ids.push_back(preset_id);
    }
    std::ranges::sort(ids);
    return ids;
}

std::string_view UIPresetManager::getButtonPresetKey(entt::id_type preset_id) const {
    if (auto it = button_preset_keys_.find(preset_id); it != button_preset_keys_.end()) {
        return it->second;
    }
    return {};
}

std::string_view UIPresetManager::getImagePresetKey(entt::id_type preset_id) const {
    if (auto it = image_preset_keys_.find(preset_id); it != image_preset_keys_.end()) {
        return it->second;
    }
    return {};
}

bool UIPresetManager::registerButtonPreset(entt::id_type preset_id, UIButtonSkin skin, bool overwrite) {
    if (preset_id == entt::null) {
        spdlog::warn("UIPresetManager: 忽略空预设 ID 注册请求。");
        return false;
    }

    if (!skin.normal_image) {
        spdlog::warn("UIPresetManager: 预设 {} 缺少 normal 图像，注册失败。", preset_id);
        return false;
    }

    if (!overwrite && button_presets_.contains(preset_id)) {
        return false;
    }

    if (skin.nine_slice_margins) {
        if (skin.normal_image) {
            skin.normal_image->setNineSliceMargins(skin.nine_slice_margins);
        }
        if (skin.hover_image) {
            skin.hover_image->setNineSliceMargins(skin.nine_slice_margins);
        }
        if (skin.pressed_image) {
            skin.pressed_image->setNineSliceMargins(skin.nine_slice_margins);
        }
        if (skin.disabled_image) {
            skin.disabled_image->setNineSliceMargins(skin.nine_slice_margins);
        }
    }

    button_presets_.insert_or_assign(preset_id, std::move(skin));
    return true;
}

bool UIPresetManager::registerImagePreset(entt::id_type preset_id, engine::render::Image image, bool overwrite) {
    if (preset_id == entt::null) {
        spdlog::warn("UIPresetManager: 忽略空图片预设 ID 注册请求。");
        return false;
    }

    if (image.getTextureId() == entt::null && image.getTexturePath().empty()) {
        spdlog::warn("UIPresetManager: 图片预设 {} 缺少有效纹理，注册失败。", preset_id);
        return false;
    }

    if (!overwrite && image_presets_.contains(preset_id)) {
        return false;
    }

    image_presets_.insert_or_assign(preset_id, std::move(image));
    return true;
}

std::optional<engine::render::Image> UIPresetManager::parseImageDefinition(const nlohmann::json& json_value) {
    if (!json_value.is_object()) {
        return std::nullopt;
    }

    std::optional<std::string> texture_path{};
    if (auto it = json_value.find("path"); it != json_value.end()) {
        texture_path = jsonToString(*it);
    }

    entt::id_type texture_id = entt::null;
    if (auto it = json_value.find("id"); it != json_value.end()) {
        if (auto id_string = jsonToString(*it); id_string) {
            texture_id = entt::hashed_string{id_string->c_str(), id_string->size()};
        }
    } else if (texture_path) {
        texture_id = entt::hashed_string{texture_path->c_str(), texture_path->size()};
    }

    engine::utils::Rect source_rect{};
    if (json_value.contains("source")) {
        const auto& src = json_value["source"];
        if (src.is_array() && src.size() == 4) {
            const float x = jsonToFloat(src[0], 0.0f);
            const float y = jsonToFloat(src[1], 0.0f);
            const float w = jsonToFloat(src[2], 0.0f);
            const float h = jsonToFloat(src[3], 0.0f);
            if (w > 0.0f && h > 0.0f) {
                source_rect = engine::utils::Rect{{x, y}, {w, h}};
            }
        } else if (src.is_object()) {
            const float x = (src.contains("x") ? jsonToFloat(src["x"], 0.0f) : 0.0f);
            const float y = (src.contains("y") ? jsonToFloat(src["y"], 0.0f) : 0.0f);
            const float w = (src.contains("w") ? jsonToFloat(src["w"], 0.0f) : 0.0f);
            const float h = (src.contains("h") ? jsonToFloat(src["h"], 0.0f) : 0.0f);
            if (w > 0.0f && h > 0.0f) {
                source_rect = engine::utils::Rect{{x, y}, {w, h}};
            }
        }
    }

    const bool flipped = json_value.contains("flipped") ? jsonToBool(json_value["flipped"], false) : false;

    if (texture_path) {
        if (texture_id != entt::null) {
            return engine::render::Image(*texture_path, texture_id, source_rect, flipped);
        }
        return engine::render::Image(*texture_path, source_rect, flipped);
    }

    if (texture_id != entt::null) {
        return engine::render::Image(texture_id, source_rect, flipped);
    }

    return std::nullopt;
}

std::optional<engine::render::NineSliceMargins> UIPresetManager::parseNineSlice(const nlohmann::json& json_value) {
    if (!json_value.is_object()) {
        return std::nullopt;
    }
    engine::render::NineSliceMargins margins{};
    if (auto it = json_value.find("left"); it != json_value.end()) {
        margins.left = jsonToFloat(*it, 0.0f);
    }
    if (auto it = json_value.find("top"); it != json_value.end()) {
        margins.top = jsonToFloat(*it, 0.0f);
    }
    if (auto it = json_value.find("right"); it != json_value.end()) {
        margins.right = jsonToFloat(*it, 0.0f);
    }
    if (auto it = json_value.find("bottom"); it != json_value.end()) {
        margins.bottom = jsonToFloat(*it, 0.0f);
    }
    return margins;
}

std::optional<UIButtonLabelStyle> UIPresetManager::parseLabelStyle(const nlohmann::json& json_value) {
    if (!json_value.is_object()) {
        return std::nullopt;
    }

    UIButtonLabelStyle style{};
    if (auto it = json_value.find("text"); it != json_value.end()) {
        if (auto text = jsonToString(*it); text) {
            style.text = std::move(*text);
        }
    }
    if (auto it = json_value.find("font_path"); it != json_value.end()) {
        if (auto font_path = jsonToString(*it); font_path) {
            style.font_path = std::move(*font_path);
        }
    }
    if (auto it = json_value.find("font_size"); it != json_value.end()) {
        style.font_size = jsonToInt(*it, 16);
    }
    if (json_value.contains("color")) {
        if (auto color = parseColor(json_value["color"])) {
            style.color = *color;
        }
    }
    if (json_value.contains("offset")) {
        if (auto offset = parseVec2(json_value["offset"])) {
            style.offset = *offset;
        }
    }
    return style;
}

std::optional<UIButtonLabelOverrides> UIPresetManager::parseLabelOverrides(const nlohmann::json& json_value) {
    if (!json_value.is_object()) {
        return std::nullopt;
    }

    UIButtonLabelOverrides overrides{};
    bool has_override = false;
    if (json_value.contains("color")) {
        if (auto color = parseColor(json_value["color"])) {
            overrides.color = *color;
            has_override = true;
        }
    }
    if (json_value.contains("offset")) {
        if (auto offset = parseVec2(json_value["offset"])) {
            overrides.offset = *offset;
            has_override = true;
        }
    }
    if (!has_override) {
        return std::nullopt;
    }
    return overrides;
}

} // namespace engine::ui
