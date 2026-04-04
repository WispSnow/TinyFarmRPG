#include "config.h"
#include <fstream>
#include <filesystem>
#include <limits>
#include <nlohmann/json.hpp>
#include "spdlog/spdlog.h"

namespace engine::core {

namespace {

constexpr engine::ui::rmlui::RmlUiTextureFilterMode kDefaultRmlUiTextureFilterMode =
    engine::ui::rmlui::RmlUiTextureFilterMode::Nearest;

} // namespace

std::unique_ptr<Config> Config::create(std::string_view filepath) {
    std::unique_ptr<Config> config(new Config(filepath));
    if (!config) {
        spdlog::error("Config::create: 创建 Config 失败。");
        return nullptr;
    }
    return config;
}

Config::Config(std::string_view filepath)
{
    loadFromFile(filepath);
}

bool Config::loadFromFile(std::string_view filepath) {
    auto path = std::filesystem::path(filepath);    // 将string_view转换为文件路径 (或std::sring)
    std::ifstream file(path);                       // ifstream 不支持std::string_view 构造
    if (!file.is_open()) {
        spdlog::warn("配置文件 '{}' 未找到。使用默认设置并创建默认配置文件。", filepath);
        if (!saveToFile(filepath)) {
            spdlog::error("无法创建默认配置文件 '{}'。", filepath);
            return false;
        }
        return false; // 文件不存在，使用默认值
    }

    const std::string file_content(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

    const nlohmann::json j = nlohmann::json::parse(file_content, nullptr, false);
    if (j.is_discarded()) {
        spdlog::error("读取配置文件 '{}' 时出错：JSON 解析失败。使用默认设置。", filepath);
        return false;
    }

    fromJson(j);
    spdlog::info("成功从 '{}' 加载配置。", filepath);
    return true;
}

bool Config::saveToFile(std::string_view filepath) {
    auto path = std::filesystem::path(filepath);    // 将string_view转换为文件路径
    std::ofstream file(path);
    if (!file.is_open()) {
        spdlog::error("无法打开配置文件 '{}' 进行写入。", filepath);
        return false;
    }

    nlohmann::ordered_json j = toJson();
    file << j.dump(4);
    if (!file.good()) {
        spdlog::error("写入配置文件 '{}' 时出错。", filepath);
        return false;
    }

    spdlog::info("成功将配置保存到 '{}'。", filepath);
    return true;
}

void Config::fromJson(const nlohmann::json& j) {
    if (!j.is_object()) {
        return;
    }

    auto assignInt = [](const nlohmann::json& obj, std::string_view key, int& target) {
        const auto it = obj.find(key);  // nlohmann::json supports string_view lookup
        if (it == obj.end()) {
            return;
        }

        if (const auto* v = it->get_ptr<const nlohmann::json::number_integer_t*>()) {
            if (*v < std::numeric_limits<int>::min() || *v > std::numeric_limits<int>::max()) {
                return;
            }
            target = static_cast<int>(*v);
            return;
        }

        if (const auto* v = it->get_ptr<const nlohmann::json::number_unsigned_t*>()) {
            const auto max_int = static_cast<nlohmann::json::number_unsigned_t>(std::numeric_limits<int>::max());
            if (*v > max_int) {
                return;
            }
            target = static_cast<int>(*v);
        }
    };

    auto assignFloat = [](const nlohmann::json& obj, std::string_view key, float& target) {
        const auto it = obj.find(key);  // nlohmann::json supports string_view lookup
        if (it == obj.end()) {
            return;
        }

        if (const auto* v = it->get_ptr<const nlohmann::json::number_float_t*>()) {
            target = static_cast<float>(*v);
            return;
        }

        if (const auto* v = it->get_ptr<const nlohmann::json::number_integer_t*>()) {
            target = static_cast<float>(*v);
            return;
        }

        if (const auto* v = it->get_ptr<const nlohmann::json::number_unsigned_t*>()) {
            target = static_cast<float>(*v);
        }
    };

    auto assignBool = [](const nlohmann::json& obj, std::string_view key, bool& target) {
        const auto it = obj.find(key);  // nlohmann::json supports string_view lookup
        if (it == obj.end()) {
            return;
        }
        if (const auto* v = it->get_ptr<const nlohmann::json::boolean_t*>()) {
            target = *v;
        }
    };

    auto assignString = [](const nlohmann::json& obj, std::string_view key, std::string& target) {
        const auto it = obj.find(key);  // nlohmann::json supports string_view lookup
        if (it == obj.end()) {
            return;
        }
        if (const auto* v = it->get_ptr<const nlohmann::json::string_t*>()) {
            target = *v;
        }
    };

    if (const auto it = j.find("window"); it != j.end() && it->is_object()) {
        const auto& window_config = *it;
        assignString(window_config, "title", window_title_);
        assignInt(window_config, "width", window_width_);
        assignInt(window_config, "height", window_height_);
        assignFloat(window_config, "window_scale", window_scale_);
        assignFloat(window_config, "logical_scale", window_logical_scale_);
        assignBool(window_config, "resizable", window_resizable_);
    }

    if (const auto it = j.find("graphics"); it != j.end() && it->is_object()) {
        const auto& graphics_config = *it;
        assignBool(graphics_config, "vsync", vsync_enabled_);
        assignBool(graphics_config, "debug_ui", debug_ui_enabled_);
        assignBool(graphics_config, "rmlui_debugger", rmlui_debugger_enabled_);
        if (const auto it_filter = graphics_config.find("rmlui_texture_filter");
            it_filter != graphics_config.end() && it_filter->is_string()) {
            const std::string filter_value = it_filter->get<std::string>();
            if (!engine::ui::rmlui::parseRmlUiTextureFilterMode(filter_value, rmlui_texture_filter_mode_)) {
                spdlog::warn("未知的 RmlUi 纹理采样模式 '{}'，回退为 nearest。", filter_value);
                rmlui_texture_filter_mode_ = kDefaultRmlUiTextureFilterMode;
            }
        }
    }

    if (const auto it = j.find("performance"); it != j.end() && it->is_object()) {
        const auto& perf_config = *it;
        assignInt(perf_config, "target_fps", target_fps_);
        assignInt(perf_config, "logic_tick_hz", logic_tick_hz_);
        assignInt(perf_config, "max_ticks_per_frame", max_ticks_per_frame_);
        assignBool(perf_config, "render_interpolation", render_interpolation_enabled_);

        if (target_fps_ < 0) {
            spdlog::warn("目标 FPS 不能为负数。设置为 0（无限制）。");
            target_fps_ = 0;
        }
        if (logic_tick_hz_ <= 0) {
            spdlog::warn("logic_tick_hz 必须大于 0。设置为默认值 60。");
            logic_tick_hz_ = 60;
        }
        if (max_ticks_per_frame_ < 1) {
            spdlog::warn("max_ticks_per_frame 不能小于 1。设置为 1。");
            max_ticks_per_frame_ = 1;
        }
    }
}

nlohmann::ordered_json Config::toJson() const {
    return nlohmann::ordered_json{
        {"window", {
            {"title", window_title_},
            {"width", window_width_},
            {"height", window_height_},
            {"window_scale", window_scale_},
            {"logical_scale", window_logical_scale_},
            {"resizable", window_resizable_}
        }},
        {"graphics", {
            {"vsync", vsync_enabled_},
            {"debug_ui", debug_ui_enabled_},
            {"rmlui_debugger", rmlui_debugger_enabled_},
            {"rmlui_texture_filter", engine::ui::rmlui::rmlUiTextureFilterModeToString(rmlui_texture_filter_mode_)}
        }},
        {"performance", {
            {"target_fps", target_fps_},
            {"logic_tick_hz", logic_tick_hz_},
            {"max_ticks_per_frame", max_ticks_per_frame_},
            {"render_interpolation", render_interpolation_enabled_}
        }}
    };
}

} // namespace engine::core 
