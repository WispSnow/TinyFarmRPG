#include "dialogue_bubble_view.h"

#include "engine/core/context.h"
#include "engine/render/text_renderer.h"
#include "engine/ui/ui_preset_manager.h"

#include <algorithm>
#include <entt/core/hashed_string.hpp>

using namespace entt::literals;

namespace {
constexpr entt::id_type DIALOGUE_PRESET_ID = "dialogue_bubble"_hs;
constexpr float DEFAULT_WIDTH = 160.0F;
constexpr float DEFAULT_HEIGHT = 48.0F;
} // namespace

namespace game::ui {

DialogueBubbleView::DialogueBubbleView(engine::core::Context& context,
                                       engine::render::TextRenderer& text_renderer,
                                       entt::id_type font_id,
                                       int font_size)
    : UIElement(glm::vec2{0.0F}, glm::vec2{DEFAULT_WIDTH, DEFAULT_HEIGHT}),
      text_renderer_(text_renderer),
      font_id_(engine::ui::resolveUIFontId(font_id)),
      font_size_(font_size) {
    setAnchor({0.0F, 0.0F}, {0.0F, 0.0F});
    setPivot({0.5F, 1.0F});
    buildSkin(context);
    buildLayout();
    setVisible(false);
}

void DialogueBubbleView::buildSkin(engine::core::Context& context) {
    auto& preset_mgr = context.getUIPresetManager();
    if (const auto* preset = preset_mgr.getImagePreset(DIALOGUE_PRESET_ID)) {
        bubble_image_ = *preset;
    } else {
        bubble_image_ = engine::render::Image{};
    }
}

void DialogueBubbleView::buildLayout() {
    auto panel = std::make_unique<engine::ui::UIPanel>(
        glm::vec2{0.0F}, getRequestedSize(), std::nullopt, bubble_image_);
    panel->setPivot({0.0F, 0.0F});
    panel_ = panel.get();

    auto label = std::make_unique<engine::ui::UILabel>(
        text_renderer_,
        "",
        font_id_,
        font_size_,
        glm::vec2{padding_, padding_},
        engine::utils::FColor::black());
    label->setPivot({0.0F, 0.0F});
    label_ = label.get();
    label_->setShadowEnabled(false);
    panel_->addChild(std::move(label));
    addChild(std::move(panel));
}

void DialogueBubbleView::setText(std::string_view text) {
    if (!label_) {
        return;
    }

    label_->setText(text);
    refreshLayoutFromText();
}

void DialogueBubbleView::refreshLayoutFromText() {
    if (!panel_ || !label_) {
        return;
    }

    const glm::vec2 label_size = label_->getSize();
    const glm::vec2 desired_size{
        std::max(DEFAULT_WIDTH, label_size.x + padding_ * 2.0F),
        std::max(DEFAULT_HEIGHT, label_size.y + padding_ * 2.0F)};
    setSize(desired_size);
    panel_->setSize(desired_size);
}

} // namespace game::ui

