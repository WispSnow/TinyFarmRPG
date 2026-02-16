#pragma once
#include "ui_element.h"
#include "ui_defaults.h"
#include "engine/utils/defs.h"
#include "engine/render/text_renderer.h"
#include <string>
#include <string_view>
#include <optional>
#include <cstdint>
#include <entt/entity/fwd.hpp>

namespace engine::ui {

/**
 * @brief UILabel 类用于创建和管理用户界面中的文本标签
 * 
 * UILabel 继承自 UIElement，提供了文本渲染功能。
 * 它可以设置文本内容、字体ID、字体大小和文本颜色。
 * 
 * @note 需要一个文本渲染器来获取和更新文本尺寸。
 */
class UILabel final : public UIElement {
private:
    engine::render::TextRenderer& text_renderer_;   ///< @brief 需要文本渲染器，用于获取/更新文本尺寸

    std::string text_;                          ///< @brief 文本内容  
    std::string font_path_;                     ///< @brief 字体路径
    entt::id_type font_id_;                     ///< @brief 字体ID
    int font_size_;                             ///< @brief 字体大小   
    entt::id_type style_id_{entt::null};        ///< @brief 文本样式 ID (entt::hashed_string，null=使用 TextRenderer 默认UI样式)
    engine::utils::TextRenderOverrides overrides_{}; ///< @brief 实例级覆写（用于动态颜色/阴影/缩放等）
    std::uint64_t last_layout_revision_{0};     ///< @brief 用于检测 layout 变更并重新计算尺寸

public:
    UILabel(engine::render::TextRenderer& text_renderer,
            std::string_view text,
            std::string_view font_path = DEFAULT_UI_FONT_PATH,
            int font_size = DEFAULT_UI_FONT_SIZE_PX,
            glm::vec2 position = {0.0f, 0.0f},
            std::optional<engine::utils::FColor> text_color = std::nullopt);

    // --- 核心方法 ---
    using UIElement::render;

    // --- Setters & Getters ---
    std::string_view getText() const { return text_; }
    entt::id_type getFontId() const { return font_id_; }
    int getFontSize() const { return font_size_; }
    entt::id_type getStyleId() const { return style_id_; }
    std::string_view getStyleKey() const {
        if (style_id_ == entt::null || !text_renderer_.hasTextStyle(style_id_)) {
            return text_renderer_.getDefaultUIStyleKey();
        }
        return text_renderer_.getTextStyleKey(style_id_);
    }

    void setText(std::string_view text);                      ///< @brief 设置文本内容, 同时更新尺寸
    void setFontPath(std::string_view font_path);              ///< @brief 设置字体路径, 同时更新ID和尺寸
    void setFontSize(int font_size);                            ///< @brief 设置字体大小, 同时更新尺寸
    void setStyleKey(std::string_view style_key);               ///< @brief 设置样式 key (空=默认)
    void setTextColor(engine::utils::FColor text_color);       ///< @brief 设置文本颜色（会同步到渐变起止色，并关闭渐变）
    void setShadowColor(engine::utils::FColor shadow_color);   ///< @brief 设置阴影颜色
    void setShadowOffset(glm::vec2 shadow_offset);             ///< @brief 设置阴影偏移
    void setShadowEnabled(bool enabled);                       ///< @brief 启用/禁用阴影
    void clearOverrides();                                     ///< @brief 清空所有实例覆写
    
protected:
    void update(float delta_time, engine::core::Context& context) override;
    void renderSelf(engine::core::Context& context) override;

private:
    void refreshSize();  ///< @brief 重新解析布局并更新元素尺寸
};


} // namespace engine::ui
