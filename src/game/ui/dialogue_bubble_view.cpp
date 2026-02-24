#include "dialogue_bubble_view.h"
#include "engine/core/context.h"
#include "engine/ui/ui_preset_manager.h"
#include "engine/render/text_renderer.h"
#include <entt/core/hashed_string.hpp>
#include <algorithm>

using namespace entt::literals;

namespace {
constexpr entt::id_type DIALOGUE_PRESET_ID = "dialogue_bubble"_hs;
constexpr float DEFAULT_WIDTH = 160.0f;
constexpr float DEFAULT_HEIGHT = 48.0f;
}

namespace game::ui {

DialogueBubbleView::DialogueBubbleView(engine::core::Context& context,
                                       engine::render::TextRenderer& text_renderer,
                                       entt::id_type font_id,
                                       int font_size)
    : UIElement(glm::vec2{0.0f}, glm::vec2{DEFAULT_WIDTH, DEFAULT_HEIGHT}),
      text_renderer_(text_renderer),
      font_id_(engine::ui::resolveUIFontId(font_id)),
      font_size_(font_size) {
    setAnchor({0.0f, 0.0f}, {0.0f, 0.0f});
    setPivot({0.5f, 1.0f});
    buildSkin(context);
    buildLayout();
    setVisible(false);
}

void DialogueBubbleView::buildSkin(engine::core::Context& context) {
    auto& preset_mgr = context.getUIPresetManager();
    if (const auto* preset = preset_mgr.getImagePreset(DIALOGUE_PRESET_ID)) {
        bubble_image_ = *preset;
    } else {
        // fallback: simple transparent panel
        bubble_image_ = engine::render::Image{};
    }
}

void DialogueBubbleView::buildLayout() {
    auto panel = std::make_unique<engine::ui::UIPanel>(glm::vec2{0.0f}, getRequestedSize(), std::nullopt, bubble_image_);
    panel->setPivot({0.0f, 0.0f});
    panel_ = panel.get();

    auto label = std::make_unique<engine::ui::UILabel>(
        text_renderer_, "", font_id_, font_size_, glm::vec2{padding_, padding_}, engine::utils::FColor::black());
    label->setPivot({0.0f, 0.0f});
    label_ = label.get();
    label_->setShadowEnabled(false);
    panel_->addChild(std::move(label));
    addChild(std::move(panel));
}

void DialogueBubbleView::setText(std::string_view text) {
    if (label_) {
        label_->setText(text);
        refreshLayoutFromText();
    }
}

void DialogueBubbleView::refreshLayoutFromText() {
    if (!panel_ || !label_) return;

    const auto label_size = label_->getSize();
    const glm::vec2 desired_size{
        std::max(DEFAULT_WIDTH, label_size.x + padding_ * 2.0f),
        std::max(DEFAULT_HEIGHT, label_size.y + padding_ * 2.0f)
    };
    setSize(desired_size);
    panel_->setSize(desired_size);
}

} // namespace game::ui
