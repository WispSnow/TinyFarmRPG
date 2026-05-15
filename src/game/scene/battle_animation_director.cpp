#include "game/scene/battle_animation_director.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>

namespace game::scene {

void scaleAnimationTimeline(BattleAnimationTimelineConfig& config, float speed) noexcept {
    if (!std::isfinite(speed) || speed <= 0.0f || speed == 1.0f) {
        return;
    }
    const float inv = 1.0f / speed;
    config.attack_duration_seconds *= inv;
    config.action_hold_seconds *= inv;
    config.cast_duration_seconds *= inv;
    config.minimum_duration_seconds *= inv;
    config.duration_seconds *= inv;
    config.impact_time_seconds *= inv;
    config.hit_feedback_duration_seconds *= inv;
    config.weapon_windup_seconds *= inv;
    config.weapon_lunge_seconds *= inv;
    config.weapon_return_seconds *= inv;
}

namespace {

constexpr float PI = 3.14159265358979323846f;
constexpr float KO_START = 0.36f;
constexpr float KO_ALPHA = 0.38f;
constexpr float KO_ROTATION_RADIANS = PI * 0.5f;
constexpr float MAX_ATTACK_STEP = 64.0f;
constexpr float MIN_ATTACK_STEP = 18.0f;
constexpr float CAST_FORWARD_STEP = 14.0f;
constexpr float SELF_TARGET_EPSILON = 0.001f;

[[nodiscard]] float clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

[[nodiscard]] float smoothstep01(float value) {
    const float t = clamp01(value);
    return t * t * (3.0f - 2.0f * t);
}

[[nodiscard]] float safeDuration(float duration) {
    return std::max(duration, 0.001f);
}

[[nodiscard]] float safeImpactTime(const BattleAnimationTimelineConfig& config) {
    return std::max(0.0f, config.impact_time_seconds);
}

[[nodiscard]] float hitFeedbackEndTime(const BattleAnimationTimelineConfig& config) {
    return safeImpactTime(config) + std::max(config.hit_feedback_duration_seconds, 0.001f);
}

[[nodiscard]] bool hasTargetFeedback(const game::battle::BattleActionResult& result) {
    return result.target_id.has_value() &&
        (result.damage > 0 || !result.states_added.empty());
}

[[nodiscard]] engine::utils::FColor multiplyColor(const engine::utils::FColor& lhs,
                                                  const engine::utils::FColor& rhs) {
    return engine::utils::FColor{
        lhs.r * rhs.r,
        lhs.g * rhs.g,
        lhs.b * rhs.b,
        lhs.a * rhs.a};
}

[[nodiscard]] BattleAnimationPose mergePose(BattleAnimationPose base, const BattleAnimationPose& overlay) {
    base.offset += overlay.offset;
    base.scale_multiplier *= overlay.scale_multiplier;
    base.rotation_radians += overlay.rotation_radians;
    base.color_multiplier = multiplyColor(base.color_multiplier, overlay.color_multiplier);
    return base;
}

[[nodiscard]] bool isFinite(const glm::vec2 value) {
    return std::isfinite(value.x) && std::isfinite(value.y);
}

} // namespace

void BattleAnimationDirector::reset() {
    clearTransient();
    persistent_poses_.clear();
}

void BattleAnimationDirector::begin(const game::battle::BattleActionResult& result,
                                    const std::vector<BattlePresentationUnitAnchor>& unit_anchors,
                                    const BattleAnimationTimelineConfig& config) {
    clearTransient();
    config_ = config;
    timeline_.result = result;
    timeline_.unit_anchors.reserve(unit_anchors.size());
    for (const auto& anchor : unit_anchors) {
        timeline_.unit_anchors.emplace(anchor.unit_id, anchor);
    }
    switch (resolvedMotionStyle()) {
        case BattleActionMotionStyle::WeaponAttack:
            timeline_.duration_seconds = safeDuration(config_.attack_duration_seconds);
            break;
        case BattleActionMotionStyle::Cast:
            timeline_.duration_seconds = safeDuration(config_.cast_duration_seconds);
            break;
        case BattleActionMotionStyle::Guard:
        case BattleActionMotionStyle::Escape:
        case BattleActionMotionStyle::Simple:
        case BattleActionMotionStyle::Auto:
            timeline_.duration_seconds = safeDuration(config_.action_hold_seconds);
            break;
    }
    if (config_.duration_seconds > 0.0f) {
        timeline_.duration_seconds = safeDuration(config_.duration_seconds);
    }
    timeline_.duration_seconds = std::max(timeline_.duration_seconds, config_.minimum_duration_seconds);
    if (result.target_defeated) {
        timeline_.duration_seconds = std::max(timeline_.duration_seconds, safeDuration(config_.attack_duration_seconds));
        timeline_.duration_seconds = std::max(timeline_.duration_seconds, safeImpactTime(config_) + 0.30f);
    } else if (hasTargetFeedback(result)) {
        timeline_.duration_seconds = std::max(timeline_.duration_seconds, hitFeedbackEndTime(config_));
    }
    timeline_.active = true;
    active_ = true;

    if (timeline_.result.status == game::battle::BattleActionStatus::Rejected) {
        timeline_.duration_seconds = std::min(timeline_.duration_seconds, 0.18f);
    }
}

void BattleAnimationDirector::update(const float delta_time_seconds) {
    if (!active_) {
        return;
    }

    const float delta = std::clamp(delta_time_seconds, 0.0f, 0.25f);
    timeline_.elapsed_seconds = std::min(timeline_.elapsed_seconds + delta, timeline_.duration_seconds);
    if (timeline_.elapsed_seconds < timeline_.duration_seconds) {
        return;
    }

    persistDefeatedTarget();
    clearTransient();
}

bool BattleAnimationDirector::finished() const {
    return !active_;
}

std::optional<BattleAnimationPose> BattleAnimationDirector::poseFor(
    const game::battle::BattleUnitId unit_id) const {
    BattleAnimationPose result{};
    bool has_pose = false;
    if (const auto persistent = persistentPoseFor(unit_id)) {
        result = mergePose(result, *persistent);
        has_pose = true;
    }
    if (const auto transient = transientPoseFor(unit_id)) {
        result = mergePose(result, *transient);
        has_pose = true;
    }
    if (!has_pose) {
        return std::nullopt;
    }
    return result;
}

std::optional<BattleAnimationPose> BattleAnimationDirector::persistentPoseFor(
    const game::battle::BattleUnitId unit_id) const {
    const auto it = persistent_poses_.find(unit_id);
    if (it == persistent_poses_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<BattleAnimationPose> BattleAnimationDirector::transientPoseFor(
    const game::battle::BattleUnitId unit_id) const {
    if (!active_ || timeline_.result.status == game::battle::BattleActionStatus::Rejected) {
        return std::nullopt;
    }

    BattleAnimationPose result{};
    bool has_pose = false;
    if (timeline_.result.actor_id == unit_id &&
        (std::abs(config_.actor_start_offset.x) > SELF_TARGET_EPSILON ||
         std::abs(config_.actor_start_offset.y) > SELF_TARGET_EPSILON)) {
        result.offset += config_.actor_start_offset;
        has_pose = true;
    }

    std::optional<BattleAnimationPose> action_pose{};
    switch (resolvedMotionStyle()) {
        case BattleActionMotionStyle::WeaponAttack:
            action_pose = attackPoseFor(unit_id);
            break;
        case BattleActionMotionStyle::Cast:
            action_pose = castPoseFor(unit_id);
            break;
        case BattleActionMotionStyle::Guard:
        case BattleActionMotionStyle::Escape:
        case BattleActionMotionStyle::Simple:
        case BattleActionMotionStyle::Auto:
            action_pose = simpleActionPoseFor(unit_id);
            break;
    }
    if (action_pose) {
        result = mergePose(result, *action_pose);
        has_pose = true;
    }
    if (const auto hit_pose = hitPoseFor(unit_id)) {
        result = mergePose(result, *hit_pose);
        has_pose = true;
    }
    if (!has_pose) {
        return std::nullopt;
    }
    return result;
}

std::optional<BattleAnimationPose> BattleAnimationDirector::attackPoseFor(
    const game::battle::BattleUnitId unit_id) const {
    if (timeline_.result.actor_id != unit_id) {
        return std::nullopt;
    }

    const glm::vec2 forward = forwardStepOffset();
    BattleAnimationPose pose{};
    const float elapsed = timeline_.elapsed_seconds;

    const float impact_time = safeImpactTime(config_);
    const float windup_duration = std::min(std::max(config_.weapon_windup_seconds, 0.0f), impact_time * 0.5f);
    const float lunge_duration = std::min(std::max(config_.weapon_lunge_seconds, 0.001f),
                                          std::max(impact_time - windup_duration, 0.001f));
    const float windup_end = std::max(0.0f, impact_time - lunge_duration);
    const float return_end = impact_time + std::max(config_.weapon_return_seconds, 0.001f);

    if (elapsed <= windup_end) {
        const float t = windup_end > 0.001f ? smoothstep01(elapsed / windup_end) : 1.0f;
        pose.offset = -forward * (0.20f * t);
        pose.scale_multiplier = 1.0f + 0.015f * t;
        return pose;
    }

    if (elapsed <= impact_time) {
        const float t = smoothstep01((elapsed - windup_end) / safeDuration(impact_time - windup_end));
        pose.offset = glm::mix(-forward * 0.20f, forward, t);
        pose.offset.y -= std::sin(t * PI) * 8.0f;
        pose.scale_multiplier = 1.0f + 0.03f * std::sin(t * PI);
        return pose;
    }

    if (elapsed <= return_end) {
        const float t = smoothstep01((elapsed - impact_time) / safeDuration(return_end - impact_time));
        pose.offset = glm::mix(forward, glm::vec2{0.0f, 0.0f}, t);
        return pose;
    }

    return std::nullopt;
}

std::optional<BattleAnimationPose> BattleAnimationDirector::castPoseFor(
    const game::battle::BattleUnitId unit_id) const {
    if (timeline_.result.actor_id != unit_id) {
        return std::nullopt;
    }

    const float t = clamp01(timeline_.elapsed_seconds / safeDuration(timeline_.duration_seconds));
    float forward_amount = 0.0f;
    if (t <= 0.30f) {
        forward_amount = smoothstep01(t / 0.30f);
    } else if (t <= 0.76f) {
        forward_amount = 1.0f;
    } else {
        forward_amount = 1.0f - smoothstep01((t - 0.76f) / 0.24f);
    }

    const float pulse = std::sin(t * PI);
    BattleAnimationPose pose{};
    pose.offset = castForwardOffset() * forward_amount;
    pose.offset.y -= 2.0f * pulse;
    pose.scale_multiplier = 1.0f + 0.018f * pulse;
    pose.color_multiplier = engine::utils::FColor{1.08f, 1.05f, 1.18f, 1.0f};
    return pose;
}

std::optional<BattleAnimationPose> BattleAnimationDirector::simpleActionPoseFor(
    const game::battle::BattleUnitId unit_id) const {
    if (timeline_.result.actor_id != unit_id) {
        return std::nullopt;
    }

    const float t = clamp01(timeline_.elapsed_seconds / safeDuration(timeline_.duration_seconds));
    const float pulse = std::sin(t * PI);
    BattleAnimationPose pose{};
    pose.offset.y = -4.0f * pulse;
    pose.scale_multiplier = 1.0f + 0.035f * pulse;
    if (timeline_.result.action_type == game::battle::BattleActionType::Guard) {
        pose.offset.y = 3.0f * pulse;
        pose.color_multiplier = engine::utils::FColor{0.84f, 0.92f, 1.15f, 1.0f};
    } else if (timeline_.result.action_type == game::battle::BattleActionType::Escape) {
        const auto actor_it = timeline_.unit_anchors.find(unit_id);
        const float direction = actor_it != timeline_.unit_anchors.end() &&
                actor_it->second.side == game::battle::BattleSide::Player
            ? 1.0f
            : -1.0f;
        const float distance = timeline_.result.escape_succeeded ? 36.0f * t : 16.0f * pulse;
        pose.offset.x = direction * distance;
        pose.offset.y = 0.0f;
    }
    return pose;
}

std::optional<BattleAnimationPose> BattleAnimationDirector::hitPoseFor(
    const game::battle::BattleUnitId unit_id) const {
    if (!timeline_.result.target_id || *timeline_.result.target_id != unit_id) {
        return std::nullopt;
    }
    const float impact_time = safeImpactTime(config_);
    const float feedback_end = hitFeedbackEndTime(config_);
    if (timeline_.elapsed_seconds < impact_time) {
        return std::nullopt;
    }
    const bool has_feedback = timeline_.result.damage > 0 || !timeline_.result.states_added.empty();
    if (!has_feedback && !timeline_.result.target_defeated) {
        return std::nullopt;
    }

    BattleAnimationPose pose{};
    if (has_feedback && timeline_.elapsed_seconds <= feedback_end) {
        const float t = clamp01((timeline_.elapsed_seconds - impact_time) / safeDuration(feedback_end - impact_time));
        const float decay = 1.0f - t;
        const float shake_direction = std::sin(t * PI * 10.0f) >= 0.0f ? 1.0f : -1.0f;
        pose.offset.x = shake_direction * decay * 7.0f;
        pose.color_multiplier = engine::utils::FColor{1.32f, 0.52f, 0.52f, 1.0f};
    }

    const float ko_start = std::max(KO_START, impact_time);
    if (timeline_.result.target_defeated && timeline_.elapsed_seconds >= ko_start) {
        const float ko_t = smoothstep01((timeline_.elapsed_seconds - ko_start) /
                                        safeDuration(timeline_.duration_seconds - ko_start));
        pose.offset.y += 10.0f * ko_t;
        pose.rotation_radians += KO_ROTATION_RADIANS * ko_t;
        pose.color_multiplier.a *= 1.0f - (1.0f - KO_ALPHA) * ko_t;
    }
    return pose;
}

BattleActionMotionStyle BattleAnimationDirector::resolvedMotionStyle() const {
    if (config_.motion_style != BattleActionMotionStyle::Auto) {
        return config_.motion_style;
    }

    switch (timeline_.result.action_type) {
        case game::battle::BattleActionType::Attack:
            return BattleActionMotionStyle::WeaponAttack;
        case game::battle::BattleActionType::Guard:
            return BattleActionMotionStyle::Guard;
        case game::battle::BattleActionType::Escape:
            return BattleActionMotionStyle::Escape;
        case game::battle::BattleActionType::Skill: {
            if (timeline_.result.damage <= 0) {
                return BattleActionMotionStyle::Simple;
            }

            const auto actor_it = timeline_.unit_anchors.find(timeline_.result.actor_id);
            if (!timeline_.result.target_id || actor_it == timeline_.unit_anchors.end()) {
                return BattleActionMotionStyle::Simple;
            }

            const auto target_it = timeline_.unit_anchors.find(*timeline_.result.target_id);
            if (target_it != timeline_.unit_anchors.end() && actor_it->second.side != target_it->second.side) {
                return BattleActionMotionStyle::WeaponAttack;
            }
            return BattleActionMotionStyle::Simple;
        }
        case game::battle::BattleActionType::Item:
        case game::battle::BattleActionType::EndTurn:
            return BattleActionMotionStyle::Simple;
    }
    return BattleActionMotionStyle::Simple;
}

glm::vec2 BattleAnimationDirector::forwardStepOffset() const {
    if (!hasValidSingleTarget()) {
        return glm::vec2{0.0f, 0.0f};
    }

    const auto actor_it = timeline_.unit_anchors.find(timeline_.result.actor_id);
    const auto target_it = timeline_.unit_anchors.find(*timeline_.result.target_id);
    if (actor_it == timeline_.unit_anchors.end() || target_it == timeline_.unit_anchors.end()) {
        return glm::vec2{0.0f, 0.0f};
    }
    if (actor_it->first == target_it->first) {
        return glm::vec2{0.0f, 0.0f};
    }

    const glm::vec2 delta = target_it->second.base_screen_position - actor_it->second.base_screen_position;
    if (!isFinite(delta)) {
        return glm::vec2{0.0f, 0.0f};
    }

    const float distance = glm::length(delta);
    if (distance <= SELF_TARGET_EPSILON) {
        return glm::vec2{0.0f, 0.0f};
    }

    const float step = std::clamp(distance * 0.34f, MIN_ATTACK_STEP, MAX_ATTACK_STEP);
    return delta / distance * step;
}

glm::vec2 BattleAnimationDirector::castForwardOffset() const {
    const auto actor_it = timeline_.unit_anchors.find(timeline_.result.actor_id);
    if (actor_it == timeline_.unit_anchors.end()) {
        return glm::vec2{0.0f, 0.0f};
    }

    if (timeline_.result.target_id && *timeline_.result.target_id != timeline_.result.actor_id) {
        const auto target_it = timeline_.unit_anchors.find(*timeline_.result.target_id);
        if (target_it != timeline_.unit_anchors.end()) {
            const glm::vec2 delta = target_it->second.base_screen_position - actor_it->second.base_screen_position;
            if (isFinite(delta)) {
                const float distance = glm::length(delta);
                if (distance > SELF_TARGET_EPSILON) {
                    return delta / distance * CAST_FORWARD_STEP;
                }
            }
        }
    }

    const float direction = actor_it->second.side == game::battle::BattleSide::Player ? -1.0f : 1.0f;
    return glm::vec2{direction * CAST_FORWARD_STEP, 0.0f};
}

bool BattleAnimationDirector::hasValidSingleTarget() const {
    return timeline_.result.target_id.has_value();
}

void BattleAnimationDirector::clearTransient() {
    timeline_ = {};
    active_ = false;
}

void BattleAnimationDirector::persistDefeatedTarget() {
    if (!timeline_.result.target_defeated || !timeline_.result.target_id) {
        return;
    }

    persistent_poses_[*timeline_.result.target_id] = BattleAnimationPose{
        .offset = glm::vec2{0.0f, 10.0f},
        .scale_multiplier = 1.0f,
        .rotation_radians = KO_ROTATION_RADIANS,
        .color_multiplier = engine::utils::FColor{1.0f, 1.0f, 1.0f, KO_ALPHA}
    };
}

} // namespace game::scene
