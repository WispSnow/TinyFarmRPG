#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <stop_token>
#include <utility>

namespace engine::async {

template <typename T>
class WorkQueue final {
public:
    explicit WorkQueue(std::size_t capacity)
        : capacity_(capacity) {}

    [[nodiscard]] bool push(T value) {
        std::lock_guard lock(mutex_);
        if (closed_ || queue_.size() >= capacity_) {
            return false;
        }
        queue_.push_back(std::move(value));
        cv_not_empty_.notify_one();
        return true;
    }

    [[nodiscard]] bool pushWithWait(T value, std::chrono::milliseconds timeout) {
        std::unique_lock lock(mutex_);
        if (closed_) {
            return false;
        }
        if (timeout <= std::chrono::milliseconds::zero()) {
            if (queue_.size() >= capacity_) {
                return false;
            }
        } else {
            const bool can_push = cv_not_full_.wait_for(lock, timeout, [this] {
                return closed_ || queue_.size() < capacity_;
            });
            if (!can_push || closed_) {
                return false;
            }
        }

        queue_.push_back(std::move(value));
        cv_not_empty_.notify_one();
        return true;
    }

    [[nodiscard]] std::optional<T> pop(std::stop_token stop_token) {
        std::unique_lock lock(mutex_);
        cv_not_empty_.wait(lock, stop_token, [this] {
            return closed_ || !queue_.empty();
        });

        if (queue_.empty()) {
            return std::nullopt;
        }

        T value = std::move(queue_.front());
        queue_.pop_front();
        cv_not_full_.notify_one();
        return value;
    }

    void close() {
        std::lock_guard lock(mutex_);
        closed_ = true;
        cv_not_empty_.notify_all();
        cv_not_full_.notify_all();
    }

    [[nodiscard]] std::size_t size() const {
        std::lock_guard lock(mutex_);
        return queue_.size();
    }

    [[nodiscard]] bool empty() const {
        std::lock_guard lock(mutex_);
        return queue_.empty();
    }

    [[nodiscard]] bool isClosed() const {
        std::lock_guard lock(mutex_);
        return closed_;
    }

private:
    std::size_t capacity_{0};
    mutable std::mutex mutex_{};
    std::deque<T> queue_{};
    bool closed_{false};
    std::condition_variable_any cv_not_empty_{};
    std::condition_variable_any cv_not_full_{};
};

} // namespace engine::async
