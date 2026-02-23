#include <gtest/gtest.h>

#include "engine/system/task_event_buffer.h"

#include <entt/entity/fwd.hpp>
#include <entt/signal/dispatcher.hpp>

#include <atomic>
#include <thread>
#include <vector>

namespace engine::system {
namespace {

struct TestEvent final {
    int value{0};
};

struct EventCapture final {
    std::vector<int> values{};

    void onEvent(const TestEvent& event) {
        values.push_back(event.value);
    }
};

TEST(TaskEventBufferTest, FlushExecutesQueuedCommandsInOrder) {
    TaskEventBuffer buffer;
    entt::dispatcher dispatcher;
    std::vector<int> order;

    buffer.enqueueCommand([&](entt::dispatcher&) { order.push_back(1); });
    buffer.enqueueCommand([&](entt::dispatcher&) { order.push_back(2); });
    buffer.enqueueCommand([&](entt::dispatcher&) { order.push_back(3); });

    buffer.flushTo(dispatcher);
    EXPECT_EQ((std::vector<int>{1, 2, 3}), order);
    EXPECT_TRUE(buffer.empty());
}

TEST(TaskEventBufferTest, ConcurrentEnqueueFlushesAllCommands) {
    constexpr int kThreadCount = 8;
    constexpr int kCommandsPerThread = 32;

    TaskEventBuffer buffer;
    entt::dispatcher dispatcher;
    std::atomic<int> executed{0};

    std::vector<std::thread> threads;
    threads.reserve(kThreadCount);
    for (int thread_index = 0; thread_index < kThreadCount; ++thread_index) {
        threads.emplace_back([&]() {
            for (int i = 0; i < kCommandsPerThread; ++i) {
                buffer.enqueueCommand([&](entt::dispatcher&) {
                    executed.fetch_add(1, std::memory_order_relaxed);
                });
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    buffer.flushTo(dispatcher);
    EXPECT_EQ(executed.load(std::memory_order_relaxed), kThreadCount * kCommandsPerThread);
    EXPECT_TRUE(buffer.empty());
}

TEST(TaskEventBufferTest, EnqueueEventQueuesForDispatcherUpdate) {
    TaskEventBuffer buffer;
    entt::dispatcher dispatcher;
    EventCapture capture;
    dispatcher.sink<TestEvent>().connect<&EventCapture::onEvent>(&capture);

    buffer.enqueueEvent(TestEvent{.value = 11});
    buffer.enqueueEvent(TestEvent{.value = 29});

    buffer.flushTo(dispatcher);
    EXPECT_TRUE(capture.values.empty());

    dispatcher.update();
    ASSERT_EQ(capture.values.size(), 2U);
    EXPECT_EQ(capture.values[0], 11);
    EXPECT_EQ(capture.values[1], 29);
}

TEST(TaskEventBufferTest, EmptyFlushDoesNothing) {
    TaskEventBuffer buffer;
    entt::dispatcher dispatcher;

    EXPECT_TRUE(buffer.empty());
    buffer.flushTo(dispatcher);
    EXPECT_TRUE(buffer.empty());
}

} // namespace
} // namespace engine::system
