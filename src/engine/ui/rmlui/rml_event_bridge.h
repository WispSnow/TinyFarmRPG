#pragma once

#include <RmlUi/Core/EventListener.h>

#include <functional>
#include <string>
#include <unordered_map>

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
 * 适合标题菜单、暂停菜单、战斗菜单这类“简单按钮 -> 命令”的 UI。
 * 对于带参数、拖拽、强交互状态同步等复杂事件，优先直接使用
 * RmlUi DataModelConstructor::BindEventCallback。
 *
 * 用法：
 *   bridge.on("start_game", [&](Rml::Event&) { ... });
 *   element->AddEventListener("click", &bridge);
 *   // RML 侧: <button data-command="start_game">
 *
 * 或直接通过 registerTo() 便捷注册。
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

    /// 将此 bridge 作为 EventListener 注册到元素的指定事件上。
    void registerTo(Rml::Element* element, std::string_view event_type);

    /// Rml::EventListener 接口实现。
    void ProcessEvent(Rml::Event& event) override;

private:
    std::unordered_map<std::string, Callback> callbacks_;
};

} // namespace engine::ui::rmlui
