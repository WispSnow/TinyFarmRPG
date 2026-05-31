#include "save_slot_summary.h"

#include "save_data.h"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <fstream>
#include <limits>
#include <string_view>

namespace game::save {
namespace {

bool readUInt32Value(const nlohmann::json& value,
                     std::uint32_t& out,
                     std::string_view field,
                     std::string& out_error) {
    if (!value.is_number_integer()) {
        out_error = "Save slot summary: " + std::string(field) + " is not an unsigned int";
        return false;
    }

    if (value.is_number_unsigned()) {
        const auto raw = value.get<std::uint64_t>();
        if (raw > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())) {
            out_error = "Save slot summary: " + std::string(field) + " is out of range";
            return false;
        }
        out = static_cast<std::uint32_t>(raw);
        return true;
    }

    const auto raw = value.get<std::int64_t>();
    if (raw < 0 ||
        static_cast<std::uint64_t>(raw) > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())) {
        out_error = "Save slot summary: " + std::string(field) + " is out of range";
        return false;
    }
    out = static_cast<std::uint32_t>(raw);
    return true;
}

} // namespace

std::optional<SlotSummary> tryReadSlotSummary(const std::filesystem::path& path, std::string& out_error) {
    out_error.clear();

    std::ifstream in(path);
    if (!in.is_open()) {
        out_error = "Could not open save file";
        return std::nullopt;
    }

    const auto json = nlohmann::json::parse(in, nullptr, false);
    if (json.is_discarded()) {
        out_error = "Failed to parse JSON";
        return std::nullopt;
    }

    if (!json.is_object()) {
        out_error = "Save root is not an object";
        return std::nullopt;
    }

    if (!json.contains(json_keys::SCHEMA_VERSION.data())) {
        out_error = "Save is missing schema_version";
        return std::nullopt;
    }
    std::uint32_t schema_version = 0;
    if (!readUInt32Value(json[json_keys::SCHEMA_VERSION.data()],
                         schema_version,
                         json_keys::SCHEMA_VERSION,
                         out_error)) {
        return std::nullopt;
    }
    if (schema_version == 0u) {
        out_error = "Save is missing schema_version";
        return std::nullopt;
    }
    if (schema_version > SAVE_SCHEMA_VERSION) {
        out_error = "Unsupported save schema_version";
        return std::nullopt;
    }

    SlotSummary out{};
    if (!json.contains(json_keys::GAME_TIME.data()) || !json[json_keys::GAME_TIME.data()].is_object()) {
        out_error = "Save is missing game_time";
        return std::nullopt;
    }

    const auto& game_time = json[json_keys::GAME_TIME.data()];
    if (game_time.contains(json_keys::DAY.data()) &&
        !readUInt32Value(game_time[json_keys::DAY.data()], out.day, "game_time.day", out_error)) {
        return std::nullopt;
    }

    if (json.contains(json_keys::TIMESTAMP.data()) && json[json_keys::TIMESTAMP.data()].is_string()) {
        out.timestamp = json[json_keys::TIMESTAMP.data()].get<std::string>();
    } else if (json.contains(json_keys::TIMESTAMP.data())) {
        out_error = "Save slot summary: timestamp is not a string";
        return std::nullopt;
    }
    return out;
}

} // namespace game::save
