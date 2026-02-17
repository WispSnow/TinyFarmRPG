#include "scheduler_profiler.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

#include <spdlog/spdlog.h>

namespace game::debug {
namespace {

[[nodiscard]] double elapsedMs(const std::chrono::steady_clock::time_point& begin,
                               const std::chrono::steady_clock::time_point& end) {
    return std::chrono::duration<double, std::milli>(end - begin).count();
}

[[nodiscard]] std::string formatStageSummary(const SchedulerProfiler::FrameSample& frame) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3);

    for (std::size_t i = 0; i < frame.stages.size(); ++i) {
        const auto& stage = frame.stages[i];
        if (i > 0U) {
            oss << ", ";
        }
        oss << game::runtime::toString(stage.stage) << "=" << stage.elapsed_ms << "ms";
    }

    return oss.str();
}

} // namespace

SchedulerProfiler::SchedulerProfiler(std::size_t max_frames)
    : frames_(std::max<std::size_t>(max_frames, 1U)) {
}

void SchedulerProfiler::clear() {
    for (auto& frame : frames_) {
        frame = FrameSample{};
    }
    frame_cursor_ = 0;
    frame_count_ = 0;
    frame_active_ = false;
    stage_active_ = false;
    working_frame_ = FrameSample{};
}

void SchedulerProfiler::beginFrame(game::runtime::GameMode mode) {
    if (!enabled_) {
        return;
    }

    frame_active_ = true;
    stage_active_ = false;
    current_stage_ = game::runtime::SchedulerStage::RemoveEntity;
    frame_started_at_ = Clock::now();
    stage_started_at_ = {};
    working_frame_ = FrameSample{};
    working_frame_.mode = mode;
}

void SchedulerProfiler::onStageStarted(game::runtime::SchedulerStage stage) {
    if (!enabled_ || !frame_active_) {
        return;
    }

    current_stage_ = stage;
    stage_started_at_ = Clock::now();
    stage_active_ = true;
}

void SchedulerProfiler::onStageCompleted(game::runtime::SchedulerStage stage) {
    if (!enabled_ || !frame_active_ || !stage_active_) {
        return;
    }

    const auto now = Clock::now();
    working_frame_.stages.push_back(StageSample{
        stage,
        elapsedMs(stage_started_at_, now)
    });
    stage_active_ = false;
}

void SchedulerProfiler::endFrame(const runtime::SystemScheduler::TickResult& result, bool emit_trace) {
    if (!enabled_ || !frame_active_) {
        return;
    }

    const auto now = Clock::now();
    if (stage_active_) {
        working_frame_.stages.push_back(StageSample{
            current_stage_,
            elapsedMs(stage_started_at_, now)
        });
        stage_active_ = false;
    }

    working_frame_.gate1_triggered = result.gate1_triggered;
    working_frame_.gate2_triggered = result.gate2_triggered;
    working_frame_.total_ms = elapsedMs(frame_started_at_, now);

    if (emit_trace && spdlog::should_log(spdlog::level::trace)) {
        spdlog::trace("SchedulerFrame mode={} gate1={} gate2={} total={:.3f}ms stages=[{}]",
                      game::runtime::toString(working_frame_.mode),
                      working_frame_.gate1_triggered,
                      working_frame_.gate2_triggered,
                      working_frame_.total_ms,
                      formatStageSummary(working_frame_));
    }

    pushFrame(std::move(working_frame_));
    working_frame_ = FrameSample{};
    frame_active_ = false;
}

const SchedulerProfiler::FrameSample* SchedulerProfiler::latestFrame() const {
    if (frame_count_ == 0U || frames_.empty()) {
        return nullptr;
    }

    const std::size_t index = (frame_cursor_ + frames_.size() - 1U) % frames_.size();
    return &frames_[index];
}

std::vector<SchedulerProfiler::FrameSample> SchedulerProfiler::recentFrames(std::size_t max_count) const {
    if (frame_count_ == 0U || max_count == 0U) {
        return {};
    }

    const std::size_t count = std::min(max_count, frame_count_);
    std::vector<FrameSample> out;
    out.reserve(count);

    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t index = (frame_cursor_ + frames_.size() - 1U - i) % frames_.size();
        out.push_back(frames_[index]);
    }

    return out;
}

std::vector<SchedulerProfiler::StageAggregate> SchedulerProfiler::aggregateRecent(std::size_t recent_frame_count) const {
    const auto recent = recentFrames(recent_frame_count);
    if (recent.empty()) {
        return {};
    }

    struct MutableAggregate {
        runtime::SchedulerStage stage{runtime::SchedulerStage::RemoveEntity};
        double total_ms{0.0};
        double max_ms{0.0};
        std::size_t samples{0};
    };

    std::vector<MutableAggregate> merged;

    for (auto frame_it = recent.rbegin(); frame_it != recent.rend(); ++frame_it) {
        for (const auto& stage : frame_it->stages) {
            auto it = std::find_if(
                merged.begin(),
                merged.end(),
                [&](const MutableAggregate& agg) { return agg.stage == stage.stage; });
            if (it == merged.end()) {
                merged.push_back(MutableAggregate{
                    stage.stage,
                    stage.elapsed_ms,
                    stage.elapsed_ms,
                    1U
                });
                continue;
            }

            it->total_ms += stage.elapsed_ms;
            it->max_ms = std::max(it->max_ms, stage.elapsed_ms);
            it->samples += 1U;
        }
    }

    std::vector<StageAggregate> out;
    out.reserve(merged.size());
    for (const auto& agg : merged) {
        out.push_back(StageAggregate{
            agg.stage,
            agg.samples > 0U ? (agg.total_ms / static_cast<double>(agg.samples)) : 0.0,
            agg.max_ms,
            agg.samples
        });
    }

    return out;
}

void SchedulerProfiler::pushFrame(FrameSample frame) {
    if (frames_.empty()) {
        return;
    }

    frames_[frame_cursor_] = std::move(frame);
    frame_cursor_ = (frame_cursor_ + 1U) % frames_.size();
    frame_count_ = std::min(frame_count_ + 1U, frames_.size());
}

} // namespace game::debug
