#include "engine/system/parallel_wave_scheduler.h"

#include "engine/async/thread_pool.h"
#include "engine/system/deferred_commands.h"

#include <entt/graph/dot.hpp>
#include <entt/graph/flow.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <future>
#include <sstream>
#include <utility>
#include <vector>

namespace engine::system {

namespace {

[[nodiscard]] double elapsedMs(const std::chrono::steady_clock::time_point& begin,
                               const std::chrono::steady_clock::time_point& end) {
    return std::chrono::duration<double, std::milli>(end - begin).count();
}

} // namespace

ParallelWaveScheduler::ParallelWaveScheduler(std::vector<SystemTaskDecl> tasks, engine::async::ThreadPool* thread_pool)
    : tasks_(std::move(tasks)),
      thread_pool_(thread_pool) {
    rebuild();
}

std::vector<double> ParallelWaveScheduler::execute(entt::registry& registry) const {
    if (!valid_) {
        spdlog::error("ParallelWaveScheduler::execute aborted: invalid task graph");
        return {};
    }

    std::vector<double> stage_elapsed_ms(tasks_.size(), 0.0);

    for (const auto& wave : waves_) {
        if (wave.task_indices.empty()) {
            continue;
        }

        DeferredCommands deferred;

        const bool all_worker_eligible = std::all_of(wave.task_indices.begin(), wave.task_indices.end(), [this](const std::size_t index) {
            return tasks_[index].policy == ExecutionPolicy::WorkerEligible;
        });
        const bool run_parallel = thread_pool_ != nullptr && all_worker_eligible && wave.task_indices.size() > 1;

        if (!run_parallel) {
            for (const std::size_t index : wave.task_indices) {
                if (index >= tasks_.size() || index >= stage_elapsed_ms.size()) {
                    spdlog::error("ParallelWaveScheduler::execute invalid task index in inline path: index={}, task_count={}",
                                  static_cast<std::uint64_t>(index),
                                  static_cast<std::uint64_t>(tasks_.size()));
                    continue;
                }
                if (!tasks_[index].run) {
                    continue;
                }

                const auto begin = std::chrono::steady_clock::now();
                tasks_[index].run(deferred);
                const auto end = std::chrono::steady_clock::now();
                stage_elapsed_ms[index] = elapsedMs(begin, end);
            }
            deferred.drain(registry);
            continue;
        }

        struct PendingFuture {
            std::size_t index{0};
            std::future<double> future{};
        };

        std::vector<PendingFuture> pending_futures;
        pending_futures.reserve(wave.task_indices.size());

        for (const std::size_t index : wave.task_indices) {
            if (index >= tasks_.size() || index >= stage_elapsed_ms.size()) {
                spdlog::error("ParallelWaveScheduler::execute invalid task index in parallel path: index={}, task_count={}",
                              static_cast<std::uint64_t>(index),
                              static_cast<std::uint64_t>(tasks_.size()));
                continue;
            }
            if (!tasks_[index].run) {
                continue;
            }

            // 复制函数对象，避免 worker 间接访问调度器生命周期对象。
            auto run_task = tasks_[index].run;
            auto future = thread_pool_->submitFuture([run_task = std::move(run_task), &deferred]() mutable {
                const auto begin = std::chrono::steady_clock::now();
                run_task(deferred);
                const auto end = std::chrono::steady_clock::now();
                return elapsedMs(begin, end);
            });

            if (future.valid()) {
                pending_futures.push_back(PendingFuture{
                    .index = index,
                    .future = std::move(future)
                });
                continue;
            }

            const auto begin = std::chrono::steady_clock::now();
            tasks_[index].run(deferred);
            const auto end = std::chrono::steady_clock::now();
            stage_elapsed_ms[index] = elapsedMs(begin, end);
        }

        for (auto& pending : pending_futures) {
            if (!pending.future.valid()) {
                continue;
            }
            stage_elapsed_ms[pending.index] = pending.future.get();
        }

        deferred.drain(registry);
    }

    return stage_elapsed_ms;
}

std::string ParallelWaveScheduler::dumpDot() const {
    std::ostringstream out;
    entt::dot(out, matrix_, [this](auto& node_out, const auto vertex) {
        const auto index = vertex_to_task_index_[static_cast<std::size_t>(vertex)];
        node_out << "label=\"" << tasks_[index].name << "\",shape=\"box\"";
    });
    return out.str();
}

void ParallelWaveScheduler::rebuild() {
    valid_ = true;
    buildGraph();
    if (!valid_) {
        return;
    }
    extractWaves();
}

void ParallelWaveScheduler::buildGraph() {
    entt::flow builder;
    for (std::size_t index = 0; index < tasks_.size(); ++index) {
        builder.bind(static_cast<entt::id_type>(index));
        if (!tasks_[index].ro_resources.empty()) {
            builder.ro(tasks_[index].ro_resources.begin(), tasks_[index].ro_resources.end());
        }
        if (!tasks_[index].rw_resources.empty()) {
            builder.rw(tasks_[index].rw_resources.begin(), tasks_[index].rw_resources.end());
        }
        if (tasks_[index].sync_point) {
            builder.sync();
        }
    }

    matrix_ = builder.graph();

    vertex_to_task_index_.clear();
    vertex_to_task_index_.resize(matrix_.size());
    for (const auto vertex : matrix_.vertices()) {
        const auto task_index = static_cast<std::size_t>(builder[vertex]);
        if (task_index >= tasks_.size()) {
            spdlog::error("ParallelWaveScheduler::buildGraph invalid vertex mapping: vertex={}, task_index={}, task_count={}",
                          static_cast<std::uint64_t>(vertex),
                          static_cast<std::uint64_t>(task_index),
                          static_cast<std::uint64_t>(tasks_.size()));
            valid_ = false;
            return;
        }
        vertex_to_task_index_[vertex] = task_index;
    }
}

void ParallelWaveScheduler::extractWaves() {
    waves_.clear();
    if (matrix_.size() == 0) {
        return;
    }

    const auto vertex_count = matrix_.size();
    std::vector<std::size_t> in_degree(vertex_count, 0);
    for (const auto [from, to] : matrix_.edges()) {
        (void)from;
        if (to >= vertex_count) {
            spdlog::error("ParallelWaveScheduler::extractWaves invalid edge target: to={}, vertex_count={}",
                          static_cast<std::uint64_t>(to),
                          static_cast<std::uint64_t>(vertex_count));
            valid_ = false;
            waves_.clear();
            return;
        }
        ++in_degree[to];
    }

    std::vector<std::size_t> current_wave;
    current_wave.reserve(vertex_count);
    for (std::size_t vertex = 0; vertex < vertex_count; ++vertex) {
        if (in_degree[vertex] == 0) {
            current_wave.push_back(vertex);
        }
    }

    std::size_t scheduled = 0;

    while (!current_wave.empty()) {
        Wave wave;
        wave.task_indices.reserve(current_wave.size());
        for (const auto vertex : current_wave) {
            if (vertex >= vertex_to_task_index_.size()) {
                spdlog::error("ParallelWaveScheduler::extractWaves invalid vertex index: vertex={}, mapped_size={}",
                              static_cast<std::uint64_t>(vertex),
                              static_cast<std::uint64_t>(vertex_to_task_index_.size()));
                valid_ = false;
                waves_.clear();
                return;
            }

            const auto task_index = vertex_to_task_index_[vertex];
            if (task_index >= tasks_.size()) {
                spdlog::error("ParallelWaveScheduler::extractWaves invalid task index mapping: vertex={}, task_index={}, task_count={}",
                              static_cast<std::uint64_t>(vertex),
                              static_cast<std::uint64_t>(task_index),
                              static_cast<std::uint64_t>(tasks_.size()));
                valid_ = false;
                waves_.clear();
                return;
            }

            wave.task_indices.push_back(task_index);
        }
        waves_.push_back(std::move(wave));
        scheduled += current_wave.size();

        std::vector<std::size_t> next_wave;
        for (const auto vertex : current_wave) {
            for (const auto [from, to] : matrix_.out_edges(vertex)) {
                (void)from;
                if (to >= in_degree.size()) {
                    spdlog::error("ParallelWaveScheduler::extractWaves invalid out edge target: to={}, degree_size={}",
                                  static_cast<std::uint64_t>(to),
                                  static_cast<std::uint64_t>(in_degree.size()));
                    valid_ = false;
                    waves_.clear();
                    return;
                }
                if (--in_degree[to] == 0) {
                    next_wave.push_back(to);
                }
            }
        }
        current_wave = std::move(next_wave);
    }

    if (scheduled != vertex_count) {
        spdlog::error("ParallelWaveScheduler::extractWaves detected invalid graph: scheduled={}, total={}",
                      static_cast<std::uint64_t>(scheduled),
                      static_cast<std::uint64_t>(vertex_count));
        valid_ = false;
        waves_.clear();
        assert(false && "ParallelWaveScheduler graph contains cycle or unreachable vertices");
    }
}

} // namespace engine::system
