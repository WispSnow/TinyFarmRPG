#pragma once

#include "engine/utils/events.h"
#include "engine/vfx/vfx_types.h"
#include "game/battle/battle_types.h"
#include "game/data/rpg_data.h"
#include "game/scene/battle_animation_director.h"
#include "game/scene/battle_presentation_unit_anchor.h"

#include <glm/vec2.hpp>

#include <optional>
#include <vector>

namespace game::scene {

enum class BattlePresentationMarkerType {
    TargetVfx,
    TargetSfx,
    EnemyHpReveal
};

struct BattlePresentationMarker {
    BattlePresentationMarkerType type{BattlePresentationMarkerType::TargetVfx};
    float time_seconds{0.0F};
    float visual_tail_seconds{0.0F};
    std::optional<game::battle::BattleUnitId> target_id{};
    engine::vfx::PlayVfxCommand vfx_command{};
    engine::utils::PlaySoundEvent sound_event{};
};

struct BattleActionPresentationPlan {
    BattleActionMotionStyle motion_style{BattleActionMotionStyle::Simple};
    float duration_seconds{0.0F};
    float impact_time_seconds{0.0F};
    float recovery_time_seconds{0.0F};
    float visual_tail_seconds{0.0F};
    glm::vec2 actor_start_offset{0.0F, 0.0F};
    std::optional<glm::vec2> captured_target_position{};
    std::vector<BattlePresentationMarker> markers{};
};

struct BattleActionPresentationPlanRequest {
    const game::battle::BattleActionResult* result{nullptr};
    const game::data::SkillData* skill{nullptr};
    const game::data::SkillData* default_attack_skill{nullptr};
    const std::vector<BattlePresentationUnitAnchor>* unit_anchors{nullptr};
    glm::vec2 actor_start_offset{0.0F, 0.0F};
};

[[nodiscard]] BattleActionPresentationPlan buildBattleActionPresentationPlan(
    const BattleActionPresentationPlanRequest& request);

} // namespace game::scene
