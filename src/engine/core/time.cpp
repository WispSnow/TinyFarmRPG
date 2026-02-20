#include "time.h"
#include <spdlog/spdlog.h>
#include <SDL3/SDL_timer.h>    // 用于 SDL_GetTicksNS()
#include <algorithm>

namespace engine::core {

Time::Time() {
    // 初始化 last_time_ 和 frame_start_time_ 为当前时间，避免第一帧 DeltaTime 过大
    last_time_ = SDL_GetTicksNS();
    frame_start_time_ = last_time_;
    spdlog::trace("Time 初始化。Last time: {}", last_time_);
}

void Time::update() {
    fixed_ticks_this_frame_ = 0;
    catch_up_clamped_this_frame_ = false;

    frame_start_time_ = SDL_GetTicksNS();   // 记录进入 update 时的时间戳
    const auto current_delta_time = static_cast<double>(frame_start_time_ - last_time_) / 1000000000.0;
    if (render_target_frame_time_ > 0.0) { // 如果设置了渲染目标帧率，则限制渲染帧率
        limitFrameRate(current_delta_time);
    } else {
        unscaled_delta_time_ = current_delta_time;
    }

    last_time_ = SDL_GetTicksNS(); // 记录离开 update 时的时间戳（作为下一帧基准）

    scaled_delta_time_ = unscaled_delta_time_ * time_scale_;
    accumulator_ += scaled_delta_time_;
}

void Time::limitFrameRate(double current_delta_time) {
    // 如果当前帧耗费的时间小于目标帧时间，则等待剩余时间
    if (current_delta_time < render_target_frame_time_) {
        const double time_to_wait = render_target_frame_time_ - current_delta_time;
        Uint64 ns_to_wait = static_cast<Uint64>(time_to_wait * 1000000000.0);
        SDL_DelayNS(ns_to_wait);
        unscaled_delta_time_ = static_cast<double>(SDL_GetTicksNS() - last_time_) / 1000000000.0;
        return;
    }
    unscaled_delta_time_ = current_delta_time;
}

float Time::getDeltaTime() const {
    return static_cast<float>(scaled_delta_time_);
}

float Time::getUnscaledDeltaTime() const {
    return static_cast<float>(unscaled_delta_time_);
}

void Time::setTimeScale(float scale) {
    if (scale < 0.0) {
        spdlog::warn("Time scale 不能为负。Clamping to 0.");
        scale = 0.0; // 防止负时间缩放
    }
    time_scale_ = scale;
}

float Time::getTimeScale() const {
    return time_scale_;
}

void Time::setTargetFps(int fps) {
    if (fps < 0) {
        spdlog::warn("Target FPS 不能为负。Setting to 0 (unlimited).");
        render_target_fps_ = 0;
    } else {
        render_target_fps_ = fps;
    }

    if (render_target_fps_ > 0) {
        render_target_frame_time_ = 1.0 / static_cast<double>(render_target_fps_);
        spdlog::info("Target FPS 设置为: {} (Frame time: {:.6f}s)", render_target_fps_, render_target_frame_time_);
    } else {
        render_target_frame_time_ = 0.0;
        spdlog::info("Target FPS 设置为: Unlimited");
    }
}

int Time::getTargetFps() const {
    return render_target_fps_;
}

void Time::setFixedDeltaTime(float delta_seconds) {
    if (!(delta_seconds > 0.0f)) {
        spdlog::warn("Fixed delta time 必须大于 0。忽略非法值: {}", delta_seconds);
        return;
    }
    fixed_delta_time_ = static_cast<double>(delta_seconds);
}

float Time::getFixedDeltaTime() const {
    return static_cast<float>(fixed_delta_time_);
}

void Time::setMaxTicksPerFrame(int max_ticks) {
    if (max_ticks < 1) {
        spdlog::warn("max_ticks_per_frame 不能小于 1。Clamping to 1.");
        max_ticks = 1;
    }
    max_ticks_per_frame_ = max_ticks;
}

int Time::getMaxTicksPerFrame() const {
    return max_ticks_per_frame_;
}

bool Time::tryConsumeFixedTick() {
    constexpr double kEpsilon = 1e-9;

    const auto clamp_excess_backlog = [this]() {
        const auto dropped_ticks = static_cast<Uint64>(accumulator_ / fixed_delta_time_);
        if (dropped_ticks > 0) {
            dropped_fixed_ticks_total_ += dropped_ticks;
            accumulator_ -= static_cast<double>(dropped_ticks) * fixed_delta_time_;
            if (accumulator_ < 0.0) {
                accumulator_ = 0.0;
            }
            catch_up_clamped_this_frame_ = true;
        }
    };

    if (fixed_ticks_this_frame_ >= max_ticks_per_frame_) {
        if (accumulator_ + kEpsilon >= fixed_delta_time_) {
            clamp_excess_backlog();
        }
        return false;
    }

    if (accumulator_ + kEpsilon < fixed_delta_time_) {
        return false;
    }

    accumulator_ -= fixed_delta_time_;
    if (accumulator_ < 0.0) {
        accumulator_ = 0.0;
    }
    ++fixed_ticks_this_frame_;
    return true;
}

float Time::getAccumulator() const {
    return static_cast<float>(accumulator_);
}

float Time::getInterpolationAlpha() const {
    if (fixed_delta_time_ <= 0.0) {
        return 0.0f;
    }
    const double alpha = accumulator_ / fixed_delta_time_;
    return static_cast<float>(std::clamp(alpha, 0.0, 1.0));
}

void Time::clearAccumulator() {
    accumulator_ = 0.0;
}

int Time::getFixedTicksThisFrame() const {
    return fixed_ticks_this_frame_;
}

bool Time::wasCatchUpClampedThisFrame() const {
    return catch_up_clamped_this_frame_;
}

Uint64 Time::getDroppedFixedTicksTotal() const {
    return dropped_fixed_ticks_total_;
}

} // namespace engine::core 
