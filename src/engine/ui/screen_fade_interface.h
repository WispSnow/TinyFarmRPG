#pragma once

#include <cstdint>

namespace engine::ui {

/**
 * @brief ScreenFade 抽象接口。
 *
 * 提供淡入淡出状态机契约，供 MapTransitionSystem 等消费者轮询使用。
 * 实现可基于旧 UIScreenFade（UIElement 层级树）或新 RmlScreenFade（RmlUi 文档）。
 */
class IScreenFade {
public:
    enum class Phase : std::uint8_t {
        Idle,
        FadingOut,
        Holding,
        FadingIn
    };

    virtual ~IScreenFade() = default;

    virtual void fadeOut(float seconds) = 0;
    virtual void fadeIn(float seconds) = 0;

    [[nodiscard]] virtual Phase phase() const = 0;
    [[nodiscard]] virtual bool isBusy() const { return phase() != Phase::Idle; }
};

} // namespace engine::ui
