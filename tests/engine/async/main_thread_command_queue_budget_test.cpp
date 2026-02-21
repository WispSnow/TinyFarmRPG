// NOLINTBEGIN
#include <gtest/gtest.h>

#include "engine/async/main_thread_command_queue.h"

#include <atomic>
#include <chrono>
#include <thread>

namespace engine::async {
namespace {

TEST(MainThreadCommandQueueBudgetTest, MaxCommandsBudgetLimitsExecution) {
    MainThreadCommandQueue queue(8);

    std::atomic<int> counter{0};
    for (int i = 0; i < 5; ++i) {
        ASSERT_TRUE(queue.enqueue([&counter]() {
            counter.fetch_add(1, std::memory_order_relaxed);
        }));
    }

    const auto result = queue.drain(MainThreadCommandQueue::DrainPolicy{
        .max_commands = 3,
        .time_budget = std::chrono::microseconds::max(),
    });

    EXPECT_EQ(result.executed, 3U);
    EXPECT_EQ(result.remaining, 2U);
    EXPECT_TRUE(result.budget_hit);
    EXPECT_EQ(counter.load(std::memory_order_relaxed), 3);
}

TEST(MainThreadCommandQueueBudgetTest, TimeBudgetStopsDraining) {
    MainThreadCommandQueue queue(8);

    std::atomic<int> counter{0};
    for (int i = 0; i < 3; ++i) {
        ASSERT_TRUE(queue.enqueue([&counter]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            counter.fetch_add(1, std::memory_order_relaxed);
        }));
    }

    const auto result = queue.drain(MainThreadCommandQueue::DrainPolicy{
        .max_commands = 8,
        .time_budget = std::chrono::microseconds(1200),
    });

    EXPECT_EQ(result.executed, 1U);
    EXPECT_EQ(result.remaining, 2U);
    EXPECT_TRUE(result.budget_hit);
    EXPECT_GE(result.elapsed_us, 1200U);
    EXPECT_EQ(counter.load(std::memory_order_relaxed), 1);
}

TEST(MainThreadCommandQueueBudgetTest, ZeroMaxCommandsDoesNotExecute) {
    MainThreadCommandQueue queue(8);

    std::atomic<int> counter{0};
    ASSERT_TRUE(queue.enqueue([&counter]() {
        counter.fetch_add(1, std::memory_order_relaxed);
    }));

    const auto result = queue.drain(MainThreadCommandQueue::DrainPolicy{
        .max_commands = 0,
        .time_budget = std::chrono::microseconds::max(),
    });

    EXPECT_EQ(result.executed, 0U);
    EXPECT_EQ(result.remaining, 1U);
    EXPECT_FALSE(result.budget_hit);
    EXPECT_EQ(counter.load(std::memory_order_relaxed), 0);
}

} // namespace
} // namespace engine::async
// NOLINTEND
