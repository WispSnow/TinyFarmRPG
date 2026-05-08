#include "game/scene/battle_victory_flow_controller.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace game::scene {
namespace {

constexpr float INTRO_SECONDS = 0.65f;
constexpr float GOLD_COUNT_SECONDS = 0.9f;
constexpr float MAX_UPDATE_DELTA_SECONDS = 0.25f;

[[nodiscard]] float clamp01(const float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

[[nodiscard]] float smoothstep01(const float value) {
    const float t = clamp01(value);
    return t * t * (3.0f - 2.0f * t);
}

[[nodiscard]] int countValue(const int target, const float elapsed_seconds) {
    if (target <= 0) {
        return 0;
    }
    const float t = smoothstep01(elapsed_seconds / GOLD_COUNT_SECONDS);
    return static_cast<int>(std::round(static_cast<float>(target) * t));
}

} // namespace

void BattleVictoryFlowController::begin(const game::battle::BattleRewardSummary& reward_summary) {
    reward_summary_ = reward_summary;
    gold_ = BattleVictoryCountValue{.target = std::max(0, reward_summary_.gold_total), .display = 0};
    phase_ = BattleVictoryFlowPhase::Intro;
    elapsed_seconds_ = 0.0f;
    phase_elapsed_seconds_ = 0.0f;
    finished_ = false;
}

void BattleVictoryFlowController::update(const float delta_time_seconds) {
    if (!active() || finished_) {
        return;
    }

    const float delta = std::clamp(delta_time_seconds, 0.0f, MAX_UPDATE_DELTA_SECONDS);
    elapsed_seconds_ += delta;
    phase_elapsed_seconds_ += delta;

    switch (phase_) {
        case BattleVictoryFlowPhase::Inactive:
        case BattleVictoryFlowPhase::WaitConfirm:
            return;
        case BattleVictoryFlowPhase::Intro:
            if (phase_elapsed_seconds_ >= INTRO_SECONDS) {
                phase_ = BattleVictoryFlowPhase::Rewards;
                phase_elapsed_seconds_ = 0.0f;
            }
            return;
        case BattleVictoryFlowPhase::Rewards:
            gold_.display = countValue(gold_.target, phase_elapsed_seconds_);
            if (phase_elapsed_seconds_ >= GOLD_COUNT_SECONDS) {
                skipToWaitConfirm();
            }
            return;
    }
}

void BattleVictoryFlowController::confirm() {
    if (!active() || finished_) {
        return;
    }
    if (phase_ == BattleVictoryFlowPhase::WaitConfirm) {
        finished_ = true;
        return;
    }
    skipToWaitConfirm();
}

void BattleVictoryFlowController::reset() {
    phase_ = BattleVictoryFlowPhase::Inactive;
    reward_summary_ = {};
    gold_ = {};
    elapsed_seconds_ = 0.0f;
    phase_elapsed_seconds_ = 0.0f;
    finished_ = false;
}

BattleVictoryFlowSnapshot BattleVictoryFlowController::snapshot() const {
    const float overlay_alpha = phase_ == BattleVictoryFlowPhase::Intro
        ? smoothstep01(phase_elapsed_seconds_ / INTRO_SECONDS)
        : (active() ? 1.0f : 0.0f);
    return BattleVictoryFlowSnapshot{
        .phase = phase_,
        .gold = gold_,
        .item_drops = reward_summary_.item_drops,
        .waiting_for_confirm = phase_ == BattleVictoryFlowPhase::WaitConfirm,
        .finished = finished_,
        .overlay_alpha = overlay_alpha,
        .elapsed_seconds = elapsed_seconds_
    };
}

void BattleVictoryFlowController::skipToWaitConfirm() {
    phase_ = BattleVictoryFlowPhase::WaitConfirm;
    phase_elapsed_seconds_ = INTRO_SECONDS;
    gold_.display = gold_.target;
}

} // namespace game::scene
