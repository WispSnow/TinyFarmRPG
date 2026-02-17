#include "system_scheduler.h"

#include "system_bundle.h"

#include "engine/system/animation_system.h"
#include "engine/system/auto_tile_system.h"
#include "engine/system/movement_system.h"
#include "engine/system/remove_entity_system.h"
#include "engine/system/spatial_index_system.h"
#include "game/system/action_sound_system.h"
#include "game/system/animal_behavior_system.h"
#include "game/system/camera_follow_system.h"
#include "game/system/chest_system.h"
#include "game/system/day_night_system.h"
#include "game/system/dialogue_system.h"
#include "game/system/interaction_system.h"
#include "game/system/item_use_system.h"
#include "game/system/light_toggle_system.h"
#include "game/system/map_transition_system.h"
#include "game/system/npc_wander_system.h"
#include "game/system/pickup_system.h"
#include "game/system/player_control_system.h"
#include "game/system/state_system.h"
#include "game/system/time_system.h"

namespace {

using game::runtime::GameMode;
using game::runtime::SchedulerStage;

const std::vector<SchedulerStage>& exploration_profile() {
    static const std::vector<SchedulerStage> kStages{
        SchedulerStage::RemoveEntity,
        SchedulerStage::TransitionUpdatePre,
        SchedulerStage::LightTogglePre,
        SchedulerStage::Time,
        SchedulerStage::DayNight,
        SchedulerStage::PlayerControl,
        SchedulerStage::NPCWander,
        SchedulerStage::AnimalBehavior,
        SchedulerStage::Chest,
        SchedulerStage::ItemUse,
        SchedulerStage::Dialogue,
        SchedulerStage::ActionSound,
        SchedulerStage::AutoTile,
        SchedulerStage::State,
        SchedulerStage::Movement,
        SchedulerStage::TransitionUpdatePost,
        SchedulerStage::LightTogglePost,
        SchedulerStage::SpatialIndex,
        SchedulerStage::Pickup,
        SchedulerStage::Interaction,
        SchedulerStage::CameraFollow,
        SchedulerStage::Animation
    };
    return kStages;
}

const std::vector<SchedulerStage>& battle_profile() {
    static const std::vector<SchedulerStage> kStages{
        SchedulerStage::RemoveEntity
    };
    return kStages;
}

const std::vector<SchedulerStage>& pause_overlay_profile() {
    static const std::vector<SchedulerStage> kStages{
        SchedulerStage::RemoveEntity
    };
    return kStages;
}

const std::vector<SchedulerStage>& cutscene_profile() {
    static const std::vector<SchedulerStage> kStages{
        SchedulerStage::RemoveEntity,
        SchedulerStage::Time,
        SchedulerStage::DayNight,
        SchedulerStage::TransitionUpdatePost,
        SchedulerStage::LightTogglePost
    };
    return kStages;
}

void trace_stage(const game::runtime::SystemScheduler::TickParams& params, SchedulerStage stage) {
    if (params.on_stage_executed) {
        params.on_stage_executed(stage);
    }
}

void execute_stage(const game::runtime::SystemScheduler::TickParams& params, SchedulerStage stage) {
    trace_stage(params, stage);

    auto& systems = params.systems;
    auto& registry = params.registry;
    const float delta_time = params.delta_time;

    switch (stage) {
        case SchedulerStage::RemoveEntity:
            if (systems.remove_entity_system) {
                systems.remove_entity_system->update(registry);
            }
            break;
        case SchedulerStage::TransitionUpdatePre:
        case SchedulerStage::TransitionUpdatePost:
            if (systems.map_transition_system) {
                systems.map_transition_system->update();
            }
            break;
        case SchedulerStage::LightTogglePre:
        case SchedulerStage::LightTogglePost:
            if (systems.light_toggle_system) {
                systems.light_toggle_system->update();
            }
            break;
        case SchedulerStage::Time:
            if (systems.time_system) {
                systems.time_system->update(delta_time);
            }
            break;
        case SchedulerStage::DayNight:
            if (systems.day_night_system) {
                systems.day_night_system->update();
            }
            break;
        case SchedulerStage::PlayerControl:
            if (systems.player_control_system) {
                systems.player_control_system->update(delta_time);
            }
            break;
        case SchedulerStage::NPCWander:
            if (systems.npc_wander_system) {
                systems.npc_wander_system->update(delta_time);
            }
            break;
        case SchedulerStage::AnimalBehavior:
            if (systems.animal_behavior_system) {
                systems.animal_behavior_system->update(delta_time);
            }
            break;
        case SchedulerStage::Chest:
            if (systems.chest_system) {
                systems.chest_system->update(delta_time);
            }
            break;
        case SchedulerStage::ItemUse:
            if (systems.item_use_system) {
                systems.item_use_system->update(delta_time);
            }
            break;
        case SchedulerStage::Dialogue:
            if (systems.dialogue_system) {
                systems.dialogue_system->update(delta_time);
            }
            break;
        case SchedulerStage::ActionSound:
            if (systems.action_sound_system) {
                systems.action_sound_system->update(delta_time);
            }
            break;
        case SchedulerStage::AutoTile:
            if (systems.auto_tile_system) {
                systems.auto_tile_system->update();
            }
            break;
        case SchedulerStage::State:
            if (systems.state_system) {
                systems.state_system->update();
            }
            break;
        case SchedulerStage::Movement:
            if (systems.movement_system) {
                systems.movement_system->update(registry, delta_time);
            }
            break;
        case SchedulerStage::SpatialIndex:
            if (systems.spatial_index_system) {
                systems.spatial_index_system->update(registry);
            }
            break;
        case SchedulerStage::Pickup:
            if (systems.pickup_system) {
                systems.pickup_system->update(delta_time);
            }
            break;
        case SchedulerStage::Interaction:
            if (systems.interaction_system) {
                systems.interaction_system->update();
            }
            break;
        case SchedulerStage::CameraFollow:
            if (systems.camera_follow_system) {
                systems.camera_follow_system->update(delta_time);
            }
            break;
        case SchedulerStage::Animation:
            if (systems.animation_system) {
                systems.animation_system->update(delta_time);
            }
            break;
    }
}

bool transition_active(const game::runtime::SystemScheduler::TickParams& params) {
    if (params.is_transition_active) {
        return params.is_transition_active();
    }
    return params.systems.map_transition_system && params.systems.map_transition_system->isTransitionActive();
}

} // namespace

