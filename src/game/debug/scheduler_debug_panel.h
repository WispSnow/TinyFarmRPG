#pragma once

#include "engine/debug/debug_panel.h"

namespace game::debug {

class SchedulerProfiler;

} // namespace game::debug

namespace game::runtime {

enum class GameMode;

} // namespace game::runtime

namespace game::debug {

class SchedulerDebugPanel final : public engine::debug::DebugPanel {
    SchedulerProfiler& profiler_;
    const game::runtime::GameMode* current_mode_{nullptr};
    int recent_frames_{30};

public:
    SchedulerDebugPanel(SchedulerProfiler& profiler, const game::runtime::GameMode* current_mode);

    [[nodiscard]] std::string_view name() const override;
    void draw(bool& is_open) override;
};

} // namespace game::debug
