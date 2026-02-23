#include "system_scheduler.h"

#include "system_bundle.h"

#include "engine/async/thread_pool.h"
#include "engine/component/collider_component.h"
#include "engine/component/tags.h"
#include "engine/component/transform_component.h"
#include "engine/component/velocity_component.h"
#include "engine/system/animation_system.h"
#include "engine/system/auto_tile_system.h"
#include "engine/system/deferred_commands.h"
#include "engine/system/movement_system.h"
#include "engine/system/parallel_wave_scheduler.h"
#include "engine/system/remove_entity_system.h"
#include "engine/system/spatial_index_system.h"
#include "engine/system/system_task_decl.h"
#include "game/component/actor_component.h"
#include "game/component/npc_component.h"
#include "game/component/state_component.h"
#include "game/component/tags.h"
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

#include <entt/core/hashed_string.hpp>
#include <entt/core/type_info.hpp>
#include <entt/entity/registry.hpp>

#include <chrono>
#include <vector>

namespace {

using game::runtime::GameMode;
using game::runtime::SchedulerStage;
using game::runtime::SystemScheduler;

using namespace entt::literals;

constexpr entt::id_type RESOURCE_SPATIAL_INDEX = "spatial_index"_hs;
constexpr entt::id_type RESOURCE_CAMERA = "camera"_hs;
constexpr entt::id_type RESOURCE_INPUT = "input"_hs;
constexpr entt::id_type RESOURCE_NPC_WANDER_DOMAIN = "npc_wander_domain"_hs;
constexpr entt::id_type RESOURCE_ANIMAL_BEHAVIOR_DOMAIN = "animal_behavior_domain"_hs;

[[nodiscard]] double elapsedMs(const std::chrono::steady_clock::time_point& begin,
                               const std::chrono::steady_clock::time_point& end) {
    return std::chrono::duration<double, std::milli>(end - begin).count();
}

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
        SchedulerStage::CameraFollow,
        SchedulerStage::Pickup,
        SchedulerStage::Interaction,
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

void trace_stage(SystemScheduler::TickResult& result, const SchedulerStage stage, const double elapsed_ms) {
    result.trace.stages.push_back(SystemScheduler::StageTrace{stage, elapsed_ms});
}

void execute_stage_main_thread(const SystemScheduler::TickParams& params,
                               const SchedulerStage stage,
                               SystemScheduler::TickResult& result) {
    auto& systems = params.systems;
    auto& registry = params.registry;
    const float delta_time = params.delta_time;

    const auto begin = std::chrono::steady_clock::now();

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
                engine::system::DeferredCommands deferred;
                systems.npc_wander_system->update(delta_time, deferred);
                deferred.drain(registry);
            }
            break;
        case SchedulerStage::AnimalBehavior:
            if (systems.animal_behavior_system) {
                engine::system::DeferredCommands deferred;
                systems.animal_behavior_system->update(delta_time, deferred);
                deferred.drain(registry);
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
                engine::system::DeferredCommands deferred;
                systems.spatial_index_system->update(registry, deferred);
                deferred.drain(registry);
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

    const auto end = std::chrono::steady_clock::now();
    trace_stage(result, stage, elapsedMs(begin, end));
}

[[nodiscard]] bool transition_active(const SystemScheduler::TickParams& params) {
    if (params.is_transition_active) {
        return params.is_transition_active();
    }
    return params.systems.map_transition_system && params.systems.map_transition_system->isTransitionActive();
}

void prepare_mid_stage_parallel_island_registry(entt::registry& registry) {
    // EnTT registry 的 storage 是惰性初始化；并发前主线程预热，避免 worker 触发隐式创建。
    (void)registry.storage<game::component::NPCTag>();
    (void)registry.storage<game::component::AnimalTag>();
    (void)registry.storage<game::component::WanderComponent>();
    (void)registry.storage<game::component::SleepRoutine>();
    (void)registry.storage<game::component::DialogueComponent>();
    (void)registry.storage<game::component::AnimalBehaviorState>();
    (void)registry.storage<game::component::ActorComponent>();
    (void)registry.storage<game::component::StateComponent>();
    (void)registry.storage<game::component::StateDirtyTag>();
    (void)registry.storage<engine::component::TransformComponent>();
    (void)registry.storage<engine::component::VelocityComponent>();
}

void prepare_post_gate_parallel_island_registry(entt::registry& registry) {
    // EnTT registry 的 storage 是惰性初始化；并发前主线程预热，避免 worker 触发隐式创建。
    (void)registry.storage<engine::component::SpatialIndexTag>();
    (void)registry.storage<engine::component::TransformDirtyTag>();
    (void)registry.storage<engine::component::TransformComponent>();
    (void)registry.storage<engine::component::AABBCollider>();
    (void)registry.storage<engine::component::CircleCollider>();
    (void)registry.storage<game::component::PlayerTag>();
}

} // namespace

