#include "engine/async/thread_pool.h"

#include <algorithm>
#include <thread>

#include <spdlog/spdlog.h>

namespace engine::async {

namespace {
[[nodiscard]] std::size_t resolveWorkerCount(std::size_t requested) {
    if (requested > 0) {
        return requested;
    }
    const std::size_t hardware = std::max<std::size_t>(1, std::thread::hardware_concurrency());
    return hardware > 1 ? hardware - 1 : 1;
}
} // namespace

ThreadPool::ThreadPool(const Options& options)
    : queue_(std::max<std::size_t>(1, options.queue_capacity)) {
    const std::size_t worker_count = resolveWorkerCount(options.worker_count);
    workers_.reserve(worker_count);
    for (std::size_t i = 0; i < worker_count; ++i) {
        workers_.emplace_back([this](std::stop_token stop_token) {
            workerLoop(stop_token);
        });
    }
    spdlog::trace("ThreadPool '{}' started with {} workers, queue_capacity={}",
                  options.name,
                  worker_count,
                  std::max<std::size_t>(1, options.queue_capacity));
}

ThreadPool::ThreadPool()
    : ThreadPool(Options{}) {}

ThreadPool::~ThreadPool() {
    stop();
}

bool ThreadPool::submit(Task task, std::chrono::milliseconds wait_timeout) {
    if (!task || stopping_.load(std::memory_order_relaxed)) {
        return false;
    }
    if (wait_timeout <= std::chrono::milliseconds::zero()) {
        return queue_.push(std::move(task));
    }
    return queue_.pushWithWait(std::move(task), wait_timeout);
}

void ThreadPool::stop() {
    const bool was_stopping = stopping_.exchange(true, std::memory_order_relaxed);
    if (was_stopping) {
        return;
    }

    queue_.close();
    for (auto& worker : workers_) {
        worker.request_stop();
    }
    workers_.clear();
}

void ThreadPool::waitForIdle() {
    std::unique_lock lock(idle_mutex_);
    idle_cv_.wait(lock, [this] {
        return queue_.empty() && active_tasks_.load(std::memory_order_relaxed) == 0;
    });
}

std::size_t ThreadPool::pendingTaskCount() const {
    return queue_.size();
}

void ThreadPool::workerLoop(std::stop_token stop_token) {
    spdlog::info("worker started on thread {}", std::hash<std::thread::id>{}(std::this_thread::get_id()));
    while (!stop_token.stop_requested()) {
        auto task_opt = queue_.pop(stop_token);
        if (!task_opt.has_value()) {
            break;
        }

        active_tasks_.fetch_add(1, std::memory_order_relaxed);
        // 约束：本项目任务函数必须 noexcept 风格，不允许依赖异常控制流。
        (*task_opt)();
        active_tasks_.fetch_sub(1, std::memory_order_relaxed);

        {
            std::lock_guard lock(idle_mutex_);
            if (queue_.empty() && active_tasks_.load(std::memory_order_relaxed) == 0) {
                idle_cv_.notify_all();
            }
        }
    }
}

} // namespace engine::async
