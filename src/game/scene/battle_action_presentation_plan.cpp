#include "game/scene/battle_action_presentation_plan.h"

#include <entt/core/hashed_string.hpp>

#include <algorithm>
#include <cmath>
#include <string_view>

namespace game::scene {
namespace {

constexpr float DEFAULT_ATTACK_DURATION_SECONDS = 0.72F;
constexpr float DEFAULT_ATTACK_IMPACT_SECONDS = 0.22F;
constexpr float DEFAULT_CAST_DURATION_SECONDS = 0.92F;
constexpr float DEFAULT_CAST_IMPACT_RATIO = 0.55F;
constexpr float DEFAULT_SIMPLE_DURATION_SECONDS = 0.34F;
constexpr float DEFAULT_SIMPLE_IMPACT_SECONDS = 0.16F;
constexpr float DEFAULT_RECOVERY_SECONDS = 0.30F;
constexpr float DEFAULT_TARGET_VFX_TAIL_SECONDS = 0.45F;
constexpr float DEFAULT_POPUP_TAIL_SECONDS = 0.85F;
constexpr float PHYSICAL_HIT_DEFAULT_VFX_SCALE = 2.0F;
constexpr std::string_view PHYSICAL_HIT_DEFAULT_VFX_ID = "battle.hit_physical";
const glm::vec2 PHYSICAL_HIT_DEFAULT_VFX_OFFSET{0.0F, -18.0F};

[[nodiscard]] bool isRecoveryDamageType(const game::data::DamageType type) {
    return type == game::data::DamageType::HpRecover ||
        type == game::data::DamageType::MpRecover;
}

[[nodiscard]] bool isRecoverySkill(const game::data::SkillData& skill) {
    return isRecoveryDamageType(skill.damage_.type);
}

[[nodiscard]] bool isAppliedSingleTargetHit(const game::battle::BattleActionResult& result) {
    return result.status == game::battle::BattleActionStatus::Applied &&
        !result.missed &&
        result.target_id.has_value();
}

[[nodiscard]] bool hasVisibleEnemyHpFeedback(const game::battle::BattleActionResult& result) {
    return result.status == game::battle::BattleActionStatus::Applied &&
        result.target_id.has_value() &&
        (result.damage > 0 || result.hp_recovered > 0 || result.mp_recovered > 0 ||
         result.missed || result.critical || result.target_defeated);
}

[[nodiscard]] bool hasNumericPopupFeedback(const game::battle::BattleActionResult& result) {
    return result.status == game::battle::BattleActionStatus::Applied &&
        result.target_id.has_value() &&
        (result.damage > 0 || result.hp_recovered > 0 || result.mp_recovered > 0 ||
         result.missed || result.critical);
}

[[nodiscard]] const BattlePresentationUnitAnchor* findAnchor(
    const std::vector<BattlePresentationUnitAnchor>& anchors,
    const game::battle::BattleUnitId unit_id) {
    const auto it = std::find_if(anchors.begin(), anchors.end(), [unit_id](const auto& anchor) {
        return anchor.unit_id == unit_id;
    });
    return it == anchors.end() ? nullptr : &*it;
}

[[nodiscard]] BattleActionMotionStyle mapMotionStyle(const game::data::SkillMotionStyle style) {
    switch (style) {
        case game::data::SkillMotionStyle::Auto:
            return BattleActionMotionStyle::Auto;
        case game::data::SkillMotionStyle::WeaponAttack:
            return BattleActionMotionStyle::WeaponAttack;
        case game::data::SkillMotionStyle::Cast:
            return BattleActionMotionStyle::Cast;
        case game::data::SkillMotionStyle::Guard:
            return BattleActionMotionStyle::Guard;
        case game::data::SkillMotionStyle::Escape:
            return BattleActionMotionStyle::Escape;
        case game::data::SkillMotionStyle::Simple:
            return BattleActionMotionStyle::Simple;
    }
    return BattleActionMotionStyle::Auto;
}

[[nodiscard]] bool usesPhysicalHitFallback(const game::battle::BattleActionResult& result,
                                           const game::data::SkillData* skill) {
    if (!isAppliedSingleTargetHit(result) || result.damage <= 0) {
        return false;
    }
    if (result.action_type == game::battle::BattleActionType::Attack) {
        return true;
    }
    return result.action_type == game::battle::BattleActionType::Skill &&
        skill &&
        skill->hit_type_ == game::data::HitType::Physical &&
        !isRecoverySkill(*skill) &&
        skill->presentation_.target_vfx_id_hash_ == engine::vfx::kInvalidVfxEffectId;
}

[[nodiscard]] BattleActionMotionStyle defaultMotionStyle(const game::battle::BattleActionResult& result,
                                                         const game::data::SkillData* skill) {
    switch (result.action_type) {
        case game::battle::BattleActionType::Attack:
            return BattleActionMotionStyle::WeaponAttack;
        case game::battle::BattleActionType::Guard:
            return BattleActionMotionStyle::Cast;
        case game::battle::BattleActionType::Escape:
            return BattleActionMotionStyle::Escape;
        case game::battle::BattleActionType::Skill:
            if (skill && skill->presentation_.configured_ &&
                skill->presentation_.motion_style_ != game::data::SkillMotionStyle::Auto) {
                return mapMotionStyle(skill->presentation_.motion_style_);
            }
            if (skill && skill->hit_type_ == game::data::HitType::Physical && !isRecoverySkill(*skill)) {
                return BattleActionMotionStyle::WeaponAttack;
            }
            return BattleActionMotionStyle::Cast;
        case game::battle::BattleActionType::Item:
        case game::battle::BattleActionType::EndTurn:
            return BattleActionMotionStyle::Simple;
    }
    return BattleActionMotionStyle::Simple;
}

[[nodiscard]] float defaultImpactTime(const BattleActionMotionStyle motion_style, const float duration_seconds) {
    switch (motion_style) {
        case BattleActionMotionStyle::WeaponAttack:
            return std::min(DEFAULT_ATTACK_IMPACT_SECONDS, duration_seconds);
        case BattleActionMotionStyle::Cast:
            return std::clamp(duration_seconds * DEFAULT_CAST_IMPACT_RATIO, 0.0F, duration_seconds);
        case BattleActionMotionStyle::Guard:
        case BattleActionMotionStyle::Escape:
        case BattleActionMotionStyle::Simple:
        case BattleActionMotionStyle::Auto:
            return std::min(DEFAULT_SIMPLE_IMPACT_SECONDS, duration_seconds);
    }
    return std::min(DEFAULT_SIMPLE_IMPACT_SECONDS, duration_seconds);
}

[[nodiscard]] float defaultDuration(const BattleActionMotionStyle motion_style) {
    switch (motion_style) {
        case BattleActionMotionStyle::WeaponAttack:
            return DEFAULT_ATTACK_DURATION_SECONDS;
        case BattleActionMotionStyle::Cast:
            return DEFAULT_CAST_DURATION_SECONDS;
        case BattleActionMotionStyle::Guard:
        case BattleActionMotionStyle::Escape:
        case BattleActionMotionStyle::Simple:
        case BattleActionMotionStyle::Auto:
            return DEFAULT_SIMPLE_DURATION_SECONDS;
    }
    return DEFAULT_SIMPLE_DURATION_SECONDS;
}

[[nodiscard]] engine::vfx::PlayVfxCommand makeTargetVfxCommand(entt::id_type effect_id,
                                                               glm::vec2 position,
                                                               float scale) {
    engine::vfx::PlayVfxCommand command{};
    command.effect_id = effect_id;
    command.world_position = position;
    command.z = 0.0F;
    command.scale = scale;
    command.loop = false;
    command.channel = engine::vfx::VfxChannel::Overlay;
    return command;
}

[[nodiscard]] engine::utils::PlaySoundEvent makeSoundEvent(entt::id_type sound_id) {
    engine::utils::PlaySoundEvent event{};
    event.entity_ = entt::null;
    event.sound_id_ = sound_id;
    return event;
}

void addMarker(BattleActionPresentationPlan& plan, BattlePresentationMarker marker) {
    plan.duration_seconds = std::max(plan.duration_seconds, marker.time_seconds + marker.visual_tail_seconds);
    plan.markers.push_back(std::move(marker));
}

} // namespace

BattleActionPresentationPlan buildBattleActionPresentationPlan(
    const BattleActionPresentationPlanRequest& request) {
    BattleActionPresentationPlan plan{};
    if (!request.result) {
        return plan;
    }

    const auto& result = *request.result;
    const auto* skill = request.skill;
    const auto* presentation = skill && skill->presentation_.configured_ ? &skill->presentation_ : nullptr;

    plan.motion_style = defaultMotionStyle(result, skill);
    plan.actor_start_offset = request.actor_start_offset;
    plan.duration_seconds = defaultDuration(plan.motion_style);
    plan.impact_time_seconds = defaultImpactTime(plan.motion_style, plan.duration_seconds);
    plan.recovery_time_seconds = DEFAULT_RECOVERY_SECONDS;
    plan.visual_tail_seconds = hasNumericPopupFeedback(result) ? DEFAULT_POPUP_TAIL_SECONDS : 0.0F;

    if (presentation) {
        plan.duration_seconds = presentation->duration_seconds_;
        plan.impact_time_seconds = presentation->impact_time_seconds_;
        plan.recovery_time_seconds = presentation->recovery_duration_seconds_;
        plan.visual_tail_seconds = std::max(plan.visual_tail_seconds, presentation->target_vfx_tail_seconds_);
    }

    plan.duration_seconds = std::max(plan.duration_seconds, plan.impact_time_seconds + plan.recovery_time_seconds);

    const BattlePresentationUnitAnchor* target_anchor = nullptr;
    if (result.target_id && request.unit_anchors) {
        target_anchor = findAnchor(*request.unit_anchors, *result.target_id);
    }

    if (target_anchor) {
        plan.captured_target_position = target_anchor->base_screen_position;
    }

    if (result.status == game::battle::BattleActionStatus::Applied) {
        addMarker(plan, BattlePresentationMarker{
            .type = BattlePresentationMarkerType::EnemyHpReveal,
            .time_seconds = plan.impact_time_seconds,
            .visual_tail_seconds = hasVisibleEnemyHpFeedback(result) ? DEFAULT_RECOVERY_SECONDS : 0.0F,
            .target_id = result.target_id
        });
    }

    if (isAppliedSingleTargetHit(result) && target_anchor) {
        const glm::vec2 base_position = target_anchor->base_screen_position;
        if (presentation &&
            (presentation->target_vfx_id_hash_ != engine::vfx::kInvalidVfxEffectId ||
             presentation->target_sfx_id_hash_ != entt::id_type{})) {
            if (presentation->target_vfx_id_hash_ != engine::vfx::kInvalidVfxEffectId) {
                addMarker(plan, BattlePresentationMarker{
                    .type = BattlePresentationMarkerType::TargetVfx,
                    .time_seconds = plan.impact_time_seconds,
                    .visual_tail_seconds = presentation->target_vfx_tail_seconds_,
                    .target_id = result.target_id,
                    .vfx_command = makeTargetVfxCommand(
                        presentation->target_vfx_id_hash_,
                        base_position + presentation->target_vfx_offset_,
                        presentation->target_vfx_scale_)
                });
            }
            if (presentation->target_sfx_id_hash_ != entt::id_type{}) {
                addMarker(plan, BattlePresentationMarker{
                    .type = BattlePresentationMarkerType::TargetSfx,
                    .time_seconds = plan.impact_time_seconds,
                    .target_id = result.target_id,
                    .sound_event = makeSoundEvent(presentation->target_sfx_id_hash_)
                });
            }
        } else if (usesPhysicalHitFallback(result, skill)) {
            const auto* default_presentation = request.default_attack_skill
                ? &request.default_attack_skill->presentation_
                : nullptr;
            const entt::id_type vfx_id = default_presentation &&
                    default_presentation->target_vfx_id_hash_ != engine::vfx::kInvalidVfxEffectId
                ? default_presentation->target_vfx_id_hash_
                : entt::hashed_string{PHYSICAL_HIT_DEFAULT_VFX_ID.data(), PHYSICAL_HIT_DEFAULT_VFX_ID.size()}.value();
            const float scale = default_presentation && default_presentation->target_vfx_scale_ > 0.0F
                ? default_presentation->target_vfx_scale_
                : PHYSICAL_HIT_DEFAULT_VFX_SCALE;
            const glm::vec2 offset = default_presentation
                ? default_presentation->target_vfx_offset_
                : PHYSICAL_HIT_DEFAULT_VFX_OFFSET;
            const float tail = default_presentation && default_presentation->target_vfx_tail_seconds_ > 0.0F
                ? default_presentation->target_vfx_tail_seconds_
                : DEFAULT_TARGET_VFX_TAIL_SECONDS;
            addMarker(plan, BattlePresentationMarker{
                .type = BattlePresentationMarkerType::TargetVfx,
                .time_seconds = plan.impact_time_seconds,
                .visual_tail_seconds = tail,
                .target_id = result.target_id,
                .vfx_command = makeTargetVfxCommand(vfx_id, base_position + offset, scale)
            });

            if (default_presentation &&
                default_presentation->target_sfx_id_hash_ != entt::id_type{}) {
                addMarker(plan, BattlePresentationMarker{
                    .type = BattlePresentationMarkerType::TargetSfx,
                    .time_seconds = plan.impact_time_seconds,
                    .target_id = result.target_id,
                    .sound_event = makeSoundEvent(default_presentation->target_sfx_id_hash_)
                });
            }
        }
    }

    plan.duration_seconds = std::max(plan.duration_seconds, plan.impact_time_seconds + plan.visual_tail_seconds);
    return plan;
}

} // namespace game::scene
