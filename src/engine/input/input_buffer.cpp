#include "engine/input/input_buffer.h"

#include <algorithm>

namespace engine::input {

InputBuffer::InputBuffer(std::size_t capacity)
    : entries_(std::max<std::size_t>(capacity, 1), 0),
      capacity_(std::max<std::size_t>(capacity, 1)) {
}

void InputBuffer::push(Uint64 timestamp_ms) {
    if (size_ < capacity_) {
        entries_[physicalIndex(size_)] = timestamp_ms;
        ++size_;
        return;
    }

    entries_[head_] = timestamp_ms;
    head_ = (head_ + 1) % capacity_;
}

bool InputBuffer::peek(Uint64 now_ms, Uint64 window_ms) const {
    if (size_ == 0) {
        return false;
    }

    for (std::size_t i = 0; i < size_; ++i) {
        const Uint64 timestamp_ms = entries_[physicalIndex(i)];
        if (now_ms >= timestamp_ms && now_ms - timestamp_ms <= window_ms) {
            return true;
        }
    }

    return false;
}

bool InputBuffer::consume(Uint64 now_ms, Uint64 window_ms) {
    if (size_ == 0) {
        return false;
    }

    for (std::size_t i = 0; i < size_; ++i) {
        const std::size_t index = physicalIndex(i);
        const Uint64 timestamp_ms = entries_[index];
        if (now_ms >= timestamp_ms && now_ms - timestamp_ms <= window_ms) {
            for (std::size_t j = i + 1; j < size_; ++j) {
                entries_[physicalIndex(j - 1)] = entries_[physicalIndex(j)];
            }
            --size_;
            return true;
        }
    }

    return false;
}

void InputBuffer::clear() {
    head_ = 0;
    size_ = 0;
}

std::vector<InputBuffer::SnapshotEntry> InputBuffer::snapshot(Uint64 now_ms) const {
    std::vector<SnapshotEntry> result;
    result.reserve(size_);

    for (std::size_t i = 0; i < size_; ++i) {
        const Uint64 timestamp_ms = entries_[physicalIndex(i)];
        result.push_back(SnapshotEntry{
            .timestamp_ms = timestamp_ms,
            .age_ms = now_ms >= timestamp_ms ? now_ms - timestamp_ms : 0,
        });
    }

    return result;
}

std::size_t InputBuffer::physicalIndex(std::size_t logical_index) const {
    return (head_ + logical_index) % capacity_;
}

} // namespace engine::input
