#include "game/world/async_preload_pipeline.h"

#include "engine/async/main_thread_command_queue.h"
#include "engine/async/thread_pool.h"
#include "engine/core/context.h"
#include "engine/loader/level_loader.h"
#include "engine/resource/image_decode_service.h"
#include "engine/resource/resource_manager.h"

#include <algorithm>
#include <chrono>
#include <vector>

#include <spdlog/spdlog.h>

namespace game::world {

AsyncPreloadPipeline::AsyncPreloadPipeline(engine::core::Context& context,
                                           game::world::MapLoadingSettings settings)
    : context_(context),
      owner_thread_id_(std::this_thread::get_id()),
      loading_settings_(std::move(settings)) {
    recreateThreadPoolIfEnabled();
}

AsyncPreloadPipeline::~AsyncPreloadPipeline() {
    // 生命周期约束：析构时必须先 stop+join worker，再释放任务状态。
    // 这样可以保证 worker/lambda 捕获的 Context 子系统引用在任务结束前仍有效。
    clearTasksImpl(false);
}

void AsyncPreloadPipeline::setLoadingSettings(game::world::MapLoadingSettings settings) {
    if (!ensureOwnerThread("setLoadingSettings")) {
        return;
    }

    loading_settings_ = std::move(settings);
    clearTasksImpl(false);
    recreateThreadPoolIfEnabled();
}

void AsyncPreloadPipeline::clearTasks() {
    clearTasksImpl(true);
}

bool AsyncPreloadPipeline::schedule(entt::id_type map_id, std::string_view level_path) {
    if (!ensureOwnerThread("schedule")) {
        return false;
    }

    if (!loading_settings_.async_preload_enabled || map_id == entt::null || level_path.empty()) {
        return false;
    }

    if (!preload_thread_pool_) {
        recreateThreadPoolIfEnabled();
    }
    if (!preload_thread_pool_) {
        return false;
    }

    auto& task = async_preload_tasks_[map_id];
    if (!task.shared) {
        task.shared = std::make_shared<AsyncPreloadTaskState>();
    }

    task.level_path = std::string(level_path);
    const std::uint64_t generation = ++preload_generation_counter_;
    task.shared->generation.store(generation, std::memory_order_release);
    task.shared->state.store(MapPreloadTaskState::Running, std::memory_order_release);

    auto shared_state = task.shared;
    auto* resource_manager = &context_.getResourceManager();
    auto* main_thread_queue = &context_.getMainThreadCommandQueue();
    const std::chrono::milliseconds submit_wait{static_cast<int>(loading_settings_.async_submit_wait_ms)};
    const std::chrono::milliseconds command_wait{static_cast<int>(loading_settings_.async_command_wait_ms)};
    const bool timing_enabled = loading_settings_.log_timings;
    const auto task_scheduled_at = std::chrono::steady_clock::now();
    const std::string path_copy = task.level_path;

    if (timing_enabled) {
        spdlog::info(
            "AsyncPreloadPipeline::schedule: submit map_id={}, worker_queue_depth={}, command_queue_depth={}",
            map_id,
            preload_thread_pool_->pendingTaskCount(),
            main_thread_queue->size());
    }

    const bool submitted = preload_thread_pool_->submit(
        [map_id,
         generation,
         shared_state,
         path_copy,
         resource_manager,
         main_thread_queue,
         command_wait,
         timing_enabled,
         task_scheduled_at]() mutable {
            struct DecodedTextureEntry {
                std::string texture_path{};
                engine::resource::DecodedImage decoded{};
            };

            const auto try_set_state = [shared_state, generation](MapPreloadTaskState next) {
                if (!shared_state) {
                    return false;
                }
                if (shared_state->generation.load(std::memory_order_acquire) != generation) {
                    return false;
                }
                shared_state->state.store(next, std::memory_order_release);
                return true;
            };

            const auto is_stale = [shared_state, generation] {
                if (!shared_state) {
                    return true;
                }
                return shared_state->generation.load(std::memory_order_acquire) != generation;
            };

            const auto worker_start_at = std::chrono::steady_clock::now();
            auto preprocess = engine::loader::LevelLoader::preprocessLevelDataWorker(path_copy);
            if (is_stale()) {
                return;
            }
            if (!preprocess.success) {
                if (try_set_state(MapPreloadTaskState::Failed)) {
                    spdlog::warn(
                        "AsyncPreloadPipeline::schedule: preprocess failed map_id={}, path='{}', error='{}'",
                        map_id,
                        path_copy,
                        preprocess.error);
                }
                return;
            }

            auto preprocess_end_at = std::chrono::steady_clock::now();
            std::vector<DecodedTextureEntry> decoded_textures{};
            decoded_textures.reserve(preprocess.data.texture_paths.size());
            for (const auto& texture_path : preprocess.data.texture_paths) {
                if (is_stale()) {
                    return;
                }
                auto decoded = engine::resource::ImageDecodeService::decodeRGBA(texture_path);
                if (!decoded.has_value()) {
                    if (try_set_state(MapPreloadTaskState::Failed)) {
                        spdlog::warn(
                            "AsyncPreloadPipeline::schedule: worker decode failed map_id={}, texture='{}'",
                            map_id,
                            texture_path);
                    }
                    return;
                }
                decoded_textures.push_back(DecodedTextureEntry{
                    .texture_path = texture_path,
                    .decoded = std::move(*decoded),
                });
            }
            auto decode_end_at = std::chrono::steady_clock::now();

            const bool enqueued = main_thread_queue->enqueueWithWait(
                [map_id,
                 generation,
                 shared_state,
                 resource_manager,
                 decoded_textures = std::move(decoded_textures),
                 timing_enabled]() mutable {
                    if (!shared_state || shared_state->generation.load(std::memory_order_acquire) != generation) {
                        return;
                    }

                    const auto commit_start_at = std::chrono::steady_clock::now();
                    bool success = true;
                    for (const auto& texture : decoded_textures) {
                        if (!texture.decoded.valid() || texture.texture_path.empty()) {
                            continue;
                        }
                        const entt::id_type texture_id = entt::hashed_string{
                            texture.texture_path.c_str(),
                            texture.texture_path.size()}.value();
                        if (!resource_manager->loadTextureFromDecoded(texture_id, texture.texture_path, texture.decoded)) {
                            success = false;
                            spdlog::warn(
                                "AsyncPreloadPipeline::schedule: main-thread upload failed map_id={}, texture='{}'",
                                map_id,
                                texture.texture_path);
                        }
                    }

                    if (timing_enabled) {
                        const auto commit_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - commit_start_at).count();
                        spdlog::info(
                            "AsyncPreloadPipeline::schedule: commit done map_id={}, texture_count={}, commit_ms={}",
                            map_id,
                            decoded_textures.size(),
                            commit_ms);
                    }

                    if (shared_state->generation.load(std::memory_order_acquire) == generation) {
                        shared_state->state.store(
                            success ? MapPreloadTaskState::Ready : MapPreloadTaskState::Failed,
                            std::memory_order_release);
                    }
                },
                command_wait);

            if (!enqueued) {
                if (try_set_state(MapPreloadTaskState::Failed)) {
                    spdlog::warn(
                        "AsyncPreloadPipeline::schedule: main-thread command enqueue failed map_id={}, path='{}'",
                        map_id,
                        path_copy);
                }
                return;
            }

            if (timing_enabled) {
                const auto queue_wait_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    worker_start_at - task_scheduled_at).count();
                const auto preprocess_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    preprocess_end_at - worker_start_at).count();
                const auto decode_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    decode_end_at - preprocess_end_at).count();
                spdlog::info(
                    "AsyncPreloadPipeline::schedule: worker done map_id={}, tileset_count={}, texture_count={}, queued_ms={}, preprocess_ms={}, decode_ms={}, command_queue_depth={}",
                    map_id,
                    preprocess.data.tilesets.size(),
                    preprocess.data.texture_paths.size(),
                    queue_wait_ms,
                    preprocess_ms,
                    decode_ms,
                    main_thread_queue->size());
            }
        },
        submit_wait);

    if (!submitted) {
        task.shared->state.store(MapPreloadTaskState::Failed, std::memory_order_release);
        spdlog::warn(
            "AsyncPreloadPipeline::schedule: worker submit failed map_id={}, path='{}'",
            map_id,
            task.level_path);
    }

    return submitted;
}

