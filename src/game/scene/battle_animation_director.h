#pragma once

#include "engine/utils/math.h"
#include "game/battle/battle_types.h"

#include <glm/vec2.hpp>

#include <optional>
#include <unordered_map>
#include <vector>

namespace game::scene {

struct BattleAnimationSpriteSnapshot {
    game::battle::BattleUnitId unit_id{0};
    game::battle::BattleSide side{game::battle::BattleSide::Player};
    glm::vec2 base_screen_position{0.0f, 0.0f};
    bool alive_after{true};
};

struct BattleAnimationPose {
    glm::vec2 offset{0.0f, 0.0f};
    float scale_multiplier{1.0f};
    float rotation_radians{0.0f};
    engine::utils::FColor color_multiplier{engine::utils::FColor::white()};
};

struct BattleAnimationTimelineConfig {
    float attack_duration_seconds{0.72f};
    float action_hold_seconds{0.34f};
};

class BattleAnimationDirector final {
public:
    void reset();
    void begin(const game::battle::BattleActionResult& result,
               const std::vector<BattleAnimationSpriteSnapshot>& sprites,
               const BattleAnimationTimelineConfig& config = {});
    void update(float delta_time_seconds);

    [[nodiscard]] bool finished() const;
    [[nodiscard]] bool active() const { return active_; }
    [[nodiscard]] std::optional<BattleAnimationPose> poseFor(game::battle::BattleUnitId unit_id) const;
    [[nodiscard]] std::optional<BattleAnimationPose> persistentPoseFor(game::battle::BattleUnitId unit_id) const;

private:
    struct Timeline {
        game::battle::BattleActionResult result{};
        std::unordered_map<game::battle::BattleUnitId, BattleAnimationSpriteSnapshot> sprites{};
        float elapsed_seconds{0.0f};
        float duration_seconds{0.0f};
        bool active{false};
    };

    [[nodiscard]] std::optional<BattleAnimationPose> transientPoseFor(game::battle::BattleUnitId unit_id) const;
    [[nodiscard]] std::optional<BattleAnimationPose> attackPoseFor(game::battle::BattleUnitId unit_id) const;
    [[nodiscard]] std::optional<BattleAnimationPose> simpleActionPoseFor(game::battle::BattleUnitId unit_id) const;
    [[nodiscard]] std::optional<BattleAnimationPose> hitPoseFor(game::battle::BattleUnitId unit_id) const;
    [[nodiscard]] bool usesForwardActionPose() const;
    [[nodiscard]] glm::vec2 forwardStepOffset() const;
    [[nodiscard]] bool hasValidSingleTarget() const;

    void clearTransient();
    void persistDefeatedTarget();

    Timeline timeline_{};
    std::unordered_map<game::battle::BattleUnitId, BattleAnimationPose> persistent_poses_{};
    BattleAnimationTimelineConfig config_{};
    bool active_{false};
};

} // namespace game::scene
