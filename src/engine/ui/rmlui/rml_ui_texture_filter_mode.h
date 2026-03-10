#pragma once

#include <cstdint>
#include <string_view>

namespace engine::ui::rmlui {

enum class RmlUiTextureFilterMode : std::uint8_t {
    Nearest = 0,
    Linear
};

[[nodiscard]] constexpr std::string_view rmlUiTextureFilterModeToString(RmlUiTextureFilterMode mode) {
    switch (mode) {
    case RmlUiTextureFilterMode::Nearest:
        return "nearest";
    case RmlUiTextureFilterMode::Linear:
        return "linear";
    }
    return "nearest";
}

[[nodiscard]] constexpr bool parseRmlUiTextureFilterMode(std::string_view value, RmlUiTextureFilterMode& out_mode) {
    if (value == "nearest") {
        out_mode = RmlUiTextureFilterMode::Nearest;
        return true;
    }
    if (value == "linear") {
        out_mode = RmlUiTextureFilterMode::Linear;
        return true;
    }
    return false;
}

} // namespace engine::ui::rmlui
