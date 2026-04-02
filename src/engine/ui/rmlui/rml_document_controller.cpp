#include "engine/ui/rmlui/rml_document_controller.h"

#include "engine/ui/rmlui/rml_ui_runtime.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <spdlog/spdlog.h>

namespace engine::ui::rmlui {

void RmlDocumentController::attach(RmlUiRuntime* runtime, uint64_t owner_scene_id) {
    if (runtime_ == runtime && owner_scene_id_ == owner_scene_id) {
        return;
    }

    unload();
    runtime_ = runtime;
    owner_scene_id_ = owner_scene_id;
}

Rml::DataModelConstructor RmlDocumentController::createModel(std::string_view model_name,
                                                             Rml::DataTypeRegister* data_type_register) {
    if (!runtime_) {
        spdlog::error("RmlDocumentController::createModel failed: runtime is null.");
        return {};
    }

    return data_bridge_.create(runtime_->getContext(), model_name, data_type_register);
}

Rml::ElementDocument* RmlDocumentController::load(std::string_view document_path) {
    if (!runtime_) {
        spdlog::error("RmlDocumentController::load failed: runtime is null.");
        return nullptr;
    }

    unloadDocument();

    document_ = runtime_->loadDocument(document_path, owner_scene_id_);
    if (!document_) {
        return nullptr;
    }

    applyHoverFocusSync();
    queueDefaultFocus();
    return document_;
}

void RmlDocumentController::unload() {
    clearHoverFocusListener();
    unloadDocument();
    data_bridge_.destroy();
    default_focus_kind_ = DefaultFocusKind::None;
    default_focus_token_.clear();
    hover_focus_candidate_filter_ = {};
    hover_focus_sync_enabled_ = false;
}

void RmlDocumentController::enableHoverFocusSync(HoverFocusSyncListener::CandidateFilter candidate_filter) {
    hover_focus_sync_enabled_ = true;
    hover_focus_candidate_filter_ = std::move(candidate_filter);
    applyHoverFocusSync();
}

void RmlDocumentController::setDefaultFocusById(std::string_view element_id) {
    default_focus_kind_ = DefaultFocusKind::ElementId;
    default_focus_token_ = std::string(element_id);
    queueDefaultFocus();
}

void RmlDocumentController::setDefaultFocusFirstEnabledByClass(std::string_view class_name) {
    default_focus_kind_ = DefaultFocusKind::FirstEnabledElementByClass;
    default_focus_token_ = std::string(class_name);
    queueDefaultFocus();
}

void RmlDocumentController::queueDefaultFocus() {
    if (!runtime_ || !document_ || default_focus_token_.empty()) {
        return;
    }

    switch (default_focus_kind_) {
        case DefaultFocusKind::ElementId:
            runtime_->queueFocusElementById(document_, default_focus_token_);
            break;
        case DefaultFocusKind::FirstEnabledElementByClass:
            runtime_->queueFocusFirstEnabledElementByClass(document_, default_focus_token_);
            break;
        case DefaultFocusKind::None:
            break;
    }
}

void RmlDocumentController::queueFocusElement(Rml::Element* element) {
    if (!runtime_ || !element) {
        return;
    }
    runtime_->queueFocusElement(element);
}

void RmlDocumentController::queueFocusElementById(std::string_view element_id) {
    if (!runtime_ || !document_) {
        return;
    }
    runtime_->queueFocusElementById(document_, element_id);
}

void RmlDocumentController::queueFocusFirstEnabledElementByClass(std::string_view class_name) {
    if (!runtime_ || !document_) {
        return;
    }
    runtime_->queueFocusFirstEnabledElementByClass(document_, class_name);
}

void RmlDocumentController::markDirty(std::string_view variable_name) {
    data_bridge_.markDirty(variable_name);
}

void RmlDocumentController::markAllDirty() {
    data_bridge_.markAllDirty();
}

void RmlDocumentController::applyHoverFocusSync() {
    if (!hover_focus_sync_enabled_ || !runtime_ || !document_) {
        return;
    }

    clearHoverFocusListener();
    hover_focus_listener_ = std::make_unique<HoverFocusSyncListener>(*runtime_, hover_focus_candidate_filter_);
    hover_focus_listener_->registerTo(document_, "mouseover");
}

void RmlDocumentController::clearHoverFocusListener() {
    if (!hover_focus_listener_) {
        return;
    }

    hover_focus_listener_->unregisterAll();
    hover_focus_listener_.reset();
}

void RmlDocumentController::unloadDocument() {
    clearHoverFocusListener();

    if (!document_) {
        return;
    }

    if (runtime_) {
        runtime_->unloadDocument(document_);
    }
    document_ = nullptr;
}

} // namespace engine::ui::rmlui
