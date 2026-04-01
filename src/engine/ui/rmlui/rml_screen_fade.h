#pragma once

#include "engine/ui/screen_fade_interface.h"

#include <RmlUi/Core/EventListener.h>

#include <cstdint>

namespace Rml {
class Element;
class ElementDocument;
class Event;
}

namespace engine::ui::rmlui {

class RmlUiRuntime;

/**
 * @brief RmlUi 驱动的全屏淡入淡出。
 *
 * 通过 RmlUi transition 驱动 opacity 过渡。
 * 状态机与旧 UIScreenFade 完全一致：FadingOut→Holding→FadingIn→Idle。
 * 淡入淡出/黑屏期间 overlay 的 pointer-events 切换为 auto 以拦截鼠标点击，
 * 空闲时恢复为 none。body 始终保持 pointer-events: none，不阻挡 mouse motion。
 */
class RmlScreenFade final : public IScreenFade, public Rml::EventListener {
public:
    RmlScreenFade(RmlUiRuntime& runtime, uint64_t owner_scene_id);
    ~RmlScreenFade();

    RmlScreenFade(const RmlScreenFade&) = delete;
    RmlScreenFade& operator=(const RmlScreenFade&) = delete;

    void fadeOut(float seconds) override;
    void fadeIn(float seconds) override;
    [[nodiscard]] Phase phase() const override { return phase_; }

    void update(float delta_time);
    void ProcessEvent(Rml::Event& event) override;

private:
    void startFade(Phase next_phase, bool target_opaque, float seconds);
    void setOverlayOpaque(bool opaque);
    void setTransitionDuration(float seconds);
    void completeTransition();
    void setOverlayInteractive(bool interactive);
    void registerListeners();
    void unregisterListeners();

    RmlUiRuntime& runtime_;
    Rml::ElementDocument* document_{nullptr};
    Rml::Element* overlay_{nullptr};
    bool transition_listener_registered_{false};
    bool overlay_opaque_{false};

    Phase phase_{Phase::Idle};
};

} // namespace engine::ui::rmlui
