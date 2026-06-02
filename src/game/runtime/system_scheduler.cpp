#include "system_scheduler.h"

#include "system_bundle.h"

#if defined(TF_ENABLE_RUNTIME_THREADS)
#include "engine/async/thread_pool.h"
#endif
#include "engine/component/animation_component.h"
#include "engine/component/audio_component.h"
#include "engine/component/collider_component.h"
#include "engine/component/sprite_component.h"
#include "engine/component/tags.h"
#include "engine/component/transform_component.h"
#include "engine/component/velocity_component.h"
#include "engine/platform/threading.h"
#include "engine/render/lighting_state.h"
#include "engine/system/animation_system.h"
#include "engine/system/auto_tile_system.h"
#include "engine/system/deferred_commands.h"
#include "engine/system/movement_system.h"
#include "engine/system/parallel_wave_scheduler.h"
#include "engine/system/remove_entity_system.h"
#include "engine/system/spatial_index_system.h"
#include "engine/system/system_task_decl.h"
#include "engine/system/task_event_buffer.h"
#include "game/component/action_sound_component.h"
#include "game/component/actor_component.h"
#include "game/component/npc_component.h"
#include "game/component/state_component.h"
#include "game/component/tags.h"
#include "game/data/game_time.h"
#include "game/system/action_sound_system.h"
#include "game/system/animal_behavior_system.h"
#include "game/system/camera_follow_system.h"
#include "game/system/chest_system.h"
#include "game/system/day_night_system.h"
#include "game/system/dialogue_system.h"
#include "game/system/enemy_encounter_system.h"
#include "game/system/farm_system.h"
#include "game/system/interaction_system.h"
#include "game/system/item_use_system.h"
#include "game/system/light_toggle_system.h"
#include "game/system/map_transition_system.h"
#include "game/system/npc_wander_system.h"
#include "game/system/pickup_system.h"
#include "game/system/party_recruitment_system.h"
#include "game/system/player_control_system.h"
#include "game/system/quest_interaction_system.h"
#include "game/system/recruitment_interaction_system.h"
#include "game/system/scripted_dialogue_lifecycle_system.h"
#include "game/system/shop_interaction_system.h"
#include "game/system/state_system.h"
#include "game/system/time_system.h"
#include "game/system/zone_trigger_system.h"
#include "game/script/script_event_bridge.h"

#include <entt/core/hashed_string.hpp>
#include <entt/core/type_info.hpp>
#include <entt/entity/registry.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string_view>
#include <vector>

