#include "dialogue_bubble_view.h"

#include "engine/core/context.h"
#include "engine/render/opengl/gl_renderer.h"
#include "engine/render/text_renderer.h"
#include "engine/ui/rmlui/rml_ui_layer.h"
#include "engine/ui/rmlui/rml_element_helpers.h"

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <string>

namespace {
using engine::ui::rmlui::setFontSizeProperty;
using engine::ui::rmlui::setPaddingProperties;
using engine::ui::rmlui::setPixelProperty;
using engine::ui::rmlui::snapToPixel;
using engine::ui::rmlui::textToInnerRml;

constexpr float DEFAULT_WIDTH = 160.0F;
constexpr float DEFAULT_HEIGHT = 48.0F;
constexpr std::string_view DOCUMENT_PATH = "ui/rmlui/hud/dialogue_bubble.rml";

} // namespace

namespace game::ui {

DialogueBubbleView::DialogueBubbleView(engine::core::Context& context,
                                       engine::render::TextRenderer& text_renderer,
                                       uint64_t owner_scene_id,
                                       entt::id_type font_id,
                                       int font_size)
    : UIElement(glm::vec2{0.0F}, glm::vec2{DEFAULT_WIDTH, DEFAULT_HEIGHT}),
      context_(context),
      text_renderer_(text_renderer),
      font_id_(engine::ui::resolveUIFontId(font_id)),
      font_size_(font_size) {
    setAnchor({0.0F, 0.0F}, {0.0F, 0.0F});
    setPivot({0.5F, 1.0F});
    initDocument(owner_scene_id);
    setVisible(false);
}

DialogueBubbleView::~DialogueBubbleView() {
    if (document_ && layer_) {
        layer_->unloadDocument(document_);
    }
    document_ = nullptr;
    panel_ = nullptr;
    text_element_ = nullptr;
}

void DialogueBubbleView::initDocument(uint64_t owner_scene_id) {
    layer_ = context_.getGLRenderer().getRmlUILayer();
    if (!layer_) {
        spdlog::error("DialogueBubbleView: RmlUILayer 不可用。");
        return;
    }

    document_ = layer_->loadDocument(DOCUMENT_PATH, owner_scene_id);
    if (!document_) {
        spdlog::error("DialogueBubbleView: 加载 RML 文档失败: {}", DOCUMENT_PATH);
        return;
    }

    panel_ = document_->GetElementById("dialogue-bubble-panel");
    text_element_ = document_->GetElementById("dialogue-bubble-text");
    if (!panel_ || !text_element_) {
        spdlog::error("DialogueBubbleView: RML 元素缺失。");
        if (layer_) {
            layer_->unloadDocument(document_);
        }
        document_ = nullptr;
        panel_ = nullptr;
        text_element_ = nullptr;
        return;
    }

    setPaddingProperties(panel_, engine::ui::Thickness{padding_, padding_, padding_, padding_});
    setFontSizeProperty(text_element_, static_cast<float>(font_size_));
    setPixelProperty(text_element_, "line-height", static_cast<float>(font_size_));
    layer_->hideDocument(document_);
    refreshLayoutFromText();
}

void DialogueBubbleView::setText(std::string_view text) {
    text_ = std::string(text);
    if (text_element_) {
        text_element_->SetInnerRML(textToInnerRml(text_));
    }
    refreshLayoutFromText();
}

void DialogueBubbleView::setVisible(bool visible) {
    UIElement::setVisible(visible);
    if (!document_ || !layer_) {
        return;
    }

    if (visible) {
        layer_->showDocument(document_);
    } else {
        layer_->hideDocument(document_);
    }
}

void DialogueBubbleView::refreshLayoutFromText() {
    const glm::vec2 text_size = text_renderer_.getTextSize(text_, font_id_, font_size_);
    const glm::vec2 outer_size{
        snapToPixel(std::max(DEFAULT_WIDTH, text_size.x + padding_ * 2.0F)),
        snapToPixel(std::max(DEFAULT_HEIGHT, text_size.y + padding_ * 2.0F))
    };
    const glm::vec2 content_box_size{
        snapToPixel(std::max(0.0F, outer_size.x - padding_ * 2.0F)),
        snapToPixel(std::max(0.0F, outer_size.y - padding_ * 2.0F))
    };

    setSize(outer_size);
    setPixelProperty(panel_, "width", content_box_size.x);
    setPixelProperty(panel_, "height", content_box_size.y);
}

void DialogueBubbleView::renderSelf(engine::core::Context& context) {
    (void)context;
    if (!panel_ || !document_) {
        return;
    }

    const glm::vec2 position = getScreenPosition();
    setPixelProperty(panel_, "left", position.x);
    setPixelProperty(panel_, "top", position.y);
}

} // namespace game::ui
