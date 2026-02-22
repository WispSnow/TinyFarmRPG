#pragma once

#include "engine/system/system_task_decl.h"

#include <entt/entity/fwd.hpp>
#include <entt/graph/adjacency_matrix.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace engine::async {
class ThreadPool;
}

namespace engine::system {

class ParallelWaveScheduler final {
public:
    struct Wave {
        std::vector<std::size_t> task_indices{};
    };

    explicit ParallelWaveScheduler(std::vector<SystemTaskDecl> tasks, engine::async::ThreadPool* thread_pool = nullptr);

    [[nodiscard]] bool valid() const noexcept { return valid_; }
    [[nodiscard]] const std::vector<Wave>& waves() const noexcept { return waves_; }

    [[nodiscard]] std::vector<double> execute(entt::registry& registry) const;
    [[nodiscard]] std::string dumpDot() const;

private:
    using Graph = entt::adjacency_matrix<entt::directed_tag>;

    void rebuild();
    void buildGraph();
    void extractWaves();

    std::vector<SystemTaskDecl> tasks_{};
    engine::async::ThreadPool* thread_pool_{nullptr};
    Graph matrix_{};
    std::vector<std::size_t> vertex_to_task_index_{};
    std::vector<Wave> waves_{};
    bool valid_{true};
};

} // namespace engine::system

