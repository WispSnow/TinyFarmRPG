#include "engine/ui/rmlui/rml_document_controller.h"

#include "engine/ui/rmlui/rml_ui_runtime.h"

#include <RmlUi/Core/Context.h>
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

    return document_;
}

void RmlDocumentController::unload() {
    unloadDocument();
    data_bridge_.destroy();
}

void RmlDocumentController::markDirty(std::string_view variable_name) {
    data_bridge_.markDirty(variable_name);
}

void RmlDocumentController::markAllDirty() {
    data_bridge_.markAllDirty();
}

void RmlDocumentController::unloadDocument() {
    if (!document_) {
        return;
    }

    if (runtime_) {
        runtime_->unloadDocument(document_);
    }
    document_ = nullptr;
}

} // namespace engine::ui::rmlui
