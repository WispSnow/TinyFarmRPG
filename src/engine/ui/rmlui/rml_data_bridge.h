#pragma once

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

#include <string>
#include <string_view>

namespace Rml {
class Context;
class DataModelConstructor;
class DataTypeRegister;
}

namespace engine::ui::rmlui {

/**
 * @brief 管理 RmlUi data model 的生命周期与脏标记。
 *
 * 典型用法：
 *   RmlDataBridge bridge;
 *   auto constructor = bridge.create(context, "time_clock");
 *   constructor.Bind("day", &model.day);
 *   constructor.Bind("time_text", &model.time_text);
 *   // 每帧数据变更时：
 *   bridge.markDirty("day");
 *
 * bridge 只负责 Create/RemoveDataModel、保存 handle、以及 dirty API；
 * 具体 Bind/RegisterStruct/BindEventCallback 仍直接通过返回的
 * DataModelConstructor 完成。
 */
class RmlDataBridge final {
public:
    RmlDataBridge() = default;
    ~RmlDataBridge();

    RmlDataBridge(const RmlDataBridge&) = delete;
    RmlDataBridge& operator=(const RmlDataBridge&) = delete;
    RmlDataBridge(RmlDataBridge&& other) noexcept;
    RmlDataBridge& operator=(RmlDataBridge&& other) noexcept;

    /// 创建一个命名 data model。返回 DataModelConstructor 供进一步绑定。
    /// @param context  RmlUi 上下文
    /// @param model_name 数据模型名称（对应 RML 中的 data-model="xxx"）
    [[nodiscard]] Rml::DataModelConstructor create(Rml::Context* context,
                                                std::string_view model_name,
                                                Rml::DataTypeRegister* data_type_register = nullptr);

    /// 标记某个变量为脏，触发 RmlUi 下一帧重新绑定该变量对应的 DOM。
    void markDirty(std::string_view variable_name);

    /// 标记所有变量为脏。
    void markAllDirty();

    /// 从 RmlUi Context 移除此 data model，释放名称以便重建。
    void destroy();

    /// data model 是否有效。
    [[nodiscard]] bool isValid() const { return valid_; }

    /// 获取底层句柄。
    [[nodiscard]] Rml::DataModelHandle& getHandle() { return handle_; }

private:
    Rml::DataModelHandle handle_;
    Rml::Context* context_{nullptr};
    std::string model_name_;
    bool valid_{false};
};

} // namespace engine::ui::rmlui
