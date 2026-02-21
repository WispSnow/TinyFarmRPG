#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <mutex>
#include <thread>

namespace engine::async {

class MainThreadCommandQueue final {
public:
    // 约束：命令函数不得抛异常（符合项目“无异常”规则）。
    using Command = std::function<void()>;

    explicit MainThreadCommandQueue(std::size_t capacity = 4096);

    MainThreadCommandQueue(const MainThreadCommandQueue&) = delete;
    MainThreadCommandQueue& operator=(const MainThreadCommandQueue&) = delete;
    MainThreadCommandQueue(MainThreadCommandQueue&&) = delete;
    MainThreadCommandQueue& operator=(MainThreadCommandQueue&&) = delete;

    [[nodiscard]] bool enqueue(Command command);
    [[nodiscard]] bool enqueueWithWait(Command command, std::chrono::milliseconds timeout);
    [[nodiscard]] std::size_t drain(std::size_t max_commands = std::numeric_limits<std::size_t>::max());

    void clear();

    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] bool empty() const;
    [[nodiscard]] std::thread::id ownerThreadId() const noexcept { return owner_thread_id_; }
    [[nodiscard]] bool isOnOwnerThread() const noexcept { return std::this_thread::get_id() == owner_thread_id_; }

private:
    const std::thread::id owner_thread_id_;
    const std::size_t capacity_;
    mutable std::mutex mutex_{};
    std::condition_variable cv_not_full_{};
    std::deque<Command> queue_{};
};

} // namespace engine::async
