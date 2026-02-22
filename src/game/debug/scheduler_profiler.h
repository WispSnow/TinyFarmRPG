#pragma once

#include "game/runtime/game_mode.h"
#include "game/runtime/system_scheduler.h"

#include <cstddef>
#include <vector>

namespace game::debug {

class SchedulerProfiler final {
public:
    struct StageSample {
        runtime::SchedulerStage stage{runtime::SchedulerStage::RemoveEntity};
        double elapsed_ms{0.0};
    };

    struct FrameSample {
        runtime::GameMode mode{runtime::GameMode::Exploration};
        bool gate1_triggered{false};
        bool gate2_triggered{false};
        double total_ms{0.0};
        std::vector<StageSample> stages{};
    };

    struct StageAggregate {
        runtime::SchedulerStage stage{runtime::SchedulerStage::RemoveEntity};
        double avg_ms{0.0};
        double max_ms{0.0};
        std::size_t samples{0};
    };

    explicit SchedulerProfiler(std::size_t max_frames = 120);

    void setEnabled(bool enabled) { enabled_ = enabled; }
    [[nodiscard]] bool isEnabled() const { return enabled_; }

    void clear();
    void captureFrame(runtime::GameMode mode, const runtime::SystemScheduler::TickResult& result, bool emit_trace);

    [[nodiscard]] const FrameSample* latestFrame() const;
    [[nodiscard]] std::vector<FrameSample> recentFrames(std::size_t max_count) const;
    [[nodiscard]] std::vector<StageAggregate> aggregateRecent(std::size_t recent_frame_count) const;

    [[nodiscard]] std::size_t frameCount() const { return frame_count_; }
    [[nodiscard]] std::size_t maxFrames() const { return frames_.size(); }

private:
    void pushFrame(FrameSample frame);

    bool enabled_{false};

    std::vector<FrameSample> frames_{};
    std::size_t frame_cursor_{0};
    std::size_t frame_count_{0};
};

} // namespace game::debug
