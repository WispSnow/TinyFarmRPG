#include "dialogue_box_view.h"

#include "engine/core/context.h"
#include "engine/ui/rmlui/rml_element_helpers.h"
#include "engine/ui/rmlui/rml_ui_runtime.h"

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <spdlog/spdlog.h>

namespace {

using engine::ui::rmlui::textToInnerRml;

constexpr std::string_view DOCUMENT_PATH = "ui/rmlui/hud/dialogue_box.rml";
constexpr std::string_view HIDDEN_CLASS = "is-hidden";
constexpr std::string_view NONE_DECORATOR = "none";

void setHidden(Rml::Element* element, bool hidden) {
    if (!element) {
        return;
    }
    element->SetClass(Rml::String{HIDDEN_CLASS.data(), HIDDEN_CLASS.size()}, hidden);
}

} // namespace

namespace game::ui {

DialogueBoxView::DialogueBoxView(engine::core::Context& context, std::uint64_t owner_scene_id)
    : context_(context) {
    initDocument(owner_scene_id);
    setVisible(false);
}

DialogueBoxView::~DialogueBoxView() {
    if (document_ && runtime_) {
        runtime_->unloadDocument(document_);
    }
    document_ = nullptr;
    panel_ = nullptr;
    portrait_frame_ = nullptr;
    portrait_ = nullptr;
    speaker_element_ = nullptr;
    text_element_ = nullptr;
    continue_element_ = nullptr;
}

bool DialogueBoxView::isReady() const {
    return document_ != nullptr &&
           panel_ != nullptr &&
           portrait_frame_ != nullptr &&
           portrait_ != nullptr &&
           speaker_element_ != nullptr &&
           text_element_ != nullptr &&
           continue_element_ != nullptr;
}

void DialogueBoxView::initDocument(std::uint64_t owner_scene_id) {
    runtime_ = context_.getRmlUi();
    if (!runtime_) {
        spdlog::error("DialogueBoxView: RmlUiRuntime 不可用。");
        return;
    }

    document_ = runtime_->loadDocument(DOCUMENT_PATH, owner_scene_id);
    if (!document_) {
        spdlog::error("DialogueBoxView: 加载 RML 文档失败: {}", DOCUMENT_PATH);
        return;
    }

    panel_ = document_->GetElementById("dialogue-box-panel");
    portrait_frame_ = document_->GetElementById("dialogue-box-portrait-frame");
    portrait_ = document_->GetElementById("dialogue-box-portrait");
    speaker_element_ = document_->GetElementById("dialogue-box-speaker");
    text_element_ = document_->GetElementById("dialogue-box-text");
    continue_element_ = document_->GetElementById("dialogue-box-continue");
    if (!isReady()) {
        spdlog::error("DialogueBoxView: RML 元素缺失。");
        runtime_->unloadDocument(document_);
        document_ = nullptr;
        panel_ = nullptr;
        portrait_frame_ = nullptr;
        portrait_ = nullptr;
        speaker_element_ = nullptr;
        text_element_ = nullptr;
        continue_element_ = nullptr;
        return;
    }

    runtime_->hideDocument(document_);
}

void DialogueBoxView::setSpeaker(std::string_view speaker) {
    speaker_ = std::string(speaker);
    if (speaker_element_) {
        speaker_element_->SetInnerRML(textToInnerRml(speaker_));
        setHidden(speaker_element_, speaker_.empty());
    }
}

void DialogueBoxView::setText(std::string_view text) {
    text_ = std::string(text);
    if (text_element_) {
        text_element_->SetInnerRML(textToInnerRml(text_));
    }
    setHidden(continue_element_, text_.empty());
}

void DialogueBoxView::setPortraitDecorator(std::string_view decorator) {
    portrait_decorator_ = decorator.empty() ? std::string{NONE_DECORATOR} : std::string{decorator};
    const bool has_portrait = portrait_decorator_ != NONE_DECORATOR;
    setHidden(portrait_frame_, !has_portrait);
    if (!portrait_) {
        return;
    }
    if (has_portrait) {
        portrait_->SetProperty("decorator", portrait_decorator_);
    } else {
        portrait_->RemoveProperty("decorator");
    }
}

void DialogueBoxView::setVisible(bool visible) {
    visible_ = visible;
    if (!document_ || !runtime_) {
        return;
    }

    if (visible) {
        runtime_->showDocument(document_);
    } else {
        runtime_->hideDocument(document_);
    }
}

} // namespace game::ui
