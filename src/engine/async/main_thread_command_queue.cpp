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
    if (!isOnOwnerThread()) {
        spdlog::warn("MainThreadCommandQueue::drain called from non-owner thread");
        return 0;
    }

    std::vector<Command> commands{};
    {
        std::lock_guard lock(mutex_);
        if (queue_.empty()) {
            return 0;
        }

        const std::size_t drain_count = std::min(max_commands, queue_.size());
        commands.reserve(drain_count);
        for (std::size_t i = 0; i < drain_count; ++i) {
            commands.push_back(std::move(queue_.front()));
            queue_.pop_front();
        }
        cv_not_full_.notify_all();
    }

    std::size_t executed = 0;
    for (auto& command : commands) {
        if (!command) {
            continue;
        }
        // 约束：提交到主线程命令队列的命令不得抛异常。
        command();
        ++executed;
    }
    return executed;
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
