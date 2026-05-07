#include "game/scene/battle_animation_director.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>

namespace game::scene {
namespace {

constexpr float PI = 3.14159265358979323846f;
constexpr float ATTACK_WINDUP_END = 0.08f;
constexpr float ATTACK_LUNGE_END = 0.22f;
constexpr float ATTACK_RETURN_END = 0.42f;
constexpr float HIT_FEEDBACK_END = 0.52f;
constexpr float KO_START = 0.36f;
constexpr float KO_ALPHA = 0.38f;
constexpr float KO_ROTATION_RADIANS = PI * 0.5f;
constexpr float MAX_ATTACK_STEP = 64.0f;
constexpr float MIN_ATTACK_STEP = 18.0f;
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

[[nodiscard]] bool hasTargetFeedback(const game::battle::BattleActionResult& result) {
    return result.target_id.has_value() &&
        (result.damage > 0 || result.hp_recovered > 0 || result.mp_recovered > 0 || !result.states_added.empty());
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
                                    const std::vector<BattleAnimationSpriteSnapshot>& sprites,
                                    const BattleAnimationTimelineConfig& config) {
    clearTransient();
    config_ = config;
    timeline_.result = result;
    timeline_.sprites.reserve(sprites.size());
    for (const auto& sprite : sprites) {
        timeline_.sprites.emplace(sprite.unit_id, sprite);
    }
    timeline_.duration_seconds = usesForwardActionPose()
        ? safeDuration(config_.attack_duration_seconds)
        : safeDuration(config_.action_hold_seconds);
    if (result.target_defeated) {
        timeline_.duration_seconds = std::max(timeline_.duration_seconds, safeDuration(config_.attack_duration_seconds));
        timeline_.duration_seconds = std::max(timeline_.duration_seconds, KO_START + 0.16f);
    } else if (hasTargetFeedback(result)) {
        timeline_.duration_seconds = std::max(timeline_.duration_seconds, HIT_FEEDBACK_END);
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
    if (const auto action_pose = usesForwardActionPose()
            ? attackPoseFor(unit_id)
            : simpleActionPoseFor(unit_id)) {
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
    if (elapsed <= ATTACK_WINDUP_END) {
        const float t = smoothstep01(elapsed / ATTACK_WINDUP_END);
        pose.offset = -forward * (0.20f * t);
        pose.scale_multiplier = 1.0f + 0.015f * t;
        return pose;
    }

    if (elapsed <= ATTACK_LUNGE_END) {
        const float t = smoothstep01((elapsed - ATTACK_WINDUP_END) / (ATTACK_LUNGE_END - ATTACK_WINDUP_END));
        pose.offset = glm::mix(-forward * 0.20f, forward, t);
        pose.offset.y -= std::sin(t * PI) * 8.0f;
        pose.scale_multiplier = 1.0f + 0.03f * std::sin(t * PI);
        return pose;
    }

    if (elapsed <= ATTACK_RETURN_END) {
        const float t = smoothstep01((elapsed - ATTACK_LUNGE_END) / (ATTACK_RETURN_END - ATTACK_LUNGE_END));
        pose.offset = glm::mix(forward, glm::vec2{0.0f, 0.0f}, t);
        return pose;
    }

    return std::nullopt;
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
        const auto actor_it = timeline_.sprites.find(unit_id);
        const float direction = actor_it != timeline_.sprites.end() &&
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
    if (timeline_.elapsed_seconds < ATTACK_LUNGE_END) {
        return std::nullopt;
    }
    const bool has_feedback = timeline_.result.damage > 0 || timeline_.result.hp_recovered > 0 ||
        timeline_.result.mp_recovered > 0 || !timeline_.result.states_added.empty();
    if (!has_feedback && !timeline_.result.target_defeated) {
        return std::nullopt;
    }

    BattleAnimationPose pose{};
    if (has_feedback && timeline_.elapsed_seconds <= HIT_FEEDBACK_END) {
        const float t = clamp01((timeline_.elapsed_seconds - ATTACK_LUNGE_END) / (HIT_FEEDBACK_END - ATTACK_LUNGE_END));
        const float decay = 1.0f - t;
        const float shake_direction = std::sin(t * PI * 10.0f) >= 0.0f ? 1.0f : -1.0f;
        pose.offset.x = shake_direction * decay * 7.0f;
        if (timeline_.result.hp_recovered > 0 || timeline_.result.mp_recovered > 0) {
            pose.color_multiplier = engine::utils::FColor{0.72f, 1.16f, 0.78f, 1.0f};
        } else {
            pose.color_multiplier = engine::utils::FColor{1.32f, 0.52f, 0.52f, 1.0f};
        }
    }

    if (timeline_.result.target_defeated && timeline_.elapsed_seconds >= KO_START) {
        const float ko_t = smoothstep01((timeline_.elapsed_seconds - KO_START) /
                                        (timeline_.duration_seconds - KO_START));
        pose.offset.y += 10.0f * ko_t;
        pose.rotation_radians += KO_ROTATION_RADIANS * ko_t;
        pose.color_multiplier.a *= 1.0f - (1.0f - KO_ALPHA) * ko_t;
    }
    return pose;
}

bool BattleAnimationDirector::usesForwardActionPose() const {
    if (timeline_.result.action_type == game::battle::BattleActionType::Attack) {
        return true;
    }
    if (timeline_.result.action_type != game::battle::BattleActionType::Skill || timeline_.result.damage <= 0) {
        return false;
    }

    const auto actor_it = timeline_.sprites.find(timeline_.result.actor_id);
    if (!timeline_.result.target_id || actor_it == timeline_.sprites.end()) {
        return false;
    }

    const auto target_it = timeline_.sprites.find(*timeline_.result.target_id);
    return target_it != timeline_.sprites.end() && actor_it->second.side != target_it->second.side;
}

glm::vec2 BattleAnimationDirector::forwardStepOffset() const {
    if (!hasValidSingleTarget()) {
        return glm::vec2{0.0f, 0.0f};
    }

    const auto actor_it = timeline_.sprites.find(timeline_.result.actor_id);
    const auto target_it = timeline_.sprites.find(*timeline_.result.target_id);
    if (actor_it == timeline_.sprites.end() || target_it == timeline_.sprites.end()) {
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