namespace game::runtime {

SystemScheduler::~SystemScheduler() = default;

SystemScheduler::TickResult SystemScheduler::tick(const TickParams& params) const {
    TickResult result{};

    if (params.mode != GameMode::Exploration) {
        for (const auto stage : profileStages(params.mode)) {
            execute_stage_main_thread(params, stage, result);
        }
        return result;
    }

    execute_stage_main_thread(params, SchedulerStage::RemoveEntity, result);

    if (transition_active(params)) {
        execute_stage_main_thread(params, SchedulerStage::TransitionUpdatePre, result);
        execute_stage_main_thread(params, SchedulerStage::LightTogglePre, result);
        result.gate1_triggered = true;
        return result;
    }

    execute_stage_main_thread(params, SchedulerStage::Time, result);
    execute_stage_main_thread(params, SchedulerStage::DayNight, result);
    execute_stage_main_thread(params, SchedulerStage::PlayerControl, result);

    prepare_mid_stage_parallel_island_registry(params.registry);
    setParallelIslandContext(params);
    auto& mid_stage_island_scheduler = midStageParallelIslandScheduler();
    if (!mid_stage_island_scheduler.valid()) {
        execute_stage_main_thread(params, SchedulerStage::NPCWander, result);
        execute_stage_main_thread(params, SchedulerStage::AnimalBehavior, result);
        clearParallelIslandContext();
    } else {
        const auto elapsed = mid_stage_island_scheduler.execute(params.registry);
        clearParallelIslandContext();
        if (elapsed.size() < 2) {
            execute_stage_main_thread(params, SchedulerStage::NPCWander, result);
            execute_stage_main_thread(params, SchedulerStage::AnimalBehavior, result);
        } else {
            trace_stage(result, SchedulerStage::NPCWander, elapsed[0]);
            trace_stage(result, SchedulerStage::AnimalBehavior, elapsed[1]);
        }
    }

    execute_stage_main_thread(params, SchedulerStage::Chest, result);
    execute_stage_main_thread(params, SchedulerStage::ItemUse, result);
    execute_stage_main_thread(params, SchedulerStage::Dialogue, result);
    execute_stage_main_thread(params, SchedulerStage::ActionSound, result);
    execute_stage_main_thread(params, SchedulerStage::AutoTile, result);
    execute_stage_main_thread(params, SchedulerStage::State, result);
    execute_stage_main_thread(params, SchedulerStage::Movement, result);

    execute_stage_main_thread(params, SchedulerStage::TransitionUpdatePost, result);
    execute_stage_main_thread(params, SchedulerStage::LightTogglePost, result);

    if (transition_active(params)) {
        result.gate2_triggered = true;
        return result;
    }

    prepare_post_gate_parallel_island_registry(params.registry);
    setParallelIslandContext(params);
    auto& island_scheduler = postGateParallelIslandScheduler();
    if (!island_scheduler.valid()) {
        execute_stage_main_thread(params, SchedulerStage::SpatialIndex, result);
        execute_stage_main_thread(params, SchedulerStage::CameraFollow, result);
        clearParallelIslandContext();
        execute_stage_main_thread(params, SchedulerStage::Pickup, result);
        execute_stage_main_thread(params, SchedulerStage::Interaction, result);
        execute_stage_main_thread(params, SchedulerStage::Animation, result);
        return result;
    }

    const auto elapsed = island_scheduler.execute(params.registry);
    clearParallelIslandContext();
    if (elapsed.size() < 2) {
        execute_stage_main_thread(params, SchedulerStage::SpatialIndex, result);
        execute_stage_main_thread(params, SchedulerStage::CameraFollow, result);
    } else {
        trace_stage(result, SchedulerStage::SpatialIndex, elapsed[0]);
        trace_stage(result, SchedulerStage::CameraFollow, elapsed[1]);
    }

    execute_stage_main_thread(params, SchedulerStage::Pickup, result);
    execute_stage_main_thread(params, SchedulerStage::Interaction, result);
    execute_stage_main_thread(params, SchedulerStage::Animation, result);
    return result;
}

const std::vector<SchedulerStage>& SystemScheduler::profileStages(const GameMode mode) {
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

    return exploration_profile();
}

engine::async::ThreadPool& SystemScheduler::parallelThreadPool() const {
    if (!parallel_thread_pool_) {
        parallel_thread_pool_ = std::make_unique<engine::async::ThreadPool>(engine::async::ThreadPool::Options{
            .name = "SystemSchedulerParallel"
        });
    }
    return *parallel_thread_pool_;
}

engine::system::ParallelWaveScheduler& SystemScheduler::midStageParallelIslandScheduler() const {
    if (!mid_stage_parallel_island_scheduler_) {
        std::vector<engine::system::SystemTaskDecl> decls;
        decls.reserve(2);

        decls.push_back(engine::system::SystemTaskDecl{
            .name = "NPCWander",
            .policy = engine::system::ExecutionPolicy::WorkerEligible,
            .run = [this](engine::system::DeferredCommands& deferred) {
                const auto* systems = parallel_island_context_.systems;
                if (!systems || !systems->npc_wander_system) {
                    return;
                }
                systems->npc_wander_system->update(parallel_island_context_.delta_time, deferred);
            },
            .rw_resources = {RESOURCE_NPC_WANDER_DOMAIN}
        });

        decls.push_back(engine::system::SystemTaskDecl{
            .name = "AnimalBehavior",
            .policy = engine::system::ExecutionPolicy::WorkerEligible,
            .run = [this](engine::system::DeferredCommands& deferred) {
                const auto* systems = parallel_island_context_.systems;
                if (!systems || !systems->animal_behavior_system) {
                    return;
                }
                systems->animal_behavior_system->update(parallel_island_context_.delta_time, deferred);
            },
            .rw_resources = {RESOURCE_ANIMAL_BEHAVIOR_DOMAIN}
        });

        mid_stage_parallel_island_scheduler_ = std::make_unique<engine::system::ParallelWaveScheduler>(
            std::move(decls),
            &parallelThreadPool());
    }
    return *mid_stage_parallel_island_scheduler_;
}

engine::system::ParallelWaveScheduler& SystemScheduler::postGateParallelIslandScheduler() const {
    if (!post_gate_parallel_island_scheduler_) {
        std::vector<engine::system::SystemTaskDecl> decls;
        decls.reserve(2);

        decls.push_back(engine::system::SystemTaskDecl{
            .name = "SpatialIndex",
            .policy = engine::system::ExecutionPolicy::WorkerEligible,
            .run = [this](engine::system::DeferredCommands& deferred) {
                const auto* systems = parallel_island_context_.systems;
                const auto* registry = parallel_island_context_.registry;
                if (systems && systems->spatial_index_system && registry) {
                    systems->spatial_index_system->update(*registry, deferred);
                }
            },
            .ro_resources = {
                entt::type_hash<engine::component::TransformDirtyTag>::value(),
                entt::type_hash<engine::component::TransformComponent>::value(),
                entt::type_hash<engine::component::AABBCollider>::value(),
                entt::type_hash<engine::component::CircleCollider>::value()
            },
            .rw_resources = {RESOURCE_SPATIAL_INDEX}
        });

        decls.push_back(engine::system::SystemTaskDecl{
            .name = "CameraFollow",
            .policy = engine::system::ExecutionPolicy::WorkerEligible,
            .run = [this](engine::system::DeferredCommands&) {
                const auto* systems = parallel_island_context_.systems;
                if (!systems || !systems->camera_follow_system) {
                    return;
                }

                // InputManager 仅在主线程 handleEvents() 中写入；并行岛执行期间只读快照状态。
                systems->camera_follow_system->update(parallel_island_context_.delta_time);
            },
            .ro_resources = {
                entt::type_hash<game::component::PlayerTag>::value(),
                entt::type_hash<engine::component::TransformComponent>::value(),
                RESOURCE_INPUT
            },
            .rw_resources = {RESOURCE_CAMERA}
        });

        post_gate_parallel_island_scheduler_ = std::make_unique<engine::system::ParallelWaveScheduler>(
            std::move(decls),
            &parallelThreadPool());
    }
    return *post_gate_parallel_island_scheduler_;
}

void SystemScheduler::setParallelIslandContext(const TickParams& params) const {
    parallel_island_context_.systems = &params.systems;
    parallel_island_context_.registry = &params.registry;
    parallel_island_context_.delta_time = params.delta_time;
}

void SystemScheduler::clearParallelIslandContext() const {
    parallel_island_context_ = {};
}

const char* toString(const SchedulerStage stage) {
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

std::string dumpPostGateParallelIslandDot() {
    std::vector<engine::system::SystemTaskDecl> decls;
    decls.reserve(2);

    decls.push_back(engine::system::SystemTaskDecl{
        .name = "SpatialIndex",
        .policy = engine::system::ExecutionPolicy::WorkerEligible,
        .run = [](engine::system::DeferredCommands&) {},
        .ro_resources = {
            entt::type_hash<engine::component::TransformDirtyTag>::value(),
            entt::type_hash<engine::component::TransformComponent>::value(),
            entt::type_hash<engine::component::AABBCollider>::value(),
            entt::type_hash<engine::component::CircleCollider>::value()
        },
        .rw_resources = {RESOURCE_SPATIAL_INDEX}
    });

    decls.push_back(engine::system::SystemTaskDecl{
        .name = "CameraFollow",
        .policy = engine::system::ExecutionPolicy::WorkerEligible,
        .run = [](engine::system::DeferredCommands&) {},
        .ro_resources = {
            entt::type_hash<game::component::PlayerTag>::value(),
            entt::type_hash<engine::component::TransformComponent>::value(),
            RESOURCE_INPUT
        },
        .rw_resources = {RESOURCE_CAMERA}
    });

    engine::system::ParallelWaveScheduler scheduler(std::move(decls), nullptr);
    if (!scheduler.valid()) {
        return {};
    }
    return scheduler.dumpDot();
}

} // namespace game::runtime
