#pragma once

#include "game_mode.h"

#include <functional>
#include <vector>

#include <entt/entity/fwd.hpp>

namespace game::runtime {

struct GameSystemBundle;

enum class SchedulerStage {
    RemoveEntity,
    TransitionUpdatePre,
    LightTogglePre,
    Time,
    DayNight,
    PlayerControl,
    NPCWander,
    AnimalBehavior,
    Chest,
    ItemUse,
    Dialogue,
    ActionSound,
    AutoTile,
    State,
    Movement,
    TransitionUpdatePost,
    LightTogglePost,
    SpatialIndex,
    Pickup,
    Interaction,
    CameraFollow,
    Animation
};

class SystemScheduler final {
public:
    struct TickParams {
        GameMode mode{GameMode::Exploration};
        GameSystemBundle& systems;
        entt::registry& registry;
        float delta_time{0.0f};
        std::function<void(SchedulerStage)> on_stage_executed{};
        std::function<bool()> is_transition_active{};
    };

    struct TickResult {
        bool gate1_triggered{false};
        bool gate2_triggered{false};
    };

    [[nodiscard]] TickResult tick(const TickParams& params) const;

    [[nodiscard]] static const std::vector<SchedulerStage>& profileStages(GameMode mode);
};

[[nodiscard]] const char* toString(SchedulerStage stage);

} // namespace game::runtime