namespace game::runtime {

SystemScheduler::TickResult SystemScheduler::tick(const TickParams& params) const {
    TickResult result{};

    if (params.mode != GameMode::Exploration) {
        // Non-exploration profiles are intentionally minimal placeholders in FND-002.
        for (const auto stage : profileStages(params.mode)) {
            execute_stage(params, stage);
        }
        return result;
    }

    execute_stage(params, SchedulerStage::RemoveEntity);

    if (transition_active(params)) {
        execute_stage(params, SchedulerStage::TransitionUpdatePre);
        execute_stage(params, SchedulerStage::LightTogglePre);
        result.gate1_triggered = true;
        return result;
    }

    execute_stage(params, SchedulerStage::Time);
    execute_stage(params, SchedulerStage::DayNight);
    execute_stage(params, SchedulerStage::PlayerControl);
    execute_stage(params, SchedulerStage::NPCWander);
    execute_stage(params, SchedulerStage::AnimalBehavior);
    execute_stage(params, SchedulerStage::Chest);
    execute_stage(params, SchedulerStage::ItemUse);
    execute_stage(params, SchedulerStage::Dialogue);
    execute_stage(params, SchedulerStage::ActionSound);
    execute_stage(params, SchedulerStage::AutoTile);
    execute_stage(params, SchedulerStage::State);
    execute_stage(params, SchedulerStage::Movement);

    execute_stage(params, SchedulerStage::TransitionUpdatePost);
    execute_stage(params, SchedulerStage::LightTogglePost);

    if (transition_active(params)) {
        result.gate2_triggered = true;
        return result;
    }

    execute_stage(params, SchedulerStage::SpatialIndex);
    execute_stage(params, SchedulerStage::Pickup);
    execute_stage(params, SchedulerStage::Interaction);
    execute_stage(params, SchedulerStage::CameraFollow);
    execute_stage(params, SchedulerStage::Animation);
    return result;
}

const std::vector<SchedulerStage>& SystemScheduler::profileStages(GameMode mode) {
    switch (mode) {
        case GameMode::Exploration:
            return exploration_profile();
        case GameMode::Battle:
            return battle_profile();
        case GameMode::PauseOverlay:
            return pause_overlay_profile();
        case GameMode::Cutscene:
            return cutscene_profile();
    }

    // Defensive fallback for invalid enum values.
    return exploration_profile();
}

const char* toString(SchedulerStage stage) {
    switch (stage) {
        case SchedulerStage::RemoveEntity:
            return "RemoveEntity";
        case SchedulerStage::TransitionUpdatePre:
            return "TransitionUpdatePre";
        case SchedulerStage::LightTogglePre:
            return "LightTogglePre";
        case SchedulerStage::Time:
            return "Time";
        case SchedulerStage::DayNight:
            return "DayNight";
        case SchedulerStage::PlayerControl:
            return "PlayerControl";
        case SchedulerStage::NPCWander:
            return "NPCWander";
        case SchedulerStage::AnimalBehavior:
            return "AnimalBehavior";
        case SchedulerStage::Chest:
            return "Chest";
        case SchedulerStage::ItemUse:
            return "ItemUse";
        case SchedulerStage::Dialogue:
            return "Dialogue";
        case SchedulerStage::ActionSound:
            return "ActionSound";
        case SchedulerStage::AutoTile:
            return "AutoTile";
        case SchedulerStage::State:
            return "State";
        case SchedulerStage::Movement:
            return "Movement";
        case SchedulerStage::TransitionUpdatePost:
            return "TransitionUpdatePost";
        case SchedulerStage::LightTogglePost:
            return "LightTogglePost";
        case SchedulerStage::SpatialIndex:
            return "SpatialIndex";
        case SchedulerStage::Pickup:
            return "Pickup";
        case SchedulerStage::Interaction:
            return "Interaction";
        case SchedulerStage::CameraFollow:
            return "CameraFollow";
        case SchedulerStage::Animation:
            return "Animation";
    }

    return "Unknown";
}

} // namespace game::runtime
