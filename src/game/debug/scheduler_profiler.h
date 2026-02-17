#pragma once

#include "game/runtime/game_mode.h"
#include "game/runtime/system_scheduler.h"

#include <chrono>
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

    void beginFrame(runtime::GameMode mode);
    void onStageStarted(runtime::SchedulerStage stage);
    void onStageCompleted(runtime::SchedulerStage stage);
    void endFrame(const runtime::SystemScheduler::TickResult& result, bool emit_trace);

    [[nodiscard]] const FrameSample* latestFrame() const;
    [[nodiscard]] std::vector<FrameSample> recentFrames(std::size_t max_count) const;
    [[nodiscard]] std::vector<StageAggregate> aggregateRecent(std::size_t recent_frame_count) const;

    [[nodiscard]] std::size_t frameCount() const { return frame_count_; }
    [[nodiscard]] std::size_t maxFrames() const { return frames_.size(); }

private:
    using Clock = std::chrono::steady_clock;

    void pushFrame(FrameSample frame);

    bool enabled_{false};
    bool frame_active_{false};
    bool stage_active_{false};

    runtime::SchedulerStage current_stage_{runtime::SchedulerStage::RemoveEntity};
    Clock::time_point frame_started_at_{};
    Clock::time_point stage_started_at_{};
    FrameSample working_frame_{};

    std::vector<FrameSample> frames_{};
    std::size_t frame_cursor_{0};
    std::size_t frame_count_{0};
};

} // namespace game::debug
