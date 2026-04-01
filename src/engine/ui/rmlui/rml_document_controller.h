#pragma once

#include "engine/ui/rmlui/hover_focus_sync_listener.h"
#include "engine/ui/rmlui/rml_bind_helpers.h"
#include "engine/ui/rmlui/rml_data_bridge.h"

#include <RmlUi/Core/DataModelHandle.h>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace Rml {
class DataTypeRegister;
class Element;
class ElementDocument;
}

namespace engine::ui::rmlui {

class RmlUiRuntime;

class RmlDocumentController final {
public:
    RmlDocumentController() = default;

    void attach(RmlUiRuntime* runtime, uint64_t owner_scene_id);

    [[nodiscard]] Rml::DataModelConstructor createModel(std::string_view model_name,
                                                        Rml::DataTypeRegister* data_type_register = nullptr);

    template<typename Callback>
    bool bindEvent(Rml::DataModelConstructor& constructor, std::string_view name, Callback&& callback) {
        return constructor.BindEventCallback(
            Rml::String{name.data(), name.size()},
            std::forward<Callback>(callback));
    }

    template<typename T>
    bool bindEvent(Rml::DataModelConstructor& constructor,
                   std::string_view name,
                   typename Rml::DataModelConstructor::DataEventMemberFunc<T> member_func,
                   T* object_pointer) {
        return constructor.BindEventCallback(Rml::String{name.data(), name.size()}, member_func, object_pointer);
    }

    template<typename Callback>
    bool bindSimpleEvent(Rml::DataModelConstructor& constructor, std::string_view name, Callback&& callback) {
        return bindSimpleEventCallback(constructor, name, std::forward<Callback>(callback));
    }

    [[nodiscard]] Rml::ElementDocument* load(std::string_view document_path);
    void unload();

    void enableHoverFocusSync(HoverFocusSyncListener::CandidateFilter candidate_filter = {});
    void disableHoverFocusSync();

    void setDefaultFocusById(std::string_view element_id);
    void setDefaultFocusFirstEnabledByClass(std::string_view class_name);
    void clearDefaultFocus();
    void queueDefaultFocus();

    void queueFocusElement(Rml::Element* element);
    void queueFocusElementById(std::string_view element_id);
    void queueFocusFirstEnabledElementByClass(std::string_view class_name);

    void show();
    void hide();

    void markDirty(std::string_view variable_name);
    void markAllDirty();
    void destroyModel();

    [[nodiscard]] bool isModelValid() const { return data_bridge_.isValid(); }
    [[nodiscard]] Rml::ElementDocument* document() const { return document_; }
    [[nodiscard]] RmlUiRuntime* runtime() const { return runtime_; }

private:
    enum class DefaultFocusKind : std::uint8_t {
        None,
        ElementId,
        FirstEnabledElementByClass,
    };

    void applyHoverFocusSync();
    void clearHoverFocusListener();
    void unloadDocument();

    RmlUiRuntime* runtime_{nullptr};
    uint64_t owner_scene_id_{0};
    RmlDataBridge data_bridge_{};
    std::unique_ptr<HoverFocusSyncListener> hover_focus_listener_{};
    Rml::ElementDocument* document_{nullptr};
    HoverFocusSyncListener::CandidateFilter hover_focus_candidate_filter_{};
    bool hover_focus_sync_enabled_{false};
    DefaultFocusKind default_focus_kind_{DefaultFocusKind::None};
    std::string default_focus_token_{};
};

} // namespace engine::ui::rmlui
