#include "engine/system/task_event_buffer.h"

#include <entt/signal/dispatcher.hpp>

namespace engine::system {

void TaskEventBuffer::enqueueCommand(Command command) {
    if (!command) {
        return;
    }

    std::lock_guard lock(mutex_);
    commands_.push_back(std::move(command));
}

void TaskEventBuffer::flushTo(entt::dispatcher& dispatcher) {
    std::vector<Command> commands;
    {
        std::lock_guard lock(mutex_);
        commands.swap(commands_);
    }

    for (auto& command : commands) {
        command(dispatcher);
    }
}

bool TaskEventBuffer::empty() const {
    std::lock_guard lock(mutex_);
    return commands_.empty();
}

} // namespace engine::system

