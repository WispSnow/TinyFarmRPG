#pragma once

#include <entt/core/fwd.hpp>

#include <functional>
#include <string>
#include <vector>

namespace engine::system {

class DeferredCommands;
class TaskEventBuffer;

enum class ExecutionPolicy {
    MainThreadOnly,
    WorkerEligible
};

struct SystemTaskDecl {
    std::string name{};
    ExecutionPolicy policy{ExecutionPolicy::MainThreadOnly};
    std::function<void(DeferredCommands&, TaskEventBuffer&)> run{};
    std::vector<entt::id_type> ro_resources{};
    std::vector<entt::id_type> rw_resources{};
    bool sync_point{false};
};

} // namespace engine::system
