#include <gtest/gtest.h>

#include "game/scene/battle_cursor_memory.h"

namespace game::scene {
namespace {

TEST(BattleCursorMemoryTest, EnabledRemembersLastIndexWhenInRangeAndEnabled) {
    const std::vector<bool> enabled{true, true, true, true};
    EXPECT_EQ(resolveCursorMemoryDefaultIndex(2, enabled, 0, true), 2);
    EXPECT_EQ(resolveCursorMemoryDefaultIndex(3, enabled, 0, true), 3);
}

TEST(BattleCursorMemoryTest, DisabledAlwaysFallsBackToFallbackIndex) {
    const std::vector<bool> enabled{true, true, true, true};
    EXPECT_EQ(resolveCursorMemoryDefaultIndex(2, enabled, 0, false), 0);
    EXPECT_EQ(resolveCursorMemoryDefaultIndex(3, enabled, 1, false), 1);
}

TEST(BattleCursorMemoryTest, NegativeOrUnknownRememberedIndexFallsBack) {
    const std::vector<bool> enabled{true, true, true, true};
    EXPECT_EQ(resolveCursorMemoryDefaultIndex(-1, enabled, 0, true), 0);
    EXPECT_EQ(resolveCursorMemoryDefaultIndex(4, enabled, 1, true), 1);  // 越界
    EXPECT_EQ(resolveCursorMemoryDefaultIndex(99, enabled, 2, true), 2);
}

TEST(BattleCursorMemoryTest, DisabledEntryAtRememberedIndexFallsBack) {
    const std::vector<bool> enabled{true, false, true, false};
    // 上次记忆的下标 1 对应 disabled — 应当回退。
    EXPECT_EQ(resolveCursorMemoryDefaultIndex(1, enabled, 0, true), 0);
    EXPECT_EQ(resolveCursorMemoryDefaultIndex(3, enabled, 2, true), 2);
    // 仍然 enabled 的位置照常命中。
    EXPECT_EQ(resolveCursorMemoryDefaultIndex(2, enabled, 0, true), 2);
}

TEST(BattleCursorMemoryTest, EmptyEntriesAlwaysFallBack) {
    const std::vector<bool> enabled{};
    EXPECT_EQ(resolveCursorMemoryDefaultIndex(0, enabled, -1, true), -1);
    EXPECT_EQ(resolveCursorMemoryDefaultIndex(-1, enabled, -1, false), -1);
}

} // namespace
} // namespace game::scene
