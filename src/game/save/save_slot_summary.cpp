#include "save_slot_summary.h"

#include "save_data.h"

#include <nlohmann/json.hpp>

#include <fstream>

namespace game::save {

std::optional<SlotSummary> tryReadSlotSummary(const std::filesystem::path& path, std::string& out_error) {
    out_error.clear();

    std::ifstream in(path);
    if (!in.is_open()) {
        out_error = "无法打开存档文件";
        return std::nullopt;
    }

    const auto json = nlohmann::json::parse(in, nullptr, false);
    if (json.is_discarded()) {
        out_error = "解析 JSON 失败";
        return std::nullopt;
    }

    if (!json.is_object()) {
        out_error = "存档根节点不是对象";
        return std::nullopt;
    }

    const std::uint32_t schema_version = json.value<std::uint32_t>(json_keys::SCHEMA_VERSION.data(), 0u);
    if (schema_version == 0u) {
        out_error = "存档缺少 schema_version";
        return std::nullopt;
    }
    if (schema_version > SAVE_SCHEMA_VERSION) {
        out_error = "存档 schema_version 不支持";
        return std::nullopt;
    }

    SlotSummary out{};
    if (!json.contains(json_keys::GAME_TIME.data()) || !json[json_keys::GAME_TIME.data()].is_object()) {
        out_error = "存档缺少 game_time";
        return std::nullopt;
    }

    out.day = json[json_keys::GAME_TIME.data()].value<std::uint32_t>(json_keys::DAY.data(), 0u);

    if (json.contains(json_keys::TIMESTAMP.data()) && json[json_keys::TIMESTAMP.data()].is_string()) {
        out.timestamp = json[json_keys::TIMESTAMP.data()].get<std::string>();
    }
    return out;
}

} // namespace game::save
