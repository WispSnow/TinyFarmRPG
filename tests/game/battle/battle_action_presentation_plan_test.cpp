#include <gtest/gtest.h>

#include "game/scene/battle_action_presentation_plan.h"

#include <entt/core/hashed_string.hpp>

#include <algorithm>
#include <optional>
#include <vector>

namespace game::scene {
namespace {

[[nodiscard]] std::vector<BattlePresentationUnitAnchor> makeAnchors() {
    return {
        BattlePresentationUnitAnchor{
            .unit_id = 1,
            .side = game::battle::BattleSide::Player,
            .base_screen_position = glm::vec2{480.0f, 172.0f},
            .alive_after = true},
        BattlePresentationUnitAnchor{
            .unit_id = 2,
            .side = game::battle::BattleSide::Enemy,
            .base_screen_position = glm::vec2{160.0f, 172.0f},
            .alive_after = true}
    };
}

[[nodiscard]] game::battle::BattleActionResult makeDamageResult(
    game::battle::BattleActionType type = game::battle::BattleActionType::Attack,
    std::optional<game::battle::BattleUnitId> target_id = game::battle::BattleUnitId{2}) {
    game::battle::BattleActionResult result{};
    result.status = game::battle::BattleActionStatus::Applied;
    result.action_type = type;
    result.actor_id = 1;
    result.target_id = target_id;
    result.damage = 24;
    return result;
}

[[nodiscard]] game::data::SkillData makeAttackSkill() {
    game::data::SkillData skill{};
    skill.id_ = "skill.attack";
    skill.id_hash_ = entt::hashed_string{"skill.attack"}.value();
    skill.hit_type_ = game::data::HitType::Physical;
    skill.damage_.type = game::data::DamageType::HpDamage;
    skill.presentation_.configured_ = true;
    skill.presentation_.motion_style_ = game::data::SkillMotionStyle::WeaponAttack;
    skill.presentation_.duration_seconds_ = 0.72f;
    skill.presentation_.impact_time_seconds_ = 0.22f;
    skill.presentation_.recovery_duration_seconds_ = 0.30f;
    skill.presentation_.target_vfx_tail_seconds_ = 0.45f;
    skill.presentation_.target_vfx_id_ = "battle.hit_physical";
    skill.presentation_.target_vfx_id_hash_ = entt::hashed_string{"battle.hit_physical"}.value();
    skill.presentation_.target_sfx_id_ = "sfx.battle.physical_hit";
    skill.presentation_.target_sfx_id_hash_ = entt::hashed_string{"sfx.battle.physical_hit"}.value();
    skill.presentation_.target_vfx_scale_ = 4.0f;
    skill.presentation_.target_vfx_offset_ = glm::vec2{0.0f, -18.0f};
    return skill;
}

[[nodiscard]] game::data::SkillData makeFireSkill() {
    game::data::SkillData skill{};
    skill.id_ = "skill.fire";
    skill.id_hash_ = entt::hashed_string{"skill.fire"}.value();
    skill.hit_type_ = game::data::HitType::Magical;
    skill.damage_.type = game::data::DamageType::HpDamage;
    skill.presentation_.configured_ = true;
    skill.presentation_.motion_style_ = game::data::SkillMotionStyle::Cast;
    skill.presentation_.duration_seconds_ = 0.92f;
    skill.presentation_.impact_time_seconds_ = 0.46f;
    skill.presentation_.recovery_duration_seconds_ = 0.30f;
    skill.presentation_.target_vfx_tail_seconds_ = 0.45f;
    skill.presentation_.target_vfx_id_hash_ = entt::hashed_string{"battle.fire_one_1"}.value();
    skill.presentation_.target_sfx_id_hash_ = entt::hashed_string{"sfx.battle.fire_1"}.value();
    skill.presentation_.target_vfx_scale_ = 6.0f;
    skill.presentation_.target_vfx_offset_ = glm::vec2{0.0f, -36.0f};
    return skill;
}

[[nodiscard]] const BattlePresentationMarker* findMarker(const BattleActionPresentationPlan& plan,
                                                         BattlePresentationMarkerType type) {
    const auto it = std::find_if(plan.markers.begin(), plan.markers.end(), [type](const auto& marker) {
        return marker.type == type;
    });
    return it == plan.markers.end() ? nullptr : &*it;
}

TEST(BattleActionPresentationPlanTest, AttackUsesConfiguredImpactForVfxSfxAndHpReveal) {
    const auto anchors = makeAnchors();
    const auto attack = makeAttackSkill();
    const auto result = makeDamageResult();

    const auto plan = buildBattleActionPresentationPlan(BattleActionPresentationPlanRequest{
        .result = &result,
        .default_attack_skill = &attack,
        .unit_anchors = &anchors
    });

    EXPECT_EQ(plan.motion_style, BattleActionMotionStyle::WeaponAttack);
    EXPECT_FLOAT_EQ(plan.impact_time_seconds, 0.22f);
    EXPECT_GE(plan.duration_seconds, 0.22f + 0.85f);
    ASSERT_TRUE(plan.captured_target_position.has_value());
    EXPECT_EQ(*plan.captured_target_position, glm::vec2(160.0f, 172.0f));

    const auto* vfx = findMarker(plan, BattlePresentationMarkerType::TargetVfx);
    ASSERT_NE(vfx, nullptr);
    EXPECT_FLOAT_EQ(vfx->time_seconds, plan.impact_time_seconds);
    EXPECT_EQ(vfx->vfx_command.world_position, glm::vec2(160.0f, 154.0f));
    EXPECT_FLOAT_EQ(vfx->vfx_command.scale, 4.0f);

    const auto* sfx = findMarker(plan, BattlePresentationMarkerType::TargetSfx);
    ASSERT_NE(sfx, nullptr);
    EXPECT_FLOAT_EQ(sfx->time_seconds, plan.impact_time_seconds);

    const auto* hp = findMarker(plan, BattlePresentationMarkerType::EnemyHpReveal);
    ASSERT_NE(hp, nullptr);
    EXPECT_FLOAT_EQ(hp->time_seconds, plan.impact_time_seconds);
}

TEST(BattleActionPresentationPlanTest, ConfiguredSkillUsesSkillPresentationTiming) {
    const auto anchors = makeAnchors();
    const auto skill = makeFireSkill();
    auto result = makeDamageResult(game::battle::BattleActionType::Skill);
    result.skill_id = skill.id_;

    const auto plan = buildBattleActionPresentationPlan(BattleActionPresentationPlanRequest{
        .result = &result,
        .skill = &skill,
        .unit_anchors = &anchors
    });

    EXPECT_EQ(plan.motion_style, BattleActionMotionStyle::Cast);
    EXPECT_FLOAT_EQ(plan.impact_time_seconds, 0.46f);
    EXPECT_FLOAT_EQ(plan.duration_seconds, 1.31f);
    const auto* vfx = findMarker(plan, BattlePresentationMarkerType::TargetVfx);
    ASSERT_NE(vfx, nullptr);
    EXPECT_EQ(vfx->vfx_command.world_position, glm::vec2(160.0f, 136.0f));
}

TEST(BattleActionPresentationPlanTest, MissSchedulesHpRevealWithoutVfx) {
    const auto anchors = makeAnchors();
    auto result = makeDamageResult();
    result.missed = true;
    result.damage = 0;

    const auto plan = buildBattleActionPresentationPlan(BattleActionPresentationPlanRequest{
        .result = &result,
        .unit_anchors = &anchors
    });

    EXPECT_EQ(findMarker(plan, BattlePresentationMarkerType::TargetVfx), nullptr);
    EXPECT_NE(findMarker(plan, BattlePresentationMarkerType::EnemyHpReveal), nullptr);
}

TEST(BattleActionPresentationPlanTest, RejectedActionHasNoMarkers) {
    const auto anchors = makeAnchors();
    auto result = makeDamageResult();
    result.status = game::battle::BattleActionStatus::Rejected;

    const auto plan = buildBattleActionPresentationPlan(BattleActionPresentationPlanRequest{
        .result = &result,
        .unit_anchors = &anchors
    });

    EXPECT_TRUE(plan.markers.empty());
}

} // namespace
} // namespace game::scene
