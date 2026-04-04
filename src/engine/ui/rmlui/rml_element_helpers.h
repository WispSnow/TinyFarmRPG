#pragma once

#include <RmlUi/Core/Element.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <string>
#include <string_view>

namespace engine::ui::rmlui {

/// @brief 将浮点坐标吸附到最近的像素整数，避免子像素布局带来的显示误差。
/// @param value 待吸附的原始数值。
/// @return 四舍五入后的像素值。
[[nodiscard]] inline float snapToPixel(float value) {
    return std::round(value);
}

/// @brief 将像素数值转换为 RmlUi 可识别的 `Npx` 字符串，并确保结果不小于 0。
/// @param value 待转换的像素数值。
/// @return 格式化后的像素字符串。
[[nodiscard]] inline std::string toPixelString(float value) {
    return std::format("{:.1f}px", std::max(0.0f, snapToPixel(value)));
}

/// @brief 为元素设置像素单位属性，空指针时静默跳过。
/// @param element 目标 RmlUi 元素。
/// @param name 属性名，例如 `left`、`top`、`width`。
/// @param value 待写入的像素值。
inline void setPixelProperty(Rml::Element* element, std::string_view name, float value) {
    if (!element) {
        return;
    }
    element->SetProperty(std::string(name), toPixelString(value));
}

/// @brief 将普通文本转为可安全写入 inner RML 的字符串，并处理换行。
/// @param text 原始文本内容。
/// @return 转义后的 RML 字符串，其中换行会被替换为 `<br/>`。
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
