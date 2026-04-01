#pragma once

#include <RmlUi/Core/EventListener.h>

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Rml {
class Element;
class Event;
}

namespace engine::ui::rmlui {

/**
 * @brief 将 RML 点击事件桥接为基于 data-command 的游戏命令回调。
 *
 * 作为 Rml::EventListener 注册到容器元素上，利用事件冒泡从触发节点向上查找最近的
 * data-command 属性，再分发到预注册的 C++ 回调。
 *
 * 适合标题菜单、暂停菜单、战斗菜单这类”简单按钮 -> 命令”的 UI。
 * 对于带参数、拖拽、强交互状态同步等复杂事件，优先直接使用
 * RmlUi DataModelConstructor::BindEventCallback。
 *
 * 同时管理注册的生命周期：registerTo() 记录注册信息，unregisterAll() 批量移除。
 * 必须在文档卸载前调用 unregisterAll()，否则卸载期间 RmlUi 可能回调已销毁的对象。
 *
 * 用法：
 *   bridge.on(“start_game”, [&](Rml::Event&) { ... });
 *   bridge.registerTo(document, “click”);
 *   // RML 侧: <button data-command=”start_game”>
 *   // 清理时：bridge.unregisterAll();
 */
class RmlEventBridge final : public Rml::EventListener {
public:
    using Callback = std::function<void(Rml::Event&)>;

    RmlEventBridge() = default;
    ~RmlEventBridge() override = default;

    RmlEventBridge(const RmlEventBridge&) = delete;
    RmlEventBridge& operator=(const RmlEventBridge&) = delete;

    /// 注册一个命名回调。名称对应 RML 中 data-command 属性的值。
    void on(std::string_view name, Callback callback);

    /// 将此 bridge 作为 EventListener 注册到元素的指定事件上，并记录注册信息。
    void registerTo(Rml::Element* element, std::string_view event_type);

    /// 移除所有通过 registerTo() 注册的事件监听器。必须在文档卸载前调用。
    void unregisterAll();

    /// Rml::EventListener 接口实现。
    void ProcessEvent(Rml::Event& event) override;

private:
    struct Registration {
        Rml::Element* element;
        std::string event_type;
    };

    std::unordered_map<std::string, Callback> callbacks_;
    std::vector<Registration> registrations_;
};

} // namespace engine::ui::rmlui
