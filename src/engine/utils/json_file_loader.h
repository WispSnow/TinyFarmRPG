#pragma once

#include <fstream>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace engine::utils {

[[nodiscard]] inline bool loadJsonObjectFile(
    std::string_view file_path,
    nlohmann::json& out_json,
    std::string_view log_prefix,
    spdlog::level::level_enum log_level = spdlog::level::warn
) {
    std::ifstream file{std::string(file_path)};
    if (!file.is_open()) {
        spdlog::log(log_level, "{}: 无法打开 JSON 文件 '{}'", log_prefix, file_path);
        return false;
    }

    const std::string file_content(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

    out_json = nlohmann::json::parse(file_content, nullptr, false);
    if (out_json.is_discarded()) {
        spdlog::log(log_level, "{}: 解析 JSON 文件 '{}' 失败。", log_prefix, file_path);
        return false;
    }

    if (!out_json.is_object()) {
        spdlog::log(log_level, "{}: JSON 文件 '{}' 根节点不是对象。", log_prefix, file_path);
        return false;
    }

    return true;
}

} // namespace engine::utils
