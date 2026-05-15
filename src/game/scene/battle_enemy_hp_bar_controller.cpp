#include "game/scene/battle_enemy_hp_bar_controller.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace game::scene {
namespace {

constexpr float MAX_UPDATE_DELTA_SECONDS = 0.25f;
constexpr float RATIO_EPSILON = 0.0001f;
constexpr float VISIBLE_ALPHA_EPSILON = 0.001f;

[[nodiscard]] float clamp01(const float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

[[nodiscard]] float hpRatio(const int hp, const int max_hp) {
    if (max_hp <= 0) {
        return 0.0f;
    }
    return clamp01(static_cast<float>(std::max(0, hp)) / static_cast<float>(max_hp));
}

[[nodiscard]] bool hasVisibleEnemyFeedback(const game::battle::BattleActionResult& result) {
    return result.damage > 0 || result.hp_recovered > 0 || result.mp_recovered > 0 || result.missed ||
        result.critical || result.target_defeated;
}

void approachRatio(BattleEnemyHpBarState& bar, const float delta_seconds, const float ratio_lerp_speed) {
    if (delta_seconds <= 0.0f) {
        return;
    }

    const float diff = bar.target_ratio - bar.display_ratio;
    if (std::abs(diff) <= RATIO_EPSILON) {
        bar.display_ratio = bar.target_ratio;
        return;
    }

    const float max_step = std::max(ratio_lerp_speed, 0.0f) * delta_seconds;
    if (std::abs(diff) <= max_step) {
        bar.display_ratio = bar.target_ratio;
    } else {
        bar.display_ratio += diff > 0.0f ? max_step : -max_step;
    }
    bar.display_ratio = clamp01(bar.display_ratio);
}

void updateVisibility(BattleEnemyHpBarState& bar,
                      const BattleEnemyHpBarConfig& config,
                      const float delta,
                      const bool enabled) {
    const bool had_reveal_time = bar.visible_seconds_remaining > 0.0f;
    if (bar.visible_seconds_remaining > 0.0f) {
        bar.visible_seconds_remaining = std::max(0.0f, bar.visible_seconds_remaining - delta);
    }

    if (enabled && (bar.highlighted || had_reveal_time || bar.visible_seconds_remaining > 0.0f)) {
        bar.alpha = 1.0f;
    } else {
        // disabled 时无条件淡出，忽略残留的 reveal 倒计时与 highlight 状态。
        const float fade_seconds = std::max(config.fade_seconds, 0.001f);
        bar.alpha = std::max(0.0f, bar.alpha - delta / fade_seconds);
    }

    bar.visible = enabled && (bar.highlighted || bar.visible_seconds_remaining > 0.0f)
                  || bar.alpha > VISIBLE_ALPHA_EPSILON;
}

} // namespace

BattleEnemyHpBarController::BattleEnemyHpBarController(BattleEnemyHpBarConfig config)
    : config_(config) {}

void BattleEnemyHpBarController::syncFromSnapshot(const game::battle::BattleSnapshot& snapshot) {
    std::vector<BattleEnemyHpBarState> next_bars;
    next_bars.reserve(snapshot.units.size());

    for (const auto& unit : snapshot.units) {
        if (unit.side != game::battle::BattleSide::Enemy) {
            continue;
        }

        BattleEnemyHpBarState bar{};
        if (const auto* existing = findBar(unit.id); existing) {
            bar = *existing;
        }
        syncBarFromUnit(bar, unit);
        next_bars.push_back(bar);
    }

    bars_ = std::move(next_bars);
}

void BattleEnemyHpBarController::revealFromResult(const game::battle::BattleActionResult& result) {
    if (!enabled_) {
        return;
    }
    if (result.status == game::battle::BattleActionStatus::Rejected || !result.target_id ||
        !hasVisibleEnemyFeedback(result)) {
        return;
    }

    auto* bar = findMutableBar(*result.target_id);
    if (!bar) {
        return;
    }

    bar->visible_seconds_remaining = std::max(config_.reveal_seconds, 0.0f);
    bar->alpha = 1.0f;
    bar->visible = true;
}

void BattleEnemyHpBarController::stageSnapshot(const game::battle::BattleSnapshot& snapshot) {
    staged_snapshot_ = snapshot;
}

void BattleEnemyHpBarController::applyStagedSnapshotAndReveal(const game::battle::BattleActionResult& result) {
    if (staged_snapshot_) {
        const game::battle::BattleSnapshot snapshot = *staged_snapshot_;
        staged_snapshot_.reset();
        syncFromSnapshot(snapshot);
    }
    revealFromResult(result);
}

void BattleEnemyHpBarController::setHighlightedTarget(const std::optional<game::battle::BattleUnitId> unit_id) {
    for (auto& bar : bars_) {
        bar.highlighted = enabled_ && unit_id.has_value() && bar.unit_id == *unit_id;
        if (bar.highlighted) {
            bar.alpha = 1.0f;
            bar.visible = true;
        }
    }
}

void BattleEnemyHpBarController::update(const float delta_time_seconds) {
    const float delta = std::clamp(delta_time_seconds, 0.0f, MAX_UPDATE_DELTA_SECONDS);
    for (auto& bar : bars_) {
        float ratio_delta = delta;
        if (bar.ratio_change_delay_seconds_remaining > 0.0f) {
            const float consumed = std::min(bar.ratio_change_delay_seconds_remaining, delta);
            bar.ratio_change_delay_seconds_remaining -= consumed;
            ratio_delta -= consumed;
        }

        approachRatio(bar, ratio_delta, config_.ratio_lerp_speed);
        updateVisibility(bar, config_, delta, enabled_);
    }
}

void BattleEnemyHpBarController::setEnabled(bool enabled) {
    if (enabled_ == enabled) {
        return;
    }
    enabled_ = enabled;
    if (!enabled_) {
        // 立即清空 reveal/highlight，让 update() 的 fade 分支可以从下一帧开始把 alpha 推向 0。
        for (auto& bar : bars_) {
            bar.visible_seconds_remaining = 0.0f;
            bar.highlighted = false;
        }
    }
}

void BattleEnemyHpBarController::clear() {
    bars_.clear();
    staged_snapshot_.reset();
}

const BattleEnemyHpBarState* BattleEnemyHpBarController::findBar(
    const game::battle::BattleUnitId unit_id) const {
    const auto it = std::find_if(bars_.begin(), bars_.end(), [unit_id](const auto& bar) {
        return bar.unit_id == unit_id;
    });
    return it == bars_.end() ? nullptr : &*it;
}

BattleEnemyHpBarState* BattleEnemyHpBarController::findMutableBar(
    const game::battle::BattleUnitId unit_id) {
    const auto it = std::find_if(bars_.begin(), bars_.end(), [unit_id](const auto& bar) {
        return bar.unit_id == unit_id;
    });
    return it == bars_.end() ? nullptr : &*it;
}

void BattleEnemyHpBarController::syncBarFromUnit(BattleEnemyHpBarState& bar,
                                                const game::battle::BattleUnit& unit) {
    const float next_ratio = hpRatio(unit.hp, unit.max_hp);
    const bool was_initialized = bar.initialized;

    bar.unit_id = unit.id;
    bar.hp = std::max(0, unit.hp);
    bar.max_hp = std::max(1, unit.max_hp);

    if (!was_initialized) {
        bar.target_ratio = next_ratio;
        bar.display_ratio = next_ratio;
        bar.ratio_change_delay_seconds_remaining = 0.0f;
        bar.initialized = true;
        return;
    }

    if (std::abs(bar.target_ratio - next_ratio) > RATIO_EPSILON) {
        bar.target_ratio = next_ratio;
        bar.ratio_change_delay_seconds_remaining = std::max(config_.change_delay_seconds, 0.0f);
    }
}

} // namespace game::scene
