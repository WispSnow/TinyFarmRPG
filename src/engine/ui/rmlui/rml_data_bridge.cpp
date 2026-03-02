#include "engine/ui/rmlui/rml_data_bridge.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <spdlog/spdlog.h>

namespace engine::ui::rmlui {

Rml::DataModelConstructor RmlDataBridge::create(Rml::Context* context, std::string_view model_name) {
    if (!context) {
        spdlog::error("RmlDataBridge::create failed: context is null.");
        return {};
    }

    const Rml::String name{model_name.data(), model_name.size()};
    auto constructor = context->CreateDataModel(name);
    if (!constructor) {
        spdlog::error("RmlDataBridge::create failed for model '{}'.", model_name);
        return {};
    }

    handle_ = constructor.GetModelHandle();
    valid_ = true;
    spdlog::trace("RmlDataBridge created model '{}'.", model_name);
    return constructor;
}

void RmlDataBridge::markDirty(std::string_view variable_name) {
    if (!handle_) {
        return;
    }
    const Rml::String name{variable_name.data(), variable_name.size()};
    handle_.DirtyVariable(name);
}

void RmlDataBridge::markAllDirty() {
    if (!handle_) {
        return;
    }
    handle_.DirtyAllVariables();
}

} // namespace engine::ui::rmlui
