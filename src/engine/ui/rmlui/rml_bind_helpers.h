#pragma once

#include <RmlUi/Core/Types.h>

#include <string_view>

namespace engine::ui::rmlui {

[[nodiscard]] inline bool updateBoundBool(bool& current, bool next) {
    if (current == next) {
        return false;
    }
    current = next;
    return true;
}

[[nodiscard]] inline bool updateBoundString(Rml::String& current, std::string_view next) {
    const Rml::String candidate{next.data(), next.size()};
    if (current == candidate) {
        return false;
    }
    current = candidate;
    return true;
}

} // namespace engine::ui::rmlui
