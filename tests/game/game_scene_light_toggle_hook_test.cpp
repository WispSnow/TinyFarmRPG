// NOLINTBEGIN
#include <gtest/gtest.h>

#include "game/runtime/game_mode.h"
#include "game/runtime/system_scheduler.h"

#include <algorithm>
#include <vector>

namespace game::scene {
namespace {

[[nodiscard]] size_t indexOf(const std::vector<game::runtime::SchedulerStage>& stages,
                             game::runtime::SchedulerStage target) {
    const auto it = std::find(stages.begin(), stages.end(), target);
    EXPECT_NE(it, stages.end());
    if (it == stages.end()) {
        return stages.size();
    }
    return static_cast<size_t>(std::distance(stages.begin(), it));
}

TEST(GameSceneLightToggleHookTest, ExplorationProfileKeepsTransitionAndLightTogglePaired) {
    const auto& stages = game::runtime::SystemScheduler::profileStages(game::runtime::GameMode::Exploration);
    ASSERT_FALSE(stages.empty());

    const size_t pre_transition =
        indexOf(stages, game::runtime::SchedulerStage::TransitionUpdatePre);
    const size_t pre_light =
        indexOf(stages, game::runtime::SchedulerStage::LightTogglePre);
    const size_t post_transition =
        indexOf(stages, game::runtime::SchedulerStage::TransitionUpdatePost);
    const size_t post_light =
        indexOf(stages, game::runtime::SchedulerStage::LightTogglePost);

    ASSERT_LT(pre_transition, pre_light);
    ASSERT_LT(post_transition, post_light);
    EXPECT_EQ(pre_light, pre_transition + 1);
    EXPECT_EQ(post_light, post_transition + 1);
}

} // namespace
} // namespace game::scene
// NOLINTEND
