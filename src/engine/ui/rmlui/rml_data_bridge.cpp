#include "engine/ui/rmlui/rml_data_bridge.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <spdlog/spdlog.h>

#include <utility>

namespace engine::ui::rmlui {

RmlDataBridge::~RmlDataBridge() {
    destroy();
}

RmlDataBridge::RmlDataBridge(RmlDataBridge&& other) noexcept
    : handle_(std::exchange(other.handle_, {})),
      context_(std::exchange(other.context_, nullptr)),
      model_name_(std::move(other.model_name_)),
      valid_(std::exchange(other.valid_, false)) {
    other.model_name_.clear();
}

RmlDataBridge& RmlDataBridge::operator=(RmlDataBridge&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    destroy();

    handle_ = std::exchange(other.handle_, {});
    context_ = std::exchange(other.context_, nullptr);
    model_name_ = std::move(other.model_name_);
    valid_ = std::exchange(other.valid_, false);
    other.model_name_.clear();
    return *this;
}

Rml::DataModelConstructor RmlDataBridge::create(Rml::Context* context,
                                                 std::string_view model_name,
                                                 Rml::DataTypeRegister* data_type_register) {
    destroy();

    if (!context) {
        spdlog::error("RmlDataBridge::create failed: context is null.");
        return {};
    }

    const Rml::String name{model_name.data(), model_name.size()};
    auto constructor = context->CreateDataModel(name, data_type_register);
    if (!constructor) {
        spdlog::error("RmlDataBridge::create failed for model '{}'.", model_name);
        return {};
    }

    handle_ = constructor.GetModelHandle();
    valid_ = static_cast<bool>(handle_);
    context_ = context;
    model_name_ = std::string(model_name);
    spdlog::trace("RmlDataBridge created model '{}'.", model_name);
    return constructor;
}

void RmlDataBridge::destroy() {
    if (valid_ && context_ && !model_name_.empty()) {
        const Rml::String name{model_name_.data(), model_name_.size()};
        context_->RemoveDataModel(name);
        spdlog::trace("RmlDataBridge destroyed model '{}'.", model_name_);
    }
    handle_ = {};
    context_ = nullptr;
    model_name_.clear();
    valid_ = false;
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
