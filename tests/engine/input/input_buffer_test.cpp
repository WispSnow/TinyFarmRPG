// NOLINTBEGIN
#include <gtest/gtest.h>

#include "engine/input/input_buffer.h"

namespace engine::input {
namespace {

TEST(InputBufferTest, PeekConsumeAndCapacityWorkPerBuffer) {
    InputBuffer buffer(3);

    buffer.push(100);
    buffer.push(200);
    buffer.push(300);

    EXPECT_FALSE(buffer.peek(350, 40));
    EXPECT_TRUE(buffer.peek(350, 60));
    EXPECT_TRUE(buffer.consume(350, 60));
    EXPECT_FALSE(buffer.peek(350, 60));

    buffer.push(400);
    buffer.push(500);

    const auto snapshot = buffer.snapshot(500);
    ASSERT_EQ(snapshot.size(), 3U);
    EXPECT_EQ(snapshot[0].timestamp_ms, 200U);
    EXPECT_EQ(snapshot[1].timestamp_ms, 400U);
    EXPECT_EQ(snapshot[2].timestamp_ms, 500U);
}

TEST(InputBufferTest, ClearRemovesAllBufferedPresses) {
    InputBuffer buffer(4);
    buffer.push(10);
    buffer.push(20);

    EXPECT_TRUE(buffer.peek(20, 20));
    buffer.clear();
    EXPECT_FALSE(buffer.peek(20, 20));
    EXPECT_TRUE(buffer.snapshot(20).empty());
}

} // namespace
} // namespace engine::input
// NOLINTEND
