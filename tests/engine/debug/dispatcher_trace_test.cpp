// NOLINTBEGIN
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include <entt/signal/dispatcher.hpp>

#include "engine/debug/dispatcher_trace.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace engine::debug {
namespace {

[[nodiscard]] std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    EXPECT_TRUE(file.is_open()) << path;
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

struct TestImmediateEvent {};

struct TestQueuedEvent {
    explicit TestQueuedEvent(int value) : value(value) {}
    int value{0};
};

TEST(DispatcherTraceTest, RecordsRecentDispatchesWithTiming) {
    entt::dispatcher dispatcher;
    DispatcherTrace trace{/*max_dispatches=*/8};

    trace.watch<TestImmediateEvent>(dispatcher, "TestImmediateEvent");
    trace.watch<TestQueuedEvent>(dispatcher, "TestQueuedEvent");

    dispatcher.trigger<TestImmediateEvent>();
    dispatcher.enqueue<TestQueuedEvent>(123);

    {
        DispatcherTrace::ScopedQueueDispatch scoped(trace);
        dispatcher.update();
    }

    const auto recent = trace.recentDispatches(/*max_count=*/10);
    ASSERT_EQ(recent.size(), 2u);

    EXPECT_EQ(recent[0].event_name, "TestQueuedEvent");
    EXPECT_TRUE(recent[0].queued);

    EXPECT_EQ(recent[1].event_name, "TestImmediateEvent");
    EXPECT_FALSE(recent[1].queued);
}

TEST(DispatcherTraceTest, MaintainsFixedSizeDispatchHistory) {
    entt::dispatcher dispatcher;
    DispatcherTrace trace{/*max_dispatches=*/2};

    trace.watch<TestImmediateEvent>(dispatcher, "TestImmediateEvent");

    dispatcher.trigger<TestImmediateEvent>();
    dispatcher.trigger<TestImmediateEvent>();
    dispatcher.trigger<TestImmediateEvent>();

    const auto recent = trace.recentDispatches(/*max_count=*/10);
    ASSERT_EQ(recent.size(), 2u);
    EXPECT_EQ(recent[0].event_name, "TestImmediateEvent");
    EXPECT_EQ(recent[1].event_name, "TestImmediateEvent");
    EXPECT_GT(recent[0].sequence, recent[1].sequence);
}

TEST(DispatcherTraceTest, DoesNotExposeFrameBoundaryApi) {
    const std::filesystem::path trace_header_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/debug/dispatcher_trace.h").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(trace_header_path)) << trace_header_path;

    const std::string source = readTextFile(trace_header_path);
    ASSERT_FALSE(source.empty());

    EXPECT_EQ(source.find("beginFrame("), std::string::npos)
        << "DispatcherTrace should not expose frame boundary APIs when the debug panel shows recent dispatches.";
}

TEST(DispatcherTraceTest, DebugUIManagerDoesNotExposeFrameHookApi) {
    const std::filesystem::path ui_header_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/debug/debug_ui_manager.h").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(ui_header_path)) << ui_header_path;

    const std::string source = readTextFile(ui_header_path);
    ASSERT_FALSE(source.empty());

    EXPECT_EQ(source.find("onFrameBegin"), std::string::npos)
        << "DebugUIManager should not expose frame boundary APIs when DispatcherTrace is per-dispatch.";
}

} // namespace
} // namespace engine::debug
// NOLINTEND
