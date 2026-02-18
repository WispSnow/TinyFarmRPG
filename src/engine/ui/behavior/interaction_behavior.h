#pragma once
#include <cstdint>
#include <glm/vec2.hpp>

namespace engine::ui {
class UIInteractive;
enum class InteractionPhase : std::uint8_t;

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
    virtual void onStateChanged(UIInteractive& /*owner*/, InteractionPhase /*old_phase*/, InteractionPhase /*new_phase*/) {}

    virtual void onDragBegin(UIInteractive& /*owner*/, const glm::vec2& /*pos*/) {}
    virtual void onDragUpdate(UIInteractive& /*owner*/, const glm::vec2& /*pos*/, const glm::vec2& /*delta*/) {}
    virtual void onDragEnd(UIInteractive& /*owner*/, const glm::vec2& /*pos*/, bool /*accepted*/) {}
};

} // namespace engine::ui
