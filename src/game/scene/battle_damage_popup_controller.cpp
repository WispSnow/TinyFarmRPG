#include "game/scene/battle_damage_popup_controller.h"

#include <glm/common.hpp>

#include <algorithm>
#include <cmath>
#include <string>

namespace game::scene {
namespace {

constexpr float DAMAGE_RISE_DISTANCE = 36.0f;
constexpr float RECOVER_RISE_DISTANCE = 28.0f;
constexpr float MISS_RISE_DISTANCE = 20.0f;
constexpr float MISS_DRIFT_DISTANCE = 10.0f;
constexpr float CRITICAL_RISE_DISTANCE = 18.0f;
constexpr float POP_IN_SECONDS = 0.12f;
constexpr float DAMAGE_FADE_START = 0.65f;
constexpr float RECOVER_FADE_START = 0.60f;
constexpr float MISS_FADE_START = 0.55f;
constexpr float CRITICAL_FADE_START = 0.48f;

[[nodiscard]] float clamp01(const float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

[[nodiscard]] float smoothstep01(const float value) {
    const float t = clamp01(value);
    return t * t * (3.0f - 2.0f * t);
}

[[nodiscard]] float easeOutCubic(const float value) {
    const float t = clamp01(value);
    const float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}

[[nodiscard]] float safeDuration(const float duration_seconds) {
    return std::max(duration_seconds, 0.001f);
}

[[nodiscard]] float fadeAlpha(const float t, const float fade_start) {
    if (t <= fade_start) {
        return 1.0f;
    }
    return 1.0f - smoothstep01((t - fade_start) / (1.0f - fade_start));
}

[[nodiscard]] float popScale(const float active_seconds, const float duration_seconds, const float peak_scale) {
    const float pop_t = clamp01(active_seconds / POP_IN_SECONDS);
    if (pop_t < 1.0f) {
        return glm::mix(0.78f, peak_scale, smoothstep01(pop_t));
    }
    const float settle_t = clamp01((active_seconds - POP_IN_SECONDS) / safeDuration(duration_seconds - POP_IN_SECONDS));
    return glm::mix(peak_scale, 1.0f, smoothstep01(settle_t));
}

[[nodiscard]] float durationForKind(const BattleDamagePopupKind kind, const BattleDamagePopupTimingConfig& timing) {
    switch (kind) {
        case BattleDamagePopupKind::HpDamage:
            return timing.hp_damage_duration_seconds;
        case BattleDamagePopupKind::HpRecover:
        case BattleDamagePopupKind::MpRecover:
            return timing.recover_duration_seconds;
        case BattleDamagePopupKind::Miss:
            return timing.miss_duration_seconds;
        case BattleDamagePopupKind::Critical:
            return timing.critical_duration_seconds;
    }
    return timing.hp_damage_duration_seconds;
}

[[nodiscard]] bool hasNumericFeedback(const game::battle::BattleActionResult& result) {
    return result.damage > 0 || result.hp_recovered > 0 || result.mp_recovered > 0;
}

[[nodiscard]] glm::vec2 laneOffset(const int lane_index, const float lane_spacing_x) {
    if (lane_index == 0) {
        return glm::vec2{0.0f, 0.0f};
    }
    const float direction = lane_index % 2 == 0 ? 1.0f : -1.0f;
    const float step = static_cast<float>((lane_index + 1) / 2);
    return glm::vec2{direction * step * lane_spacing_x, 0.0f};
}

void updatePopupPose(BattleDamagePopup& popup) {
    const float active_seconds = popup.elapsed_seconds - popup.delay_seconds;
    if (active_seconds <= 0.0f) {
        popup.visible = false;
        popup.alpha = 0.0f;
        popup.offset = glm::vec2{0.0f, 0.0f};
        popup.scale = 1.0f;
        return;
    }

    const float duration = safeDuration(popup.duration_seconds);
    const float t = clamp01(active_seconds / duration);
    popup.visible = true;
    switch (popup.kind) {
        case BattleDamagePopupKind::HpDamage:
            popup.offset = glm::vec2{0.0f, -DAMAGE_RISE_DISTANCE * easeOutCubic(t)};
            popup.alpha = fadeAlpha(t, DAMAGE_FADE_START);
            popup.scale = popScale(active_seconds, duration, 1.24f);
            return;
        case BattleDamagePopupKind::HpRecover:
        case BattleDamagePopupKind::MpRecover:
            popup.offset = glm::vec2{0.0f, -RECOVER_RISE_DISTANCE * easeOutCubic(t)};
            popup.alpha = fadeAlpha(t, RECOVER_FADE_START);
            popup.scale = popScale(active_seconds, duration, 1.14f);
            return;
        case BattleDamagePopupKind::Miss:
            popup.offset = glm::vec2{MISS_DRIFT_DISTANCE * smoothstep01(t), -MISS_RISE_DISTANCE * easeOutCubic(t)};
            popup.alpha = fadeAlpha(t, MISS_FADE_START);
            popup.scale = 1.0f;
            return;
        case BattleDamagePopupKind::Critical:
            popup.offset = glm::vec2{0.0f, -CRITICAL_RISE_DISTANCE * easeOutCubic(t)};
            popup.alpha = fadeAlpha(t, CRITICAL_FADE_START);
            popup.scale = popScale(active_seconds, duration, 1.34f);
            return;
    }
}

} // namespace

BattleDamagePopupController::BattleDamagePopupController(BattleDamagePopupLayoutConfig layout,
                                                         BattleDamagePopupTimingConfig timing)
    : layout_(layout),
      timing_(timing) {}

void BattleDamagePopupController::spawnFromResult(
    const game::battle::BattleActionResult& result,
    const std::vector<BattlePresentationUnitAnchor>& unit_anchors) {
    spawnFromResult(result, unit_anchors, timing_.impact_delay_seconds);
}

void BattleDamagePopupController::spawnFromResult(
    const game::battle::BattleActionResult& result,
    const std::vector<BattlePresentationUnitAnchor>& unit_anchors,
    const float impact_time_seconds) {
    if (result.status == game::battle::BattleActionStatus::Rejected) {
        return;
    }

    const BattlePresentationUnitAnchor* target_anchor = nullptr;
    if (result.target_id) {
        target_anchor = findAnchor(unit_anchors, *result.target_id);
    }
    const BattlePresentationUnitAnchor* actor_anchor = findAnchor(unit_anchors, result.actor_id);
    const BattlePresentationUnitAnchor* popup_anchor = target_anchor ? target_anchor : actor_anchor;
    if (!popup_anchor) {
        return;
    }

    const glm::vec2 anchor_position = popup_anchor->base_screen_position +
        (target_anchor ? layout_.target_anchor_offset : layout_.actor_anchor_offset);
    const float impact_delay = std::max(impact_time_seconds, 0.0f);

    if (result.missed) {
        if (target_anchor) {
            spawnPopup("Miss", BattleDamagePopupKind::Miss, anchor_position, impact_delay, 0);
        }
        return;
    }

    if (!target_anchor && !hasNumericFeedback(result)) {
        return;
    }

    float numeric_delay = impact_delay;
    if (result.critical) {
        spawnPopup("Critical!", BattleDamagePopupKind::Critical, anchor_position, impact_delay, 0);
        numeric_delay += std::max(timing_.critical_number_stagger_seconds, 0.0f);
    }

    int numeric_lane_index = 0;
    if (result.damage > 0) {
        spawnPopup(std::to_string(result.damage),
                   BattleDamagePopupKind::HpDamage,
                   anchor_position,
                   numeric_delay,
                   numeric_lane_index++);
    }
    if (result.hp_recovered > 0) {
        spawnPopup("+" + std::to_string(result.hp_recovered),
                   BattleDamagePopupKind::HpRecover,
                   anchor_position,
                   numeric_delay,
                   numeric_lane_index++);
    }
    if (result.mp_recovered > 0) {
        spawnPopup("+" + std::to_string(result.mp_recovered),
                   BattleDamagePopupKind::MpRecover,
                   anchor_position,
                   numeric_delay,
                   numeric_lane_index++);
    }
}

void BattleDamagePopupController::update(const float delta_time_seconds) {
    const float delta = std::clamp(delta_time_seconds, 0.0f, std::max(timing_.max_delta_seconds, 0.0f));
    for (auto& popup : popups_) {
        popup.elapsed_seconds = std::min(popup.elapsed_seconds + delta,
                                         popup.delay_seconds + safeDuration(popup.duration_seconds));
        updatePopupPose(popup);
    }

    popups_.erase(
        std::remove_if(popups_.begin(), popups_.end(), [](const BattleDamagePopup& popup) {
            return popup.elapsed_seconds >= popup.delay_seconds + safeDuration(popup.duration_seconds);
        }),
        popups_.end());
}

void BattleDamagePopupController::clear() {
    popups_.clear();
}

const BattlePresentationUnitAnchor* BattleDamagePopupController::findAnchor(
    const std::vector<BattlePresentationUnitAnchor>& unit_anchors,
    const game::battle::BattleUnitId unit_id) const {
    const auto it = std::find_if(unit_anchors.begin(), unit_anchors.end(), [unit_id](const auto& anchor) {
        return anchor.unit_id == unit_id;
    });
    return it == unit_anchors.end() ? nullptr : &*it;
}

void BattleDamagePopupController::spawnPopup(std::string text,
                                             const BattleDamagePopupKind kind,
                                             const glm::vec2 anchor_position,
                                             const float delay_seconds,
                                             const int lane_index) {
    BattleDamagePopup popup{};
    popup.text = std::move(text);
    popup.kind = kind;
    popup.base_screen_position = anchor_position + laneOffset(lane_index, layout_.lane_spacing_x);
    popup.delay_seconds = std::max(delay_seconds, 0.0f);
    popup.duration_seconds = safeDuration(durationForKind(kind, timing_));
    popup.lane_index = lane_index;
    popup.alpha = 0.0f;
    popup.scale = 1.0f;
    popup.visible = false;
    popups_.push_back(std::move(popup));
}

engine::utils::FColor battleDamagePopupColor(const BattleDamagePopupKind kind, const float alpha) {
    const float a = clamp01(alpha);
    switch (kind) {
        case BattleDamagePopupKind::HpDamage:
            return engine::utils::FColor{1.0f, 0.18f, 0.16f, a};
        case BattleDamagePopupKind::HpRecover:
            return engine::utils::FColor{0.30f, 1.0f, 0.42f, a};
        case BattleDamagePopupKind::MpRecover:
            return engine::utils::FColor{0.32f, 0.62f, 1.0f, a};
        case BattleDamagePopupKind::Miss:
            return engine::utils::FColor{0.78f, 0.80f, 0.84f, a};
        case BattleDamagePopupKind::Critical:
            return engine::utils::FColor{1.0f, 0.86f, 0.22f, a};
    }
    return engine::utils::FColor{1.0f, 1.0f, 1.0f, a};
}

} // namespace game::scene