namespace {

using game::runtime::GameMode;
using game::runtime::SchedulerStage;
using game::runtime::SystemScheduler;

using namespace entt::literals;

constexpr entt::id_type RESOURCE_SPATIAL_INDEX = "spatial_index"_hs;
constexpr entt::id_type RESOURCE_CAMERA = "camera"_hs;
constexpr entt::id_type RESOURCE_INPUT = "input"_hs;
constexpr entt::id_type RESOURCE_GAME_TIME = "game_time"_hs;
constexpr entt::id_type RESOURCE_WORLD_STATE = "world_state"_hs;
constexpr entt::id_type RESOURCE_GLOBAL_LIGHTING_STATE = "global_lighting_state"_hs;
constexpr entt::id_type RESOURCE_NPC_WANDER_DOMAIN = "npc_wander_domain"_hs;
constexpr entt::id_type RESOURCE_ANIMAL_BEHAVIOR_DOMAIN = "animal_behavior_domain"_hs;

[[nodiscard]] double elapsedMs(const std::chrono::steady_clock::time_point& begin,
                               const std::chrono::steady_clock::time_point& end) {
    return std::chrono::duration<double, std::milli>(end - begin).count();
}

enum class StageBlock : std::uint8_t {
    PreGate1,
    Gate1Transition,
    Main,
    PostTransition,
    PostGate
};

enum class ParallelIsland : std::uint8_t {
    None,
    MidStage,
    PreMovement,
    PostGate
};

using MainStageRunner = void (*)(const SystemScheduler::TickParams& params);

struct StageDecl {
    SchedulerStage stage{SchedulerStage::RemoveEntity};
    std::string_view name{};
    std::uint8_t mode_mask{0};
    StageBlock block{StageBlock::Main};
    MainStageRunner run_main{nullptr};
    ParallelIsland parallel_island{ParallelIsland::None};
    engine::system::ExecutionPolicy policy{engine::system::ExecutionPolicy::MainThreadOnly};
    std::vector<entt::id_type> ro_resources{};
    std::vector<entt::id_type> rw_resources{};
};

constexpr std::uint8_t MODE_EXPLORATION = 1U << 0U;
constexpr std::uint8_t MODE_BATTLE = 1U << 1U;
constexpr std::uint8_t MODE_PAUSE_OVERLAY = 1U << 2U;
constexpr std::uint8_t MODE_CUTSCENE = 1U << 3U;
constexpr std::uint8_t MODE_ALL = MODE_EXPLORATION | MODE_BATTLE | MODE_PAUSE_OVERLAY | MODE_CUTSCENE;

[[nodiscard]] const game::data::GameTime* find_game_time(const entt::registry& registry);

void run_remove_entity(const SystemScheduler::TickParams& params);
void run_transition_update(const SystemScheduler::TickParams& params);
void run_light_toggle(const SystemScheduler::TickParams& params);
void run_time(const SystemScheduler::TickParams& params);
void run_day_night(const SystemScheduler::TickParams& params);
void run_player_control(const SystemScheduler::TickParams& params);
void run_npc_wander(const SystemScheduler::TickParams& params);
void run_animal_behavior(const SystemScheduler::TickParams& params);
void run_chest(const SystemScheduler::TickParams& params);
void run_item_use(const SystemScheduler::TickParams& params);
void run_dialogue(const SystemScheduler::TickParams& params);
void run_quest_interaction(const SystemScheduler::TickParams& params);
void run_action_sound(const SystemScheduler::TickParams& params);
void run_auto_tile(const SystemScheduler::TickParams& params);
void run_state(const SystemScheduler::TickParams& params);
void run_script_commands(const SystemScheduler::TickParams& params);
void run_movement(const SystemScheduler::TickParams& params);
void run_zone_trigger(const SystemScheduler::TickParams& params);
void run_spatial_index(const SystemScheduler::TickParams& params);
void run_enemy_encounter(const SystemScheduler::TickParams& params);
void run_pickup(const SystemScheduler::TickParams& params);
void run_interaction(const SystemScheduler::TickParams& params);
void run_camera_follow(const SystemScheduler::TickParams& params);
void run_animation(const SystemScheduler::TickParams& params);

[[nodiscard]] const std::vector<StageDecl>& stage_declarations() {
    static const std::vector<StageDecl> kDecls{
        {
            .stage = SchedulerStage::RemoveEntity,
            .name = "RemoveEntity",
            .mode_mask = MODE_ALL,
            .block = StageBlock::PreGate1,
            .run_main = run_remove_entity
        },
        {
            .stage = SchedulerStage::TransitionUpdatePre,
            .name = "TransitionUpdatePre",
            .mode_mask = MODE_EXPLORATION,
            .block = StageBlock::Gate1Transition,
            .run_main = run_transition_update
        },
        {
            .stage = SchedulerStage::LightTogglePre,
            .name = "LightTogglePre",
            .mode_mask = MODE_EXPLORATION,
            .block = StageBlock::Gate1Transition,
            .run_main = run_light_toggle
        },
        {
            .stage = SchedulerStage::Time,
            .name = "Time",
            .mode_mask = MODE_EXPLORATION | MODE_CUTSCENE,
            .block = StageBlock::Main,
            .run_main = run_time
        },
        {
            .stage = SchedulerStage::PlayerControl,
            .name = "PlayerControl",
            .mode_mask = MODE_EXPLORATION,
            .block = StageBlock::Main,
            .run_main = run_player_control
        },
        {
            .stage = SchedulerStage::DayNight,
            .name = "DayNight",
            .mode_mask = MODE_EXPLORATION | MODE_CUTSCENE,
            .block = StageBlock::Main,
            .run_main = run_day_night,
            .parallel_island = ParallelIsland::MidStage,
            .policy = engine::system::ExecutionPolicy::WorkerEligible,
            .ro_resources = {RESOURCE_GAME_TIME, RESOURCE_WORLD_STATE},
            .rw_resources = {RESOURCE_GLOBAL_LIGHTING_STATE}
        },
        {
            .stage = SchedulerStage::NPCWander,
            .name = "NPCWander",
            .mode_mask = MODE_EXPLORATION,
            .block = StageBlock::Main,
            .run_main = run_npc_wander,
            .parallel_island = ParallelIsland::MidStage,
            .policy = engine::system::ExecutionPolicy::WorkerEligible,
            .rw_resources = {RESOURCE_NPC_WANDER_DOMAIN}
        },
        {
            .stage = SchedulerStage::AnimalBehavior,
            .name = "AnimalBehavior",
            .mode_mask = MODE_EXPLORATION,
            .block = StageBlock::Main,
            .run_main = run_animal_behavior,
            .parallel_island = ParallelIsland::MidStage,
            .policy = engine::system::ExecutionPolicy::WorkerEligible,
            .rw_resources = {RESOURCE_ANIMAL_BEHAVIOR_DOMAIN}
        },
        {
            .stage = SchedulerStage::Chest,
            .name = "Chest",
            .mode_mask = MODE_EXPLORATION,
            .block = StageBlock::Main,
            .run_main = run_chest
        },
        {
            .stage = SchedulerStage::ItemUse,
            .name = "ItemUse",
            .mode_mask = MODE_EXPLORATION,
            .block = StageBlock::Main,
            .run_main = run_item_use
        },
        {
            .stage = SchedulerStage::Dialogue,
            .name = "Dialogue",
            .mode_mask = MODE_EXPLORATION,
            .block = StageBlock::Main,
            .run_main = run_dialogue
        },
        {
            .stage = SchedulerStage::QuestInteraction,
            .name = "QuestInteraction",
            .mode_mask = MODE_EXPLORATION,
            .block = StageBlock::Main,
            .run_main = run_quest_interaction
        },
        {
            .stage = SchedulerStage::AutoTile,
            .name = "AutoTile",
            .mode_mask = MODE_EXPLORATION,
            .block = StageBlock::Main,
            .run_main = run_auto_tile
        },
        {
            .stage = SchedulerStage::ActionSound,
            .name = "ActionSound",
            .mode_mask = MODE_EXPLORATION,
            .block = StageBlock::Main,
            .run_main = run_action_sound,
            .parallel_island = ParallelIsland::PreMovement,
            .policy = engine::system::ExecutionPolicy::WorkerEligible,
            .ro_resources = {
                entt::type_hash<game::component::StateComponent>::value(),
                entt::type_hash<engine::component::AudioComponent>::value(),
                entt::type_hash<engine::component::TransformComponent>::value(),
                entt::type_hash<engine::component::NeedRemoveTag>::value()
            },
            .rw_resources = {entt::type_hash<game::component::ActionSoundComponent>::value()}
        },
        {
            .stage = SchedulerStage::State,
            .name = "State",
            .mode_mask = MODE_EXPLORATION,
            .block = StageBlock::Main,
            .run_main = run_state,
            .parallel_island = ParallelIsland::PreMovement,
            .policy = engine::system::ExecutionPolicy::WorkerEligible,
            .ro_resources = {
                entt::type_hash<game::component::StateComponent>::value(),
                entt::type_hash<engine::component::AnimationComponent>::value(),
                entt::type_hash<engine::component::NeedRemoveTag>::value()
            },
            .rw_resources = {entt::type_hash<game::component::StateDirtyTag>::value()}
        },
        {
            .stage = SchedulerStage::ScriptCommands,
            .name = "ScriptCommands",
            .mode_mask = MODE_EXPLORATION,
            .block = StageBlock::Main,
            .run_main = run_script_commands
        },
        {
            .stage = SchedulerStage::Movement,
            .name = "Movement",
            .mode_mask = MODE_EXPLORATION,
            .block = StageBlock::Main,
            .run_main = run_movement
        },
        {
            .stage = SchedulerStage::TransitionUpdatePost,
            .name = "TransitionUpdatePost",
            .mode_mask = MODE_EXPLORATION | MODE_CUTSCENE,
            .block = StageBlock::PostTransition,
            .run_main = run_transition_update
        },
        {
            .stage = SchedulerStage::LightTogglePost,
            .name = "LightTogglePost",
            .mode_mask = MODE_EXPLORATION | MODE_CUTSCENE,
            .block = StageBlock::PostTransition,
            .run_main = run_light_toggle
        },
        {
            .stage = SchedulerStage::ZoneTrigger,
            .name = "ZoneTrigger",
            .mode_mask = MODE_EXPLORATION,
            .block = StageBlock::PostGate,
            .run_main = run_zone_trigger
        },
        {
            .stage = SchedulerStage::SpatialIndex,
            .name = "SpatialIndex",
            .mode_mask = MODE_EXPLORATION,
            .block = StageBlock::PostGate,
            .run_main = run_spatial_index,
            .parallel_island = ParallelIsland::PostGate,
            .policy = engine::system::ExecutionPolicy::WorkerEligible,
            .ro_resources = {
                entt::type_hash<engine::component::TransformDirtyTag>::value(),
                entt::type_hash<engine::component::TransformComponent>::value(),
                entt::type_hash<engine::component::AABBCollider>::value(),
                entt::type_hash<engine::component::CircleCollider>::value()
            },
            .rw_resources = {RESOURCE_SPATIAL_INDEX}
        },
        {
            .stage = SchedulerStage::CameraFollow,
            .name = "CameraFollow",
            .mode_mask = MODE_EXPLORATION,
            .block = StageBlock::PostGate,
            .run_main = run_camera_follow,
            .parallel_island = ParallelIsland::PostGate,
            .policy = engine::system::ExecutionPolicy::WorkerEligible,
            .ro_resources = {
                entt::type_hash<game::component::PlayerTag>::value(),
                entt::type_hash<engine::component::TransformComponent>::value(),
                RESOURCE_INPUT
            },
            .rw_resources = {RESOURCE_CAMERA}
        },
        {
            .stage = SchedulerStage::Animation,
            .name = "Animation",
            .mode_mask = MODE_EXPLORATION,
            .block = StageBlock::PostGate,
            .run_main = run_animation,
            .parallel_island = ParallelIsland::PostGate,
            .policy = engine::system::ExecutionPolicy::WorkerEligible,
            .rw_resources = {
                entt::type_hash<engine::component::AnimationComponent>::value(),
                entt::type_hash<engine::component::SpriteComponent>::value()
            }
        },
        {
            .stage = SchedulerStage::EnemyEncounter,
            .name = "EnemyEncounter",
            .mode_mask = MODE_EXPLORATION,
            .block = StageBlock::PostGate,
            .run_main = run_enemy_encounter
        },
        {
            .stage = SchedulerStage::Pickup,
            .name = "Pickup",
            .mode_mask = MODE_EXPLORATION,
            .block = StageBlock::PostGate,
            .run_main = run_pickup
        },
        {
            .stage = SchedulerStage::Interaction,
            .name = "Interaction",
            .mode_mask = MODE_EXPLORATION,
            .block = StageBlock::PostGate,
            .run_main = run_interaction
        }
    };
    return kDecls;
}

[[nodiscard]] std::uint8_t mode_mask_for(const GameMode mode) {
    switch (mode) {
        case GameMode::Exploration:
            return MODE_EXPLORATION;
        case GameMode::Battle:
            return MODE_BATTLE;
        case GameMode::PauseOverlay:
            return MODE_PAUSE_OVERLAY;
        case GameMode::Cutscene:
            return MODE_CUTSCENE;
    }

    return MODE_EXPLORATION;
}

[[nodiscard]] bool stage_matches_mode(const StageDecl& decl, const GameMode mode) {
    return (decl.mode_mask & mode_mask_for(mode)) != 0U;
}

[[nodiscard]] const StageDecl* find_stage_decl(const SchedulerStage stage) {
    const auto& decls = stage_declarations();
    const auto it = std::find_if(decls.begin(), decls.end(), [stage](const StageDecl& decl) {
        return decl.stage == stage;
    });
    return it == decls.end() ? nullptr : &*it;
}

[[nodiscard]] std::vector<SchedulerStage> build_profile(const GameMode mode) {
    std::vector<SchedulerStage> stages;
    for (const auto& decl : stage_declarations()) {
        if (stage_matches_mode(decl, mode)) {
            stages.push_back(decl.stage);
        }
    }
    return stages;
}

const std::vector<SchedulerStage>& exploration_profile() {
    static const std::vector<SchedulerStage> kStages = build_profile(GameMode::Exploration);
    return kStages;
}

const std::vector<SchedulerStage>& battle_profile() {
    static const std::vector<SchedulerStage> kStages = build_profile(GameMode::Battle);
    return kStages;
}

const std::vector<SchedulerStage>& pause_overlay_profile() {
    static const std::vector<SchedulerStage> kStages = build_profile(GameMode::PauseOverlay);
    return kStages;
}

const std::vector<SchedulerStage>& cutscene_profile() {
    static const std::vector<SchedulerStage> kStages = build_profile(GameMode::Cutscene);
    return kStages;
}

void trace_stage(SystemScheduler::TickResult& result, const SchedulerStage stage, const double elapsed_ms) {
    result.trace.stages.push_back(SystemScheduler::StageTrace{stage, elapsed_ms});
}

void ensure_global_lighting_state(entt::registry& registry) {
    auto* state = registry.ctx().find<engine::render::GlobalLightingState>();
    if (!state) {
        (void)registry.ctx().emplace<engine::render::GlobalLightingState>();
    }
}

void run_remove_entity(const SystemScheduler::TickParams& params) {
    if (params.systems.remove_entity_system) {
        params.systems.remove_entity_system->update(params.registry);
    }
}

void run_transition_update(const SystemScheduler::TickParams& params) {
    if (params.systems.map_transition_system) {
        params.systems.map_transition_system->update();
    }
}

void run_light_toggle(const SystemScheduler::TickParams& params) {
    if (params.systems.light_toggle_system) {
        params.systems.light_toggle_system->update();
    }
}

void run_time(const SystemScheduler::TickParams& params) {
    if (params.systems.time_system) {
        params.systems.time_system->update(params.delta_time);
    }
}

void run_day_night(const SystemScheduler::TickParams& params) {
    if (params.systems.day_night_system) {
        ensure_global_lighting_state(params.registry);
        params.systems.day_night_system->update(find_game_time(params.registry));
    }
}

void run_player_control(const SystemScheduler::TickParams& params) {
    if (params.systems.player_control_system) {
        params.systems.player_control_system->update(params.delta_time);
    }
}

void run_npc_wander(const SystemScheduler::TickParams& params) {
    if (params.systems.npc_wander_system) {
        engine::system::DeferredCommands deferred;
        params.systems.npc_wander_system->update(params.delta_time, deferred);
        deferred.drain(params.registry);
    }
}

void run_animal_behavior(const SystemScheduler::TickParams& params) {
    if (params.systems.animal_behavior_system) {
        engine::system::DeferredCommands deferred;
        params.systems.animal_behavior_system->update(params.delta_time, find_game_time(params.registry), deferred);
        deferred.drain(params.registry);
    }
}

void run_chest(const SystemScheduler::TickParams& params) {
    if (params.systems.chest_system) {
        params.systems.chest_system->update(params.delta_time);
    }
}

void run_item_use(const SystemScheduler::TickParams& params) {
    if (params.systems.item_use_system) {
        params.systems.item_use_system->update(params.delta_time);
    }
    if (params.systems.farm_system) {
        params.systems.farm_system->update(params.delta_time);
    }
}

void run_dialogue(const SystemScheduler::TickParams& params) {
    if (params.systems.dialogue_system) {
        params.systems.dialogue_system->update(params.delta_time);
    }
    if (params.systems.scripted_dialogue_lifecycle_system) {
        params.systems.scripted_dialogue_lifecycle_system->update(params.delta_time);
    }
}

void run_quest_interaction(const SystemScheduler::TickParams& params) {
    if (params.systems.quest_interaction_system) {
        params.systems.quest_interaction_system->update(params.delta_time);
    }
    if (params.systems.recruitment_interaction_system) {
        params.systems.recruitment_interaction_system->update(params.delta_time);
    }
    if (params.systems.party_recruitment_system) {
        params.systems.party_recruitment_system->update(params.delta_time);
    }
    if (params.systems.shop_interaction_system) {
        params.systems.shop_interaction_system->update(params.delta_time);
    }
}

void run_action_sound(const SystemScheduler::TickParams& params) {
    if (params.systems.action_sound_system) {
        params.systems.action_sound_system->update(params.delta_time);
    }
}

void run_auto_tile(const SystemScheduler::TickParams& params) {
    if (params.systems.auto_tile_system) {
        params.systems.auto_tile_system->update();
    }
}

void run_state(const SystemScheduler::TickParams& params) {
    if (params.systems.state_system) {
        params.systems.state_system->update();
    }
}

void run_script_commands(const SystemScheduler::TickParams& params) {
    if (params.systems.script_event_bridge) {
        params.systems.script_event_bridge->drainDeferredCommands();
    }
}

void run_movement(const SystemScheduler::TickParams& params) {
    if (params.systems.movement_system) {
        params.systems.movement_system->update(params.registry, params.delta_time);
    }
}

void run_zone_trigger(const SystemScheduler::TickParams& params) {
    if (params.systems.zone_trigger_system) {
        params.systems.zone_trigger_system->update(params.delta_time);
    }
}

void run_spatial_index(const SystemScheduler::TickParams& params) {
    if (params.systems.spatial_index_system) {
        engine::system::DeferredCommands deferred;
        params.systems.spatial_index_system->update(params.registry, deferred);
        deferred.drain(params.registry);
    }
}

void run_enemy_encounter(const SystemScheduler::TickParams& params) {
    if (params.systems.enemy_encounter_system) {
        params.systems.enemy_encounter_system->update(params.delta_time);
    }
}

void run_pickup(const SystemScheduler::TickParams& params) {
    if (params.systems.pickup_system) {
        params.systems.pickup_system->update(params.delta_time);
    }
}

void run_interaction(const SystemScheduler::TickParams& params) {
    if (params.systems.interaction_system) {
        params.systems.interaction_system->update();
    }
}

void run_camera_follow(const SystemScheduler::TickParams& params) {
    if (params.systems.camera_follow_system) {
        params.systems.camera_follow_system->update(params.delta_time);
    }
}

void run_animation(const SystemScheduler::TickParams& params) {
    if (params.systems.animation_system) {
        params.systems.animation_system->update(params.delta_time);
    }
}

void execute_stage_main_thread(const SystemScheduler::TickParams& params,
                               const StageDecl& decl,
                               SystemScheduler::TickResult& result) {
    const auto begin = std::chrono::steady_clock::now();

    if (decl.run_main) {
        decl.run_main(params);
    }

    const auto end = std::chrono::steady_clock::now();
    trace_stage(result, decl.stage, elapsedMs(begin, end));
}

[[nodiscard]] bool transition_active(const SystemScheduler::TickParams& params) {
    if (params.is_transition_active) {
        return params.is_transition_active();
    }
    return params.systems.map_transition_system && params.systems.map_transition_system->isTransitionActive();
}

[[nodiscard]] const game::data::GameTime* find_game_time(const entt::registry& registry) {
    return registry.ctx().find<game::data::GameTime>();
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
    ensure_global_lighting_state(registry);
}

void prepare_pre_movement_parallel_island_registry(entt::registry& registry) {
    // EnTT registry 的 storage 是惰性初始化；并发前主线程预热，避免 worker 触发隐式创建。
    (void)registry.storage<game::component::ActionSoundComponent>();
    (void)registry.storage<game::component::StateComponent>();
    (void)registry.storage<game::component::StateDirtyTag>();
    (void)registry.storage<game::component::ActionLockedTag>();
    (void)registry.storage<engine::component::AudioComponent>();
    (void)registry.storage<engine::component::TransformComponent>();
    (void)registry.storage<engine::component::AnimationComponent>();
    (void)registry.storage<engine::component::NeedRemoveTag>();
}

void prepare_post_gate_parallel_island_registry(entt::registry& registry) {
    // EnTT registry 的 storage 是惰性初始化；并发前主线程预热，避免 worker 触发隐式创建。
    (void)registry.storage<engine::component::SpatialIndexTag>();
    (void)registry.storage<engine::component::TransformDirtyTag>();
    (void)registry.storage<engine::component::TransformComponent>();
    (void)registry.storage<engine::component::AABBCollider>();
    (void)registry.storage<engine::component::CircleCollider>();
    (void)registry.storage<engine::component::AnimationComponent>();
    (void)registry.storage<engine::component::SpriteComponent>();
    (void)registry.storage<game::component::PlayerTag>();
}

using ParallelRunner = std::function<void(SchedulerStage, engine::system::DeferredCommands&, engine::system::TaskEventBuffer&)>;

[[nodiscard]] std::vector<const StageDecl*> parallel_stage_decls(const ParallelIsland island) {
    std::vector<const StageDecl*> decls;
    for (const auto& decl : stage_declarations()) {
        if (decl.parallel_island == island) {
            decls.push_back(&decl);
        }
    }
    return decls;
}

[[nodiscard]] std::vector<engine::system::SystemTaskDecl> make_parallel_task_decls(
    const ParallelIsland island,
    ParallelRunner runner) {
    const auto decls = parallel_stage_decls(island);
    std::vector<engine::system::SystemTaskDecl> tasks;
    tasks.reserve(decls.size());

    for (const auto* decl : decls) {
        if (decl == nullptr) {
            continue;
        }

        tasks.push_back(engine::system::SystemTaskDecl{
            .name = std::string{decl->name},
            .policy = decl->policy,
            .run = [runner, stage = decl->stage](engine::system::DeferredCommands& deferred,
                                                 engine::system::TaskEventBuffer& task_events) {
                if (runner) {
                    runner(stage, deferred, task_events);
                }
            },
            .ro_resources = decl->ro_resources,
            .rw_resources = decl->rw_resources
        });
    }

    return tasks;
}

} // namespace

