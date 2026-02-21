#include "engine/async/main_thread_command_queue.h"

#include <algorithm>
#include <vector>

#include <spdlog/spdlog.h>

namespace engine::async {

MainThreadCommandQueue::MainThreadCommandQueue(std::size_t capacity)
    : owner_thread_id_(std::this_thread::get_id()),
      capacity_(std::max<std::size_t>(1, capacity)) {}

bool MainThreadCommandQueue::enqueue(Command command) {
    std::lock_guard lock(mutex_);
    if (!command || queue_.size() >= capacity_) {
        return false;
    }
    queue_.push_back(std::move(command));
    return true;
}

bool MainThreadCommandQueue::enqueueWithWait(Command command, std::chrono::milliseconds timeout) {
    if (!command) {
        return false;
    }

    std::unique_lock lock(mutex_);
    if (timeout <= std::chrono::milliseconds::zero()) {
        if (queue_.size() >= capacity_) {
            return false;
        }
    } else {
        const bool has_space = cv_not_full_.wait_for(lock, timeout, [this] {
            return queue_.size() < capacity_;
        });
        if (!has_space) {
            return false;
        }
    }

    queue_.push_back(std::move(command));
    return true;
}

std::size_t MainThreadCommandQueue::drain(std::size_t max_commands) {
    return drain(DrainPolicy{.max_commands = max_commands}).executed;
}

MainThreadCommandQueue::DrainResult MainThreadCommandQueue::drain(const DrainPolicy& policy) {
    DrainResult result{};
    if (!isOnOwnerThread()) {
        spdlog::warn("MainThreadCommandQueue::drain called from non-owner thread");
        return result;
    }

    if (policy.max_commands == 0) {
        std::lock_guard lock(mutex_);
        result.remaining = queue_.size();
        return result;
    }

    const auto start = std::chrono::steady_clock::now();
    const bool has_time_budget = policy.time_budget > std::chrono::microseconds::zero() &&
                                 policy.time_budget != std::chrono::microseconds::max();

    // 快速路径：无 time budget 时一次加锁批量搬运，减少锁争用。
    if (!has_time_budget) {
        std::vector<Command> commands{};
        {
            std::lock_guard lock(mutex_);
            const std::size_t drain_count = std::min(policy.max_commands, queue_.size());
            if (drain_count > 0) {
                commands.reserve(drain_count);
                for (std::size_t i = 0; i < drain_count; ++i) {
                    commands.push_back(std::move(queue_.front()));
                    queue_.pop_front();
                }
                result.remaining = queue_.size();
            } else {
                result.remaining = queue_.size();
            }
        }

        if (!commands.empty()) {
            cv_not_full_.notify_all();
        }
        for (auto& command : commands) {
            if (!command) {
                continue;
            }
            // 约束：提交到主线程命令队列的命令不得抛异常。
            command();
            ++result.executed;
        }
    } else {
        // 预算路径：逐条取出执行，以便在命令之间检查 time budget。
        while (result.executed < policy.max_commands) {
            if (result.executed > 0) {
                const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - start);
                if (elapsed >= policy.time_budget) {
                    result.budget_hit = true;
                    break;
                }
            }

            Command command{};
            bool has_command = false;
            {
                std::lock_guard lock(mutex_);
                if (!queue_.empty()) {
                    command = std::move(queue_.front());
                    queue_.pop_front();
                    has_command = true;
                }
            }
            if (has_command) {
                cv_not_full_.notify_one();
            }

            if (!has_command) {
                break;
            }
            if (command) {
                // 约束：提交到主线程命令队列的命令不得抛异常。
                command();
                ++result.executed;
            }
        }
        {
            std::lock_guard lock(mutex_);
            result.remaining = queue_.size();
        }
    }

    result.elapsed_us = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - start).count());
    if (result.remaining > 0 && !result.budget_hit) {
        result.budget_hit = result.executed >= policy.max_commands;
    }

    return result;
}

void MainThreadCommandQueue::clear() {
    std::lock_guard lock(mutex_);
    queue_.clear();
    cv_not_full_.notify_all();
}

std::size_t MainThreadCommandQueue::size() const {
    std::lock_guard lock(mutex_);
    return queue_.size();
}

bool MainThreadCommandQueue::empty() const {
    std::lock_guard lock(mutex_);
    return queue_.empty();
}

} // namespace engine::async
