#pragma once

#include "engine/async/work_queue.h"

#include <atomic>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <functional>
#include <future>
#include <string_view>
#include <thread>
#include <type_traits>
#include <vector>

namespace engine::async {

class ThreadPool final {
public:
    // 约束：任务函数不得抛异常（符合项目“无异常”规则）。
    using Task = std::function<void()>;

    struct Options {
        std::size_t worker_count{0};
        std::size_t queue_capacity{256};
        std::string_view name{"ThreadPool"};
    };

    ThreadPool();
    explicit ThreadPool(const Options& options);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    [[nodiscard]] bool submit(Task task, std::chrono::milliseconds wait_timeout = std::chrono::milliseconds::zero());

    template <typename Fn, typename... Args>
        requires std::invocable<Fn, Args...>
    [[nodiscard]] auto submitFuture(Fn&& fn, Args&&... args) -> std::future<std::invoke_result_t<Fn, Args...>> {
        using Result = std::invoke_result_t<Fn, Args...>;
        auto task = std::make_shared<std::packaged_task<Result()>>(
            [f = std::forward<Fn>(fn), ... captured = std::forward<Args>(args)]() mutable {
                return std::invoke(std::move(f), std::move(captured)...);
            });
        auto future = task->get_future();
        if (!submit([task]() { (*task)(); })) {
            return std::future<Result>{};
        }
        return future;
    }

    void stop();
    void waitForIdle();

    [[nodiscard]] std::size_t pendingTaskCount() const;
    [[nodiscard]] std::size_t workerCount() const noexcept { return workers_.size(); }
    [[nodiscard]] bool isStopping() const noexcept { return stopping_.load(std::memory_order_relaxed); }

private:
    void workerLoop(std::stop_token stop_token);

    WorkQueue<Task> queue_;
    std::vector<std::jthread> workers_{};
    std::atomic<bool> stopping_{false};
    std::atomic<std::size_t> active_tasks_{0};
    mutable std::mutex idle_mutex_{};
    std::condition_variable idle_cv_{};
};

} // namespace engine::async