namespace game::runtime {

SystemScheduler::~SystemScheduler() = default;

SystemScheduler::TickResult SystemScheduler::tick(const TickParams& params) const {
    TickResult result{};

    const auto execute_decl = [&](const StageDecl& decl) {
        execute_stage_main_thread(params, decl, result);
    };

    const auto execute_parallel_island = [&](const ParallelIsland island) {
        const auto decls = parallel_stage_decls(island);
        if (decls.empty()) {
            return;
        }

        switch (island) {
            case ParallelIsland::MidStage:
                prepare_mid_stage_parallel_island_registry(params.registry);
                setParallelIslandContext(params, find_game_time(params.registry));
                break;
            case ParallelIsland::PreMovement:
                prepare_pre_movement_parallel_island_registry(params.registry);
                setParallelIslandContext(params);
                break;
            case ParallelIsland::PostGate:
                prepare_post_gate_parallel_island_registry(params.registry);
                setParallelIslandContext(params);
                break;
            case ParallelIsland::None:
                return;
        }

        auto* scheduler = [this, island]() -> engine::system::ParallelWaveScheduler* {
            switch (island) {
                case ParallelIsland::MidStage:
                    return &midStageParallelIslandScheduler();
                case ParallelIsland::PreMovement:
                    return &preMovementParallelIslandScheduler();
                case ParallelIsland::PostGate:
                    return &postGateParallelIslandScheduler();
                case ParallelIsland::None:
                    return nullptr;
            }
            return nullptr;
        }();

        if (scheduler == nullptr || !scheduler->valid()) {
            for (const auto* decl : decls) {
                if (decl != nullptr) {
                    execute_decl(*decl);
                }
            }
            clearParallelIslandContext();
            return;
        }

        const auto elapsed = scheduler->execute(params.registry, params.dispatcher);
        clearParallelIslandContext();
        if (elapsed.size() < decls.size()) {
            for (const auto* decl : decls) {
                if (decl != nullptr) {
                    execute_decl(*decl);
                }
            }
            return;
        }

        for (std::size_t index = 0; index < decls.size(); ++index) {
            if (decls[index] != nullptr) {
                trace_stage(result, decls[index]->stage, elapsed[index]);
            }
        }
    };

    const auto execute_block = [&](const StageBlock block) {
        ParallelIsland last_executed_island = ParallelIsland::None;
        for (const auto& decl : stage_declarations()) {
            if (!stage_matches_mode(decl, params.mode) || decl.block != block) {
                continue;
            }

            if (decl.parallel_island != ParallelIsland::None) {
                if (decl.parallel_island != last_executed_island) {
                    execute_parallel_island(decl.parallel_island);
                    last_executed_island = decl.parallel_island;
                }
                continue;
            }

            execute_decl(decl);
        }
    };

    if (params.mode != GameMode::Exploration) {
        for (const auto& decl : stage_declarations()) {
            if (stage_matches_mode(decl, params.mode)) {
                execute_decl(decl);
            }
        }
        return result;
    }

    execute_block(StageBlock::PreGate1);

    if (transition_active(params)) {
        execute_block(StageBlock::Gate1Transition);
        result.gate1_triggered = true;
        return result;
    }

    execute_block(StageBlock::Main);
    execute_block(StageBlock::PostTransition);

    if (transition_active(params)) {
        result.gate2_triggered = true;
        return result;
    }

    execute_block(StageBlock::PostGate);
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

engine::async::ThreadPool* SystemScheduler::parallelThreadPool() const {
    if (!engine::platform::runtimeThreadingEnabled()) {
        return nullptr;
    }
#if defined(TF_ENABLE_RUNTIME_THREADS)
    if (!parallel_thread_pool_) {
        parallel_thread_pool_ = std::make_unique<engine::async::ThreadPool>(engine::async::ThreadPool::Options{
            .name = "SystemSchedulerParallel"
        });
    }
    return parallel_thread_pool_.get();
#else
    return nullptr;
#endif
}

engine::system::ParallelWaveScheduler& SystemScheduler::midStageParallelIslandScheduler() const {
    if (!mid_stage_parallel_island_scheduler_) {
        mid_stage_parallel_island_scheduler_ = std::make_unique<engine::system::ParallelWaveScheduler>(
            buildMidStageParallelIslandTasks(),
            parallelThreadPool());
    }
    return *mid_stage_parallel_island_scheduler_;
}

engine::system::ParallelWaveScheduler& SystemScheduler::preMovementParallelIslandScheduler() const {
    if (!pre_movement_parallel_island_scheduler_) {
        pre_movement_parallel_island_scheduler_ = std::make_unique<engine::system::ParallelWaveScheduler>(
            buildPreMovementParallelIslandTasks(),
            parallelThreadPool());
    }
    return *pre_movement_parallel_island_scheduler_;
}

engine::system::ParallelWaveScheduler& SystemScheduler::postGateParallelIslandScheduler() const {
    if (!post_gate_parallel_island_scheduler_) {
        post_gate_parallel_island_scheduler_ = std::make_unique<engine::system::ParallelWaveScheduler>(
            buildPostGateParallelIslandTasks(),
            parallelThreadPool());
    }
    return *post_gate_parallel_island_scheduler_;
}

std::vector<engine::system::SystemTaskDecl> SystemScheduler::buildMidStageParallelIslandTasks() const {
    return make_parallel_task_decls(ParallelIsland::MidStage,
                                    [this](const SchedulerStage stage,
                                           engine::system::DeferredCommands& deferred,
                                           engine::system::TaskEventBuffer& task_events) {
                                        executeParallelStage(stage, deferred, task_events);
                                    });
}

std::vector<engine::system::SystemTaskDecl> SystemScheduler::buildPreMovementParallelIslandTasks() const {
    return make_parallel_task_decls(ParallelIsland::PreMovement,
                                    [this](const SchedulerStage stage,
                                           engine::system::DeferredCommands& deferred,
                                           engine::system::TaskEventBuffer& task_events) {
                                        executeParallelStage(stage, deferred, task_events);
                                    });
}

std::vector<engine::system::SystemTaskDecl> SystemScheduler::buildPostGateParallelIslandTasks() const {
    return make_parallel_task_decls(ParallelIsland::PostGate,
                                    [this](const SchedulerStage stage,
                                           engine::system::DeferredCommands& deferred,
                                           engine::system::TaskEventBuffer& task_events) {
                                        executeParallelStage(stage, deferred, task_events);
                                    });
}

void SystemScheduler::executeParallelStage(const SchedulerStage stage,
                                           engine::system::DeferredCommands& deferred,
                                           engine::system::TaskEventBuffer& task_events) const {
    const auto* systems = parallel_island_context_.systems;
    if (systems == nullptr) {
        return;
    }

    switch (stage) {
        case SchedulerStage::DayNight:
            if (systems->day_night_system) {
                systems->day_night_system->update(parallel_island_context_.game_time);
            }
            return;
        case SchedulerStage::NPCWander:
            if (systems->npc_wander_system) {
                systems->npc_wander_system->update(parallel_island_context_.delta_time, deferred);
            }
            return;
        case SchedulerStage::AnimalBehavior:
            if (systems->animal_behavior_system) {
                systems->animal_behavior_system->update(parallel_island_context_.delta_time,
                                                        parallel_island_context_.game_time,
                                                        deferred);
            }
            return;
        case SchedulerStage::ActionSound:
            if (systems->action_sound_system) {
                systems->action_sound_system->update(parallel_island_context_.delta_time, task_events);
            }
            return;
        case SchedulerStage::State:
            if (systems->state_system) {
                systems->state_system->update(deferred, task_events);
            }
            return;
        case SchedulerStage::SpatialIndex:
            if (systems->spatial_index_system && parallel_island_context_.registry) {
                systems->spatial_index_system->update(*parallel_island_context_.registry, deferred);
            }
            return;
        case SchedulerStage::CameraFollow:
            if (systems->camera_follow_system) {
                // InputManager 仅在主线程 handleEvents() 中写入；并行岛执行期间只读快照状态。
                systems->camera_follow_system->update(parallel_island_context_.delta_time);
            }
            return;
        case SchedulerStage::Animation:
            if (systems->animation_system) {
                systems->animation_system->update(parallel_island_context_.delta_time, task_events);
            }
            return;
        default:
            return;
    }
}

void SystemScheduler::setParallelIslandContext(const TickParams& params, const game::data::GameTime* game_time) const {
    parallel_island_context_.systems = &params.systems;
    parallel_island_context_.registry = &params.registry;
    parallel_island_context_.game_time = game_time;
    parallel_island_context_.delta_time = params.delta_time;
}

void SystemScheduler::clearParallelIslandContext() const {
    parallel_island_context_ = {};
}

const char* toString(const SchedulerStage stage) {
    if (const auto* decl = find_stage_decl(stage)) {
        return decl->name.data();
    }

    return "Unknown";
}

std::string dumpPostGateParallelIslandDot() {
    engine::system::ParallelWaveScheduler scheduler(
        make_parallel_task_decls(ParallelIsland::PostGate, {}),
        nullptr);
    if (!scheduler.valid()) {
        return {};
    }
    return scheduler.dumpDot();
}

} // namespace game::runtime
