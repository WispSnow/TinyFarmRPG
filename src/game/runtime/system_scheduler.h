#pragma once

#include "engine/async/thread_pool.h"
#include "engine/system/parallel_wave_scheduler.h"
#include "game_mode.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <entt/entity/fwd.hpp>
#include <entt/signal/fwd.hpp>

namespace game::data {
struct GameTime;
}

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
    QuestInteraction,
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
    ~SystemScheduler();

    struct TickParams {
        GameMode mode{GameMode::Exploration};
        GameSystemBundle& systems;
        entt::registry& registry;
        entt::dispatcher* dispatcher{nullptr};
        float delta_time{0.0f};
        std::function<bool()> is_transition_active{};
    };

    struct StageTrace {
        SchedulerStage stage{SchedulerStage::RemoveEntity};
        double elapsed_ms{0.0};
    };

    struct TickTrace {
        std::vector<StageTrace> stages{};
    };

    struct TickResult {
        bool gate1_triggered{false};
        bool gate2_triggered{false};
        TickTrace trace{};
    };

    [[nodiscard]] TickResult tick(const TickParams& params) const;

    [[nodiscard]] static const std::vector<SchedulerStage>& profileStages(GameMode mode);

private:
    struct ParallelIslandContext {
        GameSystemBundle* systems{nullptr};
        const entt::registry* registry{nullptr};
        const game::data::GameTime* game_time{nullptr};
        float delta_time{0.0f};
    };

    engine::async::ThreadPool& parallelThreadPool() const;
    engine::system::ParallelWaveScheduler& midStageParallelIslandScheduler() const;
    engine::system::ParallelWaveScheduler& preMovementParallelIslandScheduler() const;
    engine::system::ParallelWaveScheduler& postGateParallelIslandScheduler() const;
    void setParallelIslandContext(const TickParams& params, const game::data::GameTime* game_time = nullptr) const;
    void clearParallelIslandContext() const;

    // 当前实现假设并行任务图在运行期静态不变，因此三个 scheduler 使用 lazy-init 并长期缓存。
    // 若未来支持运行时动态改图（例如模式/场景切换后调整任务集合），需要补充显式 invalidate 机制。
    mutable std::unique_ptr<engine::async::ThreadPool> parallel_thread_pool_{};
    mutable std::unique_ptr<engine::system::ParallelWaveScheduler> mid_stage_parallel_island_scheduler_{};
    mutable std::unique_ptr<engine::system::ParallelWaveScheduler> pre_movement_parallel_island_scheduler_{};
    mutable std::unique_ptr<engine::system::ParallelWaveScheduler> post_gate_parallel_island_scheduler_{};
    mutable ParallelIslandContext parallel_island_context_{};
};

[[nodiscard]] const char* toString(SchedulerStage stage);
[[nodiscard]] std::string dumpPostGateParallelIslandDot();

} // namespace game::runtime
