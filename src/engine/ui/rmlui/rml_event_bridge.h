#pragma once

#include <RmlUi/Core/EventListener.h>

#include <entt/signal/fwd.hpp>

#include <functional>
#include <string>
#include <unordered_map>

namespace Rml {
class Element;
class Event;
}

namespace engine::ui::rmlui {

/**
 * @brief 将 RML 元素事件桥接到游戏侧回调。
 *
 * 作为 Rml::EventListener 注册到 RML 元素上，解析事件参数后调用预注册的 C++ 回调。
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
