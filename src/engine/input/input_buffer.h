#pragma once

#include <SDL3/SDL.h>

#include <cstddef>
#include <vector>

namespace engine::input {

class InputBuffer final {
public:
    struct SnapshotEntry {
        Uint64 timestamp_ms{0};
        Uint64 age_ms{0};
    };

    static constexpr std::size_t DEFAULT_CAPACITY = 8;

    explicit InputBuffer(std::size_t capacity = DEFAULT_CAPACITY);

    void push(Uint64 timestamp_ms);
    [[nodiscard]] bool peek(Uint64 now_ms, Uint64 window_ms) const;
    bool consume(Uint64 now_ms, Uint64 window_ms);
    void clear();

    [[nodiscard]] std::vector<SnapshotEntry> snapshot(Uint64 now_ms) const;

private:
    [[nodiscard]] std::size_t physicalIndex(std::size_t logical_index) const;

    std::vector<Uint64> entries_{};
    std::size_t capacity_{DEFAULT_CAPACITY};
    std::size_t head_{0};
    std::size_t size_{0};
};

} // namespace engine::input
