#pragma once

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

#include <string_view>
#include <utility>

namespace engine::ui::rmlui {

[[nodiscard]] inline bool updateBoundBool(bool& current, bool next) {
    if (current == next) {
        return false;
    }
    current = next;
    return true;
}

[[nodiscard]] inline bool updateBoundString(Rml::String& current, std::string_view next) {
    const Rml::String candidate{next.data(), next.size()};
    if (current == candidate) {
        return false;
    }
    current = candidate;
    return true;
}

template<typename Callback>
[[nodiscard]] inline bool bindSimpleEventCallback(Rml::DataModelConstructor& constructor,
                                                  std::string_view name,
                                                  Callback&& callback) {
    return constructor.BindEventCallback(
        Rml::String{name.data(), name.size()},
        [callback = std::forward<Callback>(callback)](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            callback();
        });
}

} // namespace engine::ui::rmlui
