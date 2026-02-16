#pragma once

#include "engine/debug/debug_panel.h"

namespace engine::core {
class Time;
}

namespace engine::debug {

class TimeDebugPanel final : public DebugPanel {
    engine::core::Time& time_;
    float time_scale_{1.0f};
    int target_fps_{0};

public:
    explicit TimeDebugPanel(engine::core::Time& time);

    std::string_view name() const override;
    void draw(bool& is_open) override;
    void onShow() override;

private:
    void syncFromTime();
};

} // namespace engine::debug
