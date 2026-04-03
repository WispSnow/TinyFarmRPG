#pragma once

#include "engine/ui/rmlui/rml_bind_helpers.h"
#include "engine/ui/rmlui/rml_data_bridge.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Event.h>

#include <concepts>
#include <cstdint>
#include <string_view>
#include <utility>

namespace Rml {
class DataTypeRegister;
class ElementDocument;
}

namespace engine::ui::rmlui {

class RmlUiRuntime;

/// @brief 单个 RmlUi 文档的生命周期与功能管理器。
///
/// 每个 UI 界面（Scene）持有一个 RmlDocumentController，负责将分散的
/// RmlUi 操作（文档加载/卸载、数据绑定、事件绑定）
/// 统一封装为面向游戏层的简洁接口。
///
/// **主要职责：**
/// - **文档生命周期**：通过 @ref load / @ref unload 加载和卸载 `.rml` 文档，
///   并统一托管文档实例生命周期。
/// - **数据模型**：通过 @ref createModel 创建 RmlUi 数据模型，并提供
///   @ref bindEvent / @ref bindSimpleEvent 快捷方法绑定事件回调；
///   通过 @ref markDirty / @ref markAllDirty 通知 RmlUi 刷新绑定变量。
///
/// **使用流程：**
/// 1. 调用 @ref attach 绑定 @ref RmlUiRuntime 和所属 Scene ID。
/// 2. （可选）调用 `createModel` 创建数据模型并绑定变量/事件。
/// 3. 调用 @ref load 加载文档。
/// 4. 场景卸载时调用 @ref unload 清理所有资源。
class RmlDocumentController final {
public:
    RmlDocumentController() = default;

    void attach(RmlUiRuntime* runtime, uint64_t owner_scene_id);

    [[nodiscard]] Rml::DataModelConstructor createModel(std::string_view model_name,
                                                        Rml::DataTypeRegister* data_type_register = nullptr);

    /// @brief 绑定完整签名的事件回调。
    /// @details 回调签名须为 `void(Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)`，
    ///          可访问事件对象、触发元素及参数列表。
    /// @param constructor 数据模型构造器。
    /// @param name 与 RML 中 `data-event-*="name"` 对应的事件 ID。
    /// @param callback 满足上述签名的可调用对象。
    template<typename Callback>
        requires std::invocable<Callback, Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&>
    bool bindEvent(Rml::DataModelConstructor& constructor, std::string_view name, Callback&& callback) {
        return constructor.BindEventCallback(
            Rml::String{name.data(), name.size()},
            std::forward<Callback>(callback));
    }

    /// @brief 绑定成员函数形式的事件回调（bindEvent 的成员函数重载）。
    /// @details 适用于将回调直接绑定到某个对象的成员函数，
    ///          成员函数签名须符合 `DataEventMemberFunc<T>` 要求。
    /// @param constructor 数据模型构造器。
    /// @param name 与 RML 中 `data-event-*="name"` 对应的事件 ID。
    /// @param member_func 目标成员函数指针。
    /// @param object_pointer 调用成员函数的对象指针。
    template<typename T>
    bool bindEvent(Rml::DataModelConstructor& constructor,
                   std::string_view name,
                   typename Rml::DataModelConstructor::DataEventMemberFunc<T> member_func,
                   T* object_pointer) {
        return constructor.BindEventCallback(Rml::String{name.data(), name.size()}, member_func, object_pointer);
    }

    /// @brief 绑定无参数的简化事件回调。
    /// @details 回调签名为 `void()`，内部自动丢弃 RmlUi 事件参数。
    ///          不需要访问事件信息时优先使用此方法，比 bindEvent 更简洁。
    /// @param constructor 数据模型构造器。
    /// @param name 与 RML 中 `data-event-*="name"` 对应的事件 ID。
    /// @param callback `void()` 签名的可调用对象。
    template<typename Callback>
        requires std::invocable<Callback>
    bool bindSimpleEvent(Rml::DataModelConstructor& constructor, std::string_view name, Callback&& callback) {
        return bindSimpleEventCallback(constructor, name, std::forward<Callback>(callback));
    }

    [[nodiscard]] Rml::ElementDocument* load(std::string_view document_path);
    void unload();

    void markDirty(std::string_view variable_name);
    void markAllDirty();

    [[nodiscard]] bool isModelValid() const { return data_bridge_.isValid(); }
    [[nodiscard]] Rml::ElementDocument* document() const { return document_; }
    [[nodiscard]] RmlUiRuntime* runtime() const { return runtime_; }

private:
    void unloadDocument();

    RmlUiRuntime* runtime_{nullptr};
    uint64_t owner_scene_id_{0};
    RmlDataBridge data_bridge_{};
    Rml::ElementDocument* document_{nullptr};
};

} // namespace engine::ui::rmlui
