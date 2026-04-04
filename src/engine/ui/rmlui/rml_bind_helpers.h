#pragma once

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

#include <string_view>
#include <utility>

namespace engine::ui::rmlui {

/// @brief 更新绑定的布尔值，仅在值发生变化时修改。
/// @param current 当前绑定值，若需要更新则会被修改。
/// @param next 待绑定的新值。
/// @return 若值发生变化返回 true，否则返回 false。
[[nodiscard]] inline bool updateBoundBool(bool& current, bool next) {
    if (current == next) {
        return false;
    }
    current = next;
    return true;
}

/// @brief 更新绑定的字符串值，仅在值发生变化时修改。
/// @param current 当前绑定字符串，若需要更新则会被修改。
/// @param next 待绑定的新字符串。
/// @return 若值发生变化返回 true，否则返回 false。
[[nodiscard]] inline bool updateBoundString(Rml::String& current, std::string_view next) {
    const Rml::String candidate{next.data(), next.size()};
    if (current == candidate) {
        return false;
    }
    current = candidate;
    return true;
}

/// @brief 向数据模型注册一个无参数的事件回调。
/// @tparam Callback 可调用类型，签名为 `void()`。
/// @param constructor 用于注册回调的数据模型构造器。
/// @param name 事件回调名称。
/// @param callback 事件触发时执行的回调函数。
/// @return 注册成功返回 true，否则返回 false。
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
