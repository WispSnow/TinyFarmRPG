#include "engine/system/deferred_commands.h"

#include <entt/entity/registry.hpp>

namespace engine::system {

void DeferredCommands::destroy(entt::entity entity) {
    enqueue([entity](entt::registry& registry) {
        registry.destroy(entity);
    });
}

void DeferredCommands::drain(entt::registry& registry) {
    std::vector<Command> commands;
    {
        std::lock_guard lock(mutex_);
        commands.swap(commands_);
    }

    for (auto& command : commands) {
        command(registry);
    }
}

bool DeferredCommands::empty() const {
    std::lock_guard lock(mutex_);
    return commands_.empty();
}

void DeferredCommands::enqueue(Command command) {
    if (!command) {
        return;
    }

    std::lock_guard lock(mutex_);
    commands_.push_back(std::move(command));
}

} // namespace engine::system

