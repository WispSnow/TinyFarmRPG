// NOLINTBEGIN
#include <gtest/gtest.h>

#include "engine/async/main_thread_command_queue.h"
#include "engine/async/thread_pool.h"

#include <atomic>
#include <chrono>
#include <future>
#include <thread>

namespace engine::async {
namespace {

TEST(ThreadPoolTest, ExecutesSubmittedTasks) {
    ThreadPool pool({.worker_count = 2, .queue_capacity = 16, .name = "ThreadPoolTest"});

    std::atomic<int> counter{0};
    ASSERT_TRUE(pool.submit([&counter]() { counter.fetch_add(1, std::memory_order_relaxed); }));
    ASSERT_TRUE(pool.submit([&counter]() { counter.fetch_add(1, std::memory_order_relaxed); }));
    ASSERT_TRUE(pool.submit([&counter]() { counter.fetch_add(1, std::memory_order_relaxed); }));

    pool.waitForIdle();
    EXPECT_EQ(counter.load(std::memory_order_relaxed), 3);
}

TEST(ThreadPoolTest, QueueCapacityAppliesBackpressure) {
    ThreadPool pool({.worker_count = 1, .queue_capacity = 1, .name = "ThreadPoolCapacityTest"});

    std::promise<void> started_promise;
    auto started_future = started_promise.get_future();
    std::promise<void> block_promise;
    auto block_future = block_promise.get_future();

    ASSERT_TRUE(pool.submit([&started_promise, &block_future]() {
        started_promise.set_value();
        block_future.wait();
    }));
    if (started_future.wait_for(std::chrono::milliseconds(500)) != std::future_status::ready) {
        block_promise.set_value();
        FAIL() << "worker did not start in time";
    }

    const bool second_submitted = pool.submit([]() {}); // queue capacity = 1, this slot is now occupied
    if (!second_submitted) {
        block_promise.set_value();
        FAIL() << "expected second submit to fill the queue";
    }
    EXPECT_FALSE(pool.submit([]() {}));

    block_promise.set_value();
    pool.waitForIdle();
}

TEST(ThreadPoolTest, SubmitFutureReturnsResult) {
    ThreadPool pool({.worker_count = 1, .queue_capacity = 8, .name = "ThreadPoolFutureTest"});

    auto f = pool.submitFuture([](int a, int b) { return a + b; }, 10, 32);
    ASSERT_TRUE(f.valid());
    EXPECT_EQ(f.get(), 42);
}

TEST(ThreadPoolTest, RejectsNewTasksAfterStop) {
    ThreadPool pool({.worker_count = 1, .queue_capacity = 8, .name = "ThreadPoolStopTest"});
    pool.stop();
    EXPECT_FALSE(pool.submit([]() {}));
}

TEST(MainThreadCommandQueueTest, DrainExecutesCommands) {
    MainThreadCommandQueue queue(8);

    std::atomic<int> counter{0};
    ASSERT_TRUE(queue.enqueue([&counter]() { counter.fetch_add(1, std::memory_order_relaxed); }));
    ASSERT_TRUE(queue.enqueue([&counter]() { counter.fetch_add(1, std::memory_order_relaxed); }));
    ASSERT_TRUE(queue.enqueue([&counter]() { counter.fetch_add(1, std::memory_order_relaxed); }));

    EXPECT_EQ(queue.drain(), 3);
    EXPECT_EQ(counter.load(std::memory_order_relaxed), 3);
    EXPECT_TRUE(queue.empty());
}

TEST(MainThreadCommandQueueTest, CapacityLimitRejectsOverflow) {
    MainThreadCommandQueue queue(1);

    ASSERT_TRUE(queue.enqueue([]() {}));
    EXPECT_FALSE(queue.enqueue([]() {}));

    EXPECT_EQ(queue.drain(), 1);
}

} // namespace
} // namespace engine::async
// NOLINTEND
