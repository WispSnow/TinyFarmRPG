#include <gtest/gtest.h>

#include "engine/async/thread_pool.h"
#include "engine/system/deferred_commands.h"
#include "engine/system/parallel_wave_scheduler.h"
#include "engine/system/system_task_decl.h"
#include "engine/system/task_event_buffer.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <string>
#include <thread>
#include <vector>

namespace engine::system {
namespace {

constexpr entt::id_type RES_A = 1001;
constexpr entt::id_type RES_B = 1002;
constexpr entt::id_type RES_SYNC = 1003;

struct Marker final {};

TEST(ParallelWaveSchedulerTest, NoDependenciesShareSingleWave) {
    std::vector<SystemTaskDecl> tasks;
    tasks.push_back(SystemTaskDecl{
        .name = "TaskA",
        .policy = ExecutionPolicy::WorkerEligible,
        .run = [](DeferredCommands&, TaskEventBuffer&) {},
        .rw_resources = {RES_A}
    });
    tasks.push_back(SystemTaskDecl{
        .name = "TaskB",
        .policy = ExecutionPolicy::WorkerEligible,
        .run = [](DeferredCommands&, TaskEventBuffer&) {},
        .rw_resources = {RES_B}
    });

    ParallelWaveScheduler scheduler(std::move(tasks));
    ASSERT_TRUE(scheduler.valid());
    ASSERT_EQ(scheduler.waves().size(), 1U);
    ASSERT_EQ(scheduler.waves()[0].task_indices.size(), 2U);

    auto wave = scheduler.waves()[0].task_indices;
    std::sort(wave.begin(), wave.end());
    EXPECT_EQ(wave[0], 0U);
    EXPECT_EQ(wave[1], 1U);
}

TEST(ParallelWaveSchedulerTest, WriteReadDependencySplitsWaves) {
    std::vector<SystemTaskDecl> tasks;
    tasks.push_back(SystemTaskDecl{
        .name = "Producer",
        .policy = ExecutionPolicy::WorkerEligible,
        .run = [](DeferredCommands&, TaskEventBuffer&) {},
        .rw_resources = {RES_SYNC}
    });
    tasks.push_back(SystemTaskDecl{
        .name = "Consumer",
        .policy = ExecutionPolicy::WorkerEligible,
        .run = [](DeferredCommands&, TaskEventBuffer&) {},
        .ro_resources = {RES_SYNC}
    });

    ParallelWaveScheduler scheduler(std::move(tasks));
    ASSERT_TRUE(scheduler.valid());
    ASSERT_EQ(scheduler.waves().size(), 2U);
    ASSERT_EQ(scheduler.waves()[0].task_indices.size(), 1U);
    ASSERT_EQ(scheduler.waves()[1].task_indices.size(), 1U);
    EXPECT_EQ(scheduler.waves()[0].task_indices[0], 0U);
    EXPECT_EQ(scheduler.waves()[1].task_indices[0], 1U);
}

TEST(ParallelWaveSchedulerTest, SubmitFailureFallsBackToInlineExecution) {
    std::atomic<int> counter_a{0};
    std::atomic<int> counter_b{0};

    std::vector<SystemTaskDecl> tasks;
    tasks.push_back(SystemTaskDecl{
        .name = "TaskA",
        .policy = ExecutionPolicy::WorkerEligible,
        .run = [&](DeferredCommands&, TaskEventBuffer&) { counter_a.fetch_add(1, std::memory_order_relaxed); },
        .rw_resources = {RES_A}
    });
    tasks.push_back(SystemTaskDecl{
        .name = "TaskB",
        .policy = ExecutionPolicy::WorkerEligible,
        .run = [&](DeferredCommands&, TaskEventBuffer&) { counter_b.fetch_add(1, std::memory_order_relaxed); },
        .rw_resources = {RES_B}
    });

    engine::async::ThreadPool pool({.worker_count = 1, .queue_capacity = 1, .name = "ParallelWaveSchedulerFallbackTest"});
    pool.stop();

    entt::registry registry;
    ParallelWaveScheduler scheduler(std::move(tasks), &pool);
    ASSERT_TRUE(scheduler.valid());

    const auto elapsed = scheduler.execute(registry);
    ASSERT_EQ(elapsed.size(), 2U);
    EXPECT_GE(elapsed[0], 0.0);
    EXPECT_GE(elapsed[1], 0.0);
    EXPECT_EQ(counter_a.load(std::memory_order_relaxed), 1);
    EXPECT_EQ(counter_b.load(std::memory_order_relaxed), 1);
}

TEST(ParallelWaveSchedulerTest, WorkerEligibleWaveRunsOnMultipleWorkers) {
    std::atomic<int> entered{0};
    std::atomic<int> running{0};
    std::atomic<int> max_running{0};

    const auto task_body = [&](DeferredCommands&, TaskEventBuffer&) {
        entered.fetch_add(1, std::memory_order_relaxed);

        const int current = running.fetch_add(1, std::memory_order_relaxed) + 1;
        int observed = max_running.load(std::memory_order_relaxed);
        while (current > observed &&
               !max_running.compare_exchange_weak(observed, current, std::memory_order_relaxed)) {
        }

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
        while (entered.load(std::memory_order_relaxed) < 2 &&
               std::chrono::steady_clock::now() < deadline) {
            std::this_thread::yield();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        running.fetch_sub(1, std::memory_order_relaxed);
    };

    std::vector<SystemTaskDecl> tasks;
    tasks.push_back(SystemTaskDecl{
        .name = "TaskA",
        .policy = ExecutionPolicy::WorkerEligible,
        .run = task_body,
        .rw_resources = {RES_A}
    });
    tasks.push_back(SystemTaskDecl{
        .name = "TaskB",
        .policy = ExecutionPolicy::WorkerEligible,
        .run = task_body,
        .rw_resources = {RES_B}
    });

    engine::async::ThreadPool pool({.worker_count = 2, .queue_capacity = 8, .name = "ParallelWaveSchedulerLivePoolTest"});
    entt::registry registry;
    ParallelWaveScheduler scheduler(std::move(tasks), &pool);
    ASSERT_TRUE(scheduler.valid());

    const auto elapsed = scheduler.execute(registry);
    ASSERT_EQ(elapsed.size(), 2U);
    EXPECT_EQ(entered.load(std::memory_order_relaxed), 2);
    EXPECT_GE(max_running.load(std::memory_order_relaxed), 2);
}

TEST(ParallelWaveSchedulerTest, DeferredCommandsDrainBetweenWaves) {
    entt::registry registry;
    const entt::entity entity = registry.create();
    bool saw_marker = false;

    std::vector<SystemTaskDecl> tasks;
    tasks.push_back(SystemTaskDecl{
        .name = "WriteMarker",
        .policy = ExecutionPolicy::MainThreadOnly,
        .run = [entity](DeferredCommands& deferred, TaskEventBuffer&) {
            deferred.emplaceOrReplace<Marker>(entity);
        },
        .rw_resources = {RES_SYNC}
    });
    tasks.push_back(SystemTaskDecl{
        .name = "ReadMarker",
        .policy = ExecutionPolicy::MainThreadOnly,
        .run = [&](DeferredCommands&, TaskEventBuffer&) {
            saw_marker = registry.all_of<Marker>(entity);
        },
        .ro_resources = {RES_SYNC}
    });

    ParallelWaveScheduler scheduler(std::move(tasks));
    ASSERT_TRUE(scheduler.valid());
    ASSERT_EQ(scheduler.waves().size(), 2U);

    const auto elapsed = scheduler.execute(registry);
    ASSERT_EQ(elapsed.size(), 2U);
    EXPECT_TRUE(registry.all_of<Marker>(entity));
    EXPECT_TRUE(saw_marker);
}

TEST(ParallelWaveSchedulerTest, DeferredDrainHappensBeforeTaskEventFlush) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    const entt::entity entity = registry.create();
    bool saw_marker_during_flush = false;

    std::vector<SystemTaskDecl> tasks;
    tasks.push_back(SystemTaskDecl{
        .name = "WriteMarkerAndEvent",
        .policy = ExecutionPolicy::MainThreadOnly,
        .run = [&](DeferredCommands& deferred, TaskEventBuffer& task_events) {
            deferred.emplaceOrReplace<Marker>(entity);
            task_events.enqueueCommand([&](entt::dispatcher&) {
                saw_marker_during_flush = registry.all_of<Marker>(entity);
            });
        },
        .rw_resources = {RES_A}
    });

    ParallelWaveScheduler scheduler(std::move(tasks));
    ASSERT_TRUE(scheduler.valid());

    const auto elapsed = scheduler.execute(registry, &dispatcher);
    ASSERT_EQ(elapsed.size(), 1U);
    EXPECT_TRUE(saw_marker_during_flush);
}

TEST(ParallelWaveSchedulerTest, DotDumpContainsTaskNames) {
    std::vector<SystemTaskDecl> tasks;
    tasks.push_back(SystemTaskDecl{
        .name = "TaskX",
        .policy = ExecutionPolicy::MainThreadOnly,
        .run = [](DeferredCommands&, TaskEventBuffer&) {},
        .rw_resources = {RES_A}
    });
    tasks.push_back(SystemTaskDecl{
        .name = "TaskY",
        .policy = ExecutionPolicy::MainThreadOnly,
        .run = [](DeferredCommands&, TaskEventBuffer&) {},
        .rw_resources = {RES_B}
    });

    ParallelWaveScheduler scheduler(std::move(tasks));
    ASSERT_TRUE(scheduler.valid());

    const std::string dot = scheduler.dumpDot();
    EXPECT_NE(dot.find("TaskX"), std::string::npos);
    EXPECT_NE(dot.find("TaskY"), std::string::npos);
}

} // namespace
} // namespace engine::system
