// NOLINTBEGIN
#include <gtest/gtest.h>

#include "game/runtime/game_mode.h"
#include "game/runtime/system_scheduler.h"

#include <array>

namespace game::runtime {
namespace {

TEST(SystemSchedulerInvariantTest, RemoveEntityIsFirstStageForAllModes) {
    constexpr std::array<GameMode, 4> kModes{
        GameMode::Exploration,
        GameMode::Battle,
        GameMode::PauseOverlay,
        GameMode::Cutscene
    };

    for (const auto mode : kModes) {
        const auto& stages = SystemScheduler::profileStages(mode);
        ASSERT_FALSE(stages.empty());
        EXPECT_EQ(stages.front(), SchedulerStage::RemoveEntity);
    }
}

} // namespace
} // namespace game::runtime
// NOLINTEND
