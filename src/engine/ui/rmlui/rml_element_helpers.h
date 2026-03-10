#pragma once

#include "engine/ui/ui_types.h"

#include <RmlUi/Core/ComputedValues.h>
#include <RmlUi/Core/Element.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
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

inline void setPaddingProperties(Rml::Element* element, const engine::ui::Thickness& padding) {
    setPixelProperty(element, "padding-left", padding.left);
    setPixelProperty(element, "padding-top", padding.top);
    setPixelProperty(element, "padding-right", padding.right);
    setPixelProperty(element, "padding-bottom", padding.bottom);
}

inline void setFontSizeProperty(Rml::Element* element, float font_size) {
    setPixelProperty(element, "font-size", font_size);
}

[[nodiscard]] inline float computeLineSpacingScale(float target_line_height, float font_line_height) {
    if (target_line_height <= 0.0f || font_line_height <= 0.0f) {
        return 1.0f;
    }
    return std::max(target_line_height / font_line_height, 0.01f);
}

[[nodiscard]] inline float computedLengthOr(const Rml::Style::LengthPercentage& value, float fallback) {
    if (value.type != Rml::Style::LengthPercentage::Length) {
        return fallback;
    }
    const float resolved = value.value;
    if (!std::isfinite(resolved) || resolved >= std::numeric_limits<float>::max() * 0.5f) {
        return fallback;
    }
    return resolved;
}

[[nodiscard]] inline float computedLengthOr(const Rml::Style::LengthPercentageAuto& value, float fallback) {
    if (value.type != Rml::Style::LengthPercentageAuto::Length) {
        return fallback;
    }
    const float resolved = value.value;
    if (!std::isfinite(resolved) || resolved >= std::numeric_limits<float>::max() * 0.5f) {
        return fallback;
    }
    return resolved;
}

[[nodiscard]] inline engine::ui::Thickness getComputedPadding(const Rml::Element* element,
                                                              const engine::ui::Thickness& fallback) {
    if (!element) {
        return fallback;
    }

    const auto& computed = element->GetComputedValues();
    return engine::ui::Thickness{
        computedLengthOr(computed.padding_left(), fallback.left),
        computedLengthOr(computed.padding_top(), fallback.top),
        computedLengthOr(computed.padding_right(), fallback.right),
        computedLengthOr(computed.padding_bottom(), fallback.bottom)
    };
}

[[nodiscard]] inline float getComputedFontSize(const Rml::Element* element, float fallback) {
    if (!element) {
        return fallback;
    }
    const float value = element->GetComputedValues().font_size();
    return std::isfinite(value) && value > 0.0f ? value : fallback;
}

[[nodiscard]] inline float getComputedLineHeight(const Rml::Element* element, float fallback) {
    if (!element) {
        return fallback;
    }
    const float value = element->GetComputedValues().line_height().value;
    return std::isfinite(value) && value > 0.0f ? value : fallback;
}

[[nodiscard]] inline float getComputedWidth(const Rml::Element* element, float fallback) {
    if (!element) {
        return fallback;
    }
    return computedLengthOr(element->GetComputedValues().width(), fallback);
}

[[nodiscard]] inline float getComputedHeight(const Rml::Element* element, float fallback) {
    if (!element) {
        return fallback;
    }
    return computedLengthOr(element->GetComputedValues().height(), fallback);
}

[[nodiscard]] inline float getComputedMaxWidth(const Rml::Element* element, float fallback) {
    if (!element) {
        return fallback;
    }
    return computedLengthOr(element->GetComputedValues().max_width(), fallback);
}

[[nodiscard]] inline float getComputedMarginBottom(const Rml::Element* element, float fallback) {
    if (!element) {
        return fallback;
    }
    return computedLengthOr(element->GetComputedValues().margin_bottom(), fallback);
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
