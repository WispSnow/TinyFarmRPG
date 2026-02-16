#pragma once

#include <chrono>
#include <string>
#include <string_view>

#include <spdlog/spdlog.h>

namespace engine::utils {

class ScopedTimer final {
    using Clock = std::chrono::steady_clock;

public:
    explicit ScopedTimer(std::string_view label,
                         bool enabled = true,
                         spdlog::level::level_enum level = spdlog::level::debug)
        : enabled_(enabled), level_(level), label_(label), start_(Clock::now()) {}

    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;
    ScopedTimer(ScopedTimer&&) = delete;
    ScopedTimer& operator=(ScopedTimer&&) = delete;

    ~ScopedTimer() {
        if (!enabled_) {
            return;
        }
        const auto end = Clock::now();
        const auto ms = std::chrono::duration<double, std::milli>(end - start_).count();
        spdlog::log(level_, "{}: {:.3f} ms", label_, ms);
    }

    [[nodiscard]] double elapsedMs() const {
        const auto now = Clock::now();
        return std::chrono::duration<double, std::milli>(now - start_).count();
    }

private:
    bool enabled_{true};
    spdlog::level::level_enum level_{spdlog::level::debug};
    std::string label_{};
    Clock::time_point start_{};
};

} // namespace engine::utils

