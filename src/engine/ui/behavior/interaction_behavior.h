#pragma once
#include <glm/vec2.hpp>

namespace engine::ui {
class UIInteractive;

/**
 * @brief 交互行为基类，可挂载到 UIInteractive 以扩展输入响应。
 */
class InteractionBehavior {
public:
    InteractionBehavior() = default;
    virtual ~InteractionBehavior() = default;

    InteractionBehavior(const InteractionBehavior&) = delete;
    InteractionBehavior& operator=(const InteractionBehavior&) = delete;
    InteractionBehavior(InteractionBehavior&&) = delete;
    InteractionBehavior& operator=(InteractionBehavior&&) = delete;

    virtual void onAttach(UIInteractive& /*owner*/) {}

    virtual void onHoverEnter(UIInteractive& /*owner*/) {}
    virtual void onHoverExit(UIInteractive& /*owner*/) {}
    virtual void onPressed(UIInteractive& /*owner*/) {}
    virtual void onReleased(UIInteractive& /*owner*/, bool /*inside*/) {}
    virtual void onClick(UIInteractive& /*owner*/) {}

    virtual void onDragBegin(UIInteractive& /*owner*/, const glm::vec2& /*pos*/) {}
    virtual void onDragUpdate(UIInteractive& /*owner*/, const glm::vec2& /*pos*/, const glm::vec2& /*delta*/) {}
    virtual void onDragEnd(UIInteractive& /*owner*/, const glm::vec2& /*pos*/, bool /*accepted*/) {}
};

} // namespace engine::ui
