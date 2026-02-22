#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>

#include <entt/core/hashed_string.hpp>

#include "map_loading_settings.h"

namespace engine::async {
class ThreadPool;
}

namespace engine::core {
class Context;
}

namespace game::world {

enum class MapPreloadTaskState : std::uint8_t {
    NotScheduled = 0,
    // Worker running, or worker result has been enqueued to main-thread queue but not committed yet.
    Running = 1,
    // Main-thread commit command finished successfully (e.g. GPU texture uploads completed).
    Ready = 2,
    Failed = 3,
    // Ready result has been consumed by loadMap() and marked as applied.
    Applied = 4,
};

class AsyncPreloadPipeline final {
public:
    // 线程安全契约：以下 API 仅允许 owner thread（通常是主线程）调用。
    // - setLoadingSettings()
    // - clearTasks()
    // - schedule()
    // - getTaskState()
    // - markAppliedIfReady()
    // worker 线程只允许通过 AsyncPreloadTaskState 的原子字段回写状态。
    explicit AsyncPreloadPipeline(engine::core::Context& context,
                                  game::world::MapLoadingSettings settings = {});
    ~AsyncPreloadPipeline();

    AsyncPreloadPipeline(const AsyncPreloadPipeline&) = delete;
    AsyncPreloadPipeline& operator=(const AsyncPreloadPipeline&) = delete;
    AsyncPreloadPipeline(AsyncPreloadPipeline&&) = delete;
    AsyncPreloadPipeline& operator=(AsyncPreloadPipeline&&) = delete;

    void setLoadingSettings(game::world::MapLoadingSettings settings);
    [[nodiscard]] const game::world::MapLoadingSettings& loadingSettings() const { return loading_settings_; }

    void clearTasks();
    [[nodiscard]] bool schedule(entt::id_type map_id, std::string_view level_path);
    [[nodiscard]] MapPreloadTaskState getTaskState(entt::id_type map_id) const;
    void markAppliedIfReady(entt::id_type map_id);

private:
    struct AsyncPreloadTaskState {
        std::atomic<MapPreloadTaskState> state{MapPreloadTaskState::NotScheduled};
        std::atomic<std::uint64_t> generation{0};
    };

    struct AsyncPreloadTask {
        std::shared_ptr<AsyncPreloadTaskState> shared{};
        std::string level_path{};
    };

    [[nodiscard]] bool ensureOwnerThread(std::string_view api_name) const;
    void clearTasksImpl(bool enforce_owner_thread);
    void recreateThreadPoolIfEnabled();

    engine::core::Context& context_;
    std::thread::id owner_thread_id_;
    game::world::MapLoadingSettings loading_settings_{};
    std::unordered_map<entt::id_type, AsyncPreloadTask> async_preload_tasks_{};
    std::uint64_t preload_generation_counter_{0};
    std::unique_ptr<engine::async::ThreadPool> preload_thread_pool_;
};

} // namespace game::world
