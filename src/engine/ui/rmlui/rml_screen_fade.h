#pragma once

#include "engine/ui/screen_fade_interface.h"

#include <cstdint>

namespace Rml {
class Element;
class ElementDocument;
}

namespace engine::ui::rmlui {

class RmlUiRuntime;

/**
 * @brief RmlUi 驱动的全屏淡入淡出。
 *
 * 通过直接操作 DOM 元素的 opacity 属性实现线性插值渐变。
 * 状态机与旧 UIScreenFade 完全一致：FadingOut→Holding→FadingIn→Idle。
 * 淡入淡出/黑屏期间 overlay 的 pointer-events 切换为 auto 以拦截鼠标点击，
 * 空闲时恢复为 none。body 始终保持 pointer-events: none，不阻挡 mouse motion。
 */
class RmlScreenFade final : public IScreenFade {
public:
    RmlScreenFade(RmlUiRuntime& runtime, uint64_t owner_scene_id);
    ~RmlScreenFade();

    RmlScreenFade(const RmlScreenFade&) = delete;
    RmlScreenFade& operator=(const RmlScreenFade&) = delete;

    void fadeOut(float seconds) override;
    void fadeIn(float seconds) override;
    [[nodiscard]] Phase phase() const override { return phase_; }

    void update(float delta_time);

private:
    void startFade(Phase next_phase, float target_alpha, float seconds);
    void applyOpacity();
    void setOverlayInteractive(bool interactive);

    RmlUiRuntime& runtime_;
    Rml::ElementDocument* document_{nullptr};
    Rml::Element* overlay_{nullptr};

    Phase phase_{Phase::Idle};
    float alpha_{0.0f};
    float from_alpha_{0.0f};
    float to_alpha_{0.0f};
    float duration_{0.0f};
    float timer_{0.0f};
};

} // namespace engine::ui::rmlui
