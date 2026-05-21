#pragma once

#include "game/battle/battle_reward_resolver.h"
#include "game/domain/actor_progression_service.h"

#include <vector>

namespace game::scene {

/// @brief Victory 结算演出的阶段。
enum class BattleVictoryFlowPhase {
    Inactive,
    Intro,
    Rewards,
    WaitConfirm
};

/// @brief Victory overlay 中单个奖励数字的表现状态。
struct BattleVictoryCountValue {
    int target{0};
    int display{0};
};

/// @brief BattleVictoryFlowController 的只读快照。
struct BattleVictoryFlowSnapshot {
    BattleVictoryFlowPhase phase{BattleVictoryFlowPhase::Inactive};
    BattleVictoryCountValue gold{};
    BattleVictoryCountValue exp{};
    std::vector<game::battle::BattleRewardItemDrop> item_drops{};
    std::vector<game::domain::ActorExperienceGrant> level_ups{};
    bool waiting_for_confirm{false};
    bool finished{false};
    float overlay_alpha{0.0f};
    float elapsed_seconds{0.0f};
};

/// @brief 纯逻辑 Victory 流程控制器，负责阶段推进、奖励 count-up 与确认语义。
class BattleVictoryFlowController final {
public:
    BattleVictoryFlowController() = default;

    /// @brief 启动 Victory 演出并载入本场奖励摘要。
    void begin(const game::battle::BattleRewardSummary& reward_summary,
               game::domain::PartyExperienceGrantResult experience_preview = {});

    /// @brief 推进阶段与奖励数字动画。
    void update(float delta_time_seconds);

    /// @brief 确认键：演出中加速到等待确认，等待确认时结束流程。
    void confirm();

    /// @brief 重置为未激活状态。
    void reset();

    [[nodiscard]] bool active() const { return phase_ != BattleVictoryFlowPhase::Inactive; }
    [[nodiscard]] bool finished() const { return finished_; }
    [[nodiscard]] BattleVictoryFlowSnapshot snapshot() const;

private:
    void skipToWaitConfirm();

    BattleVictoryFlowPhase phase_{BattleVictoryFlowPhase::Inactive};
    game::battle::BattleRewardSummary reward_summary_{};
    game::domain::PartyExperienceGrantResult experience_preview_{};
    BattleVictoryCountValue gold_{};
    BattleVictoryCountValue exp_{};
    float elapsed_seconds_{0.0f};
    float phase_elapsed_seconds_{0.0f};
    bool finished_{false};
};

} // namespace game::scene
