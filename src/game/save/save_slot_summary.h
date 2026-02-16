#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace game::save {

struct SlotSummary {
    std::uint32_t day{0};
    std::string timestamp{};
};

[[nodiscard]] std::optional<SlotSummary> tryReadSlotSummary(const std::filesystem::path& path, std::string& out_error);

} // namespace game::save