MapPreloadTaskState AsyncPreloadPipeline::getTaskState(entt::id_type map_id) const {
    if (!ensureOwnerThread("getTaskState")) {
        return MapPreloadTaskState::NotScheduled;
    }

    const auto it = async_preload_tasks_.find(map_id);
    if (it == async_preload_tasks_.end() || !it->second.shared) {
        return MapPreloadTaskState::NotScheduled;
    }

    return it->second.shared->state.load(std::memory_order_acquire);
}

void AsyncPreloadPipeline::markAppliedIfReady(entt::id_type map_id) {
    if (!ensureOwnerThread("markAppliedIfReady")) {
        return;
    }

    if (const auto it = async_preload_tasks_.find(map_id); it != async_preload_tasks_.end() && it->second.shared) {
        if (it->second.shared->state.load(std::memory_order_acquire) == MapPreloadTaskState::Ready) {
            it->second.shared->state.store(MapPreloadTaskState::Applied, std::memory_order_release);
        }
    }
}

bool AsyncPreloadPipeline::ensureOwnerThread(std::string_view api_name) const {
    if (std::this_thread::get_id() == owner_thread_id_) {
        return true;
    }

    spdlog::warn("AsyncPreloadPipeline::{} called from non-owner thread", api_name);
    return false;
}

void AsyncPreloadPipeline::clearTasksImpl(bool enforce_owner_thread) {
    if (enforce_owner_thread && !ensureOwnerThread("clearTasks")) {
        return;
    }

    // 先 bump generation，使晚到任务在提交前失效；再 stop+join worker。
    for (auto& [_, task] : async_preload_tasks_) {
        if (!task.shared) {
            continue;
        }
        task.shared->generation.fetch_add(1, std::memory_order_acq_rel);
        task.shared->state.store(MapPreloadTaskState::NotScheduled, std::memory_order_release);
    }
    async_preload_tasks_.clear();
    preload_generation_counter_ = 0;

    if (preload_thread_pool_) {
        preload_thread_pool_->stop();
        preload_thread_pool_.reset();
    }
}

void AsyncPreloadPipeline::recreateThreadPoolIfEnabled() {
    if (!loading_settings_.async_preload_enabled) {
        return;
    }

    preload_thread_pool_ = std::make_unique<engine::async::ThreadPool>(engine::async::ThreadPool::Options{
        .worker_count = std::max<std::size_t>(1, loading_settings_.async_worker_count),
        .queue_capacity = std::max<std::size_t>(1, loading_settings_.async_queue_capacity),
        .name = "MapPreloadThreadPool",
    });
}

} // namespace game::world
