#pragma once

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

#include <functional>
#include <string>
#include <string_view>

namespace Rml {
class Context;
class DataModelConstructor;
}

namespace engine::ui::rmlui {

/**
 * @brief 封装 RmlUi Data Model 绑定，简化游戏数据与 RML 文档的数据驱动关联。
 *
 * 典型用法：
 *   RmlDataBridge bridge;
 *   bridge.create(context, "time_clock");
 *   bridge.bind("day", &model.day);
 *   bridge.bind("time_text", &model.time_text);
 *   // 每帧数据变更时：
 *   bridge.markDirty("day");
 */
class RmlDataBridge final {
public:
    RmlDataBridge() = default;
    ~RmlDataBridge() = default;

    RmlDataBridge(const RmlDataBridge&) = delete;
    RmlDataBridge& operator=(const RmlDataBridge&) = delete;
    RmlDataBridge(RmlDataBridge&&) noexcept = default;
    RmlDataBridge& operator=(RmlDataBridge&&) noexcept = default;

    /// 创建一个命名 data model。返回 DataModelConstructor 供进一步绑定。
    /// @param context  RmlUi 上下文
    /// @param model_name 数据模型名称（对应 RML 中的 data-model="xxx"）
    [[nodiscard]] Rml::DataModelConstructor create(Rml::Context* context, std::string_view model_name);

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
