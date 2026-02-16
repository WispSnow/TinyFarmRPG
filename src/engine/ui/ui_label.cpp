#include "ui_label.h"
#include "engine/core/context.h"
#include "engine/render/text_renderer.h"
#include <entt/core/hashed_string.hpp>
#include <spdlog/spdlog.h>

namespace engine::ui {

namespace {

[[nodiscard]] engine::utils::LayoutOptions resolveLabelLayout(const engine::render::TextRenderer& renderer,
                                                             entt::id_type style_id,
                                                             const engine::utils::TextRenderOverrides& overrides) {
    const entt::id_type resolved_style =
        (style_id == entt::null || !renderer.hasTextStyle(style_id)) ? renderer.getDefaultUIStyleId() : style_id;
    auto layout = renderer.getTextStyle(resolved_style).layout;
    if (overrides.glyph_scale) {
        layout.glyph_scale = *overrides.glyph_scale;
    }
    return layout;
}

} // namespace

UILabel::UILabel(engine::render::TextRenderer& text_renderer,
                 std::string_view text,
                 std::string_view font_path,
                 int font_size,
                 glm::vec2 position,
                 std::optional<engine::utils::FColor> text_color
                )
    : UIElement(std::move(position)),
      text_renderer_(text_renderer),
      text_(text),
      font_path_(font_path),
      font_id_(entt::hashed_string{font_path.data(), font_path.size()}),
      font_size_(font_size) {
    if (text_color) {
        overrides_.color = *text_color;
    }
    last_layout_revision_ = text_renderer_.getLayoutRevision();
    refreshSize();
    spdlog::trace("UILabel 构造完成");
}

void UILabel::update(float delta_time, engine::core::Context& context) {
    const auto revision = text_renderer_.getLayoutRevision();
    if (revision != last_layout_revision_) {
        refreshSize();
        last_layout_revision_ = revision;
    }

    UIElement::update(delta_time, context);
}

void UILabel::renderSelf(engine::core::Context& /*context*/) {
    if (text_.empty()) {
        return;
    }

    const engine::utils::TextRenderOverrides* overrides = overrides_.isEmpty() ? nullptr : &overrides_;
    text_renderer_.drawUIText(text_, font_id_, font_size_, getScreenPosition(), style_id_, overrides);
}

void UILabel::setText(std::string_view text)
{
    text_ = text;
    refreshSize();
}

void UILabel::setFontPath(std::string_view font_path)
{
    font_path_ = font_path;
    font_id_ = entt::hashed_string{font_path_.c_str(), font_path_.size()};
    refreshSize();
}

void UILabel::setFontSize(int font_size)
{
    font_size_ = font_size;
    refreshSize();
}

void UILabel::setStyleKey(std::string_view style_key) {
    if (style_key.empty()) {
        style_id_ = entt::null;
    } else {
        style_id_ = entt::hashed_string{style_key.data(), style_key.size()};
    }
    refreshSize();
}

void UILabel::setTextColor(engine::utils::FColor text_color) {
    overrides_.color = text_color;
}

void UILabel::setShadowColor(engine::utils::FColor shadow_color) {
    overrides_.shadow_color = shadow_color;
}

void UILabel::setShadowOffset(glm::vec2 shadow_offset) {
    overrides_.shadow_offset = shadow_offset;
}

void UILabel::setShadowEnabled(bool enabled) {
    overrides_.shadow_enabled = enabled;
}

void UILabel::clearOverrides() {
    const bool layout_might_change = overrides_.glyph_scale.has_value();
    overrides_ = engine::utils::TextRenderOverrides{};
    if (layout_might_change) {
        refreshSize();
    }
}

void UILabel::refreshSize() {
    const auto layout = resolveLabelLayout(text_renderer_, style_id_, overrides_);
    setSize(text_renderer_.getTextSize(text_, font_id_, font_size_, font_path_, &layout));
}

} // namespace engine::ui
