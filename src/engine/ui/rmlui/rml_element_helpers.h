#pragma once

#include <RmlUi/Core/Element.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <string>
#include <string_view>

namespace engine::ui::rmlui {

[[nodiscard]] inline float snapToPixel(float value) {
    return std::round(value);
}

[[nodiscard]] inline std::string toPixelString(float value) {
    return std::format("{:.1f}px", std::max(0.0f, snapToPixel(value)));
}

inline void setPixelProperty(Rml::Element* element, std::string_view name, float value) {
    if (!element) {
        return;
    }
    element->SetProperty(std::string(name), toPixelString(value));
}

[[nodiscard]] inline std::string textToInnerRml(std::string_view text) {
    std::string output;
    output.reserve(text.size() + 16);

    for (const char c : text) {
        switch (c) {
        case '&':
            output += "&amp;";
            break;
        case '<':
            output += "&lt;";
            break;
        case '>':
            output += "&gt;";
            break;
        case '"':
            output += "&quot;";
            break;
        case '\n':
            output += "<br/>";
            break;
        default:
            output.push_back(c);
            break;
        }
    }

    return output;
}

} // namespace engine::ui::rmlui
