#include "floating_notice_view.h"

#include "engine/core/context.h"
#include "engine/ui/rmlui/rml_element_helpers.h"
#include "engine/ui/rmlui/rml_ui_runtime.h"

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <spdlog/spdlog.h>

namespace {

using engine::ui::rmlui::setPixelProperty;
using engine::ui::rmlui::snapToPixel;
using engine::ui::rmlui::textToInnerRml;

constexpr std::string_view DOCUMENT_PATH = "ui/rmlui/hud/floating_notice.rml";

} // namespace

namespace game::ui {

FloatingNoticeView::FloatingNoticeView(engine::core::Context& context, uint64_t owner_scene_id)
    : context_(context) {
    initDocument(owner_scene_id);
    setVisible(false);
}

FloatingNoticeView::~FloatingNoticeView() {
    if (document_ && runtime_) {
        runtime_->unloadDocument(document_);
    }
    document_ = nullptr;
    panel_ = nullptr;
    text_element_ = nullptr;
}

void FloatingNoticeView::initDocument(uint64_t owner_scene_id) {
    runtime_ = context_.getRmlUi();
    if (!runtime_) {
        spdlog::error("FloatingNoticeView: RmlUiRuntime 不可用。");
        return;
    }

    document_ = runtime_->loadDocument(DOCUMENT_PATH, owner_scene_id);
    if (!document_) {
        spdlog::error("FloatingNoticeView: 加载 RML 文档失败: {}", DOCUMENT_PATH);
        return;
    }

    panel_ = document_->GetElementById("floating-notice-panel");
    text_element_ = document_->GetElementById("floating-notice-text");
    if (!panel_ || !text_element_) {
        spdlog::error("FloatingNoticeView: RML 元素缺失。");
        runtime_->unloadDocument(document_);
        document_ = nullptr;
        panel_ = nullptr;
        text_element_ = nullptr;
        return;
    }

    runtime_->hideDocument(document_);
}

void FloatingNoticeView::refreshLayoutMetrics() {
    if (!document_ || !panel_) {
        return;
    }

    document_->UpdateDocument();
    size_ = {
        snapToPixel(panel_->GetOffsetWidth()),
        snapToPixel(panel_->GetOffsetHeight()),
    };
}

void FloatingNoticeView::setText(std::string_view text) {
    text_ = std::string(text);
    if (text_element_) {
        text_element_->SetInnerRML(textToInnerRml(text_));
    }
    if (visible_) {
        refreshLayoutMetrics();
    }
}

void FloatingNoticeView::setVisible(bool visible) {
    visible_ = visible;
    if (!document_ || !runtime_) {
        return;
    }

    if (visible) {
        runtime_->showDocument(document_);
        refreshLayoutMetrics();
    } else {
        runtime_->hideDocument(document_);
    }
}

void FloatingNoticeView::setWorldAnchor(glm::vec2 world_position, glm::vec2 screen_offset) {
    world_anchor_.setWorldAnchor(world_position, screen_offset);
}

void FloatingNoticeView::clearWorldAnchor() {
    world_anchor_.clearWorldAnchor();
}

void FloatingNoticeView::refreshAnchoredPosition(const engine::render::Camera& camera, float interpolation_alpha) {
    if (!visible_ || !panel_ || !document_ || !world_anchor_.hasWorldAnchor()) {
        return;
    }

    const glm::vec2 screen_anchor = world_anchor_.resolveScreenAnchorPosition(camera, interpolation_alpha);
    const glm::vec2 top_left = screen_anchor - size_ * pivot_;
    setPixelProperty(panel_, "left", top_left.x);
    setPixelProperty(panel_, "top", top_left.y);
}

} // namespace game::ui
