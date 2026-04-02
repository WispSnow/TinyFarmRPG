#pragma once

#include <RmlUi/Core/EventListener.h>

#include <functional>
#include <string>
#include <vector>

namespace Rml {
class Element;
class Event;
}

namespace engine::ui::rmlui {

class RmlUiRuntime;

/// @brief 将鼠标悬停（hover）状态同步为 RmlUi 焦点（focus）的事件监听器。
///
/// RmlUi 中 `:hover` 与 `:focus` 是相互独立的伪类状态。在同时支持鼠标和
/// 手柄/键盘导航的场景下，需要保持两者同步：当鼠标悬停到某个按钮上时，
/// 该按钮应同时获得键盘焦点，以确保手柄确认键能作用于当前悬停项。
///
/// **工作原理：**
/// 1. 通过 @ref registerTo 将此 listener 注册到需要监听的元素（通常是容器）的
///    鼠标移动/进入事件上。
/// 2. 事件触发时，@ref ProcessEvent 从事件目标元素沿 DOM 树向上遍历，
///    查找第一个满足以下条件的 `<button>` 元素：可见、未禁用、通过可选的
///    @ref CandidateFilter 过滤。
/// 3. 找到符合条件的按钮后，调用 @ref RmlUiRuntime::focusElement 将焦点移至该按钮。
///
/// @note 使用完毕或文档卸载前，必须调用 @ref unregisterAll 移除所有注册，
///       否则会产生悬空指针。
class HoverFocusSyncListener final : public Rml::EventListener {
public:
    using CandidateFilter = std::function<bool(Rml::Element*)>;

    explicit HoverFocusSyncListener(RmlUiRuntime& runtime, CandidateFilter candidate_filter = {});

    void setCandidateFilter(CandidateFilter candidate_filter);

    /// 将此 listener 注册到元素的指定事件上，并记录注册信息。
    void registerTo(Rml::Element* element, std::string_view event_type);

    /// 移除所有通过 registerTo() 注册的事件监听器。必须在文档卸载前调用。
    void unregisterAll();

    void ProcessEvent(Rml::Event& event) override;

private:
    struct Registration {
        Rml::Element* element;
        std::string event_type;
    };

    RmlUiRuntime* runtime_{nullptr};
    CandidateFilter candidate_filter_{};
    std::vector<Registration> registrations_;
};

} // namespace engine::ui::rmlui
