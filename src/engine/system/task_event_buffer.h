#pragma once

#include <entt/signal/dispatcher.hpp>

#include <functional>
#include <mutex>
#include <type_traits>
#include <utility>
#include <vector>

namespace engine::system {

class TaskEventBuffer final {
public:
    using Command = std::function<void(entt::dispatcher&)>;

    TaskEventBuffer() = default;
    ~TaskEventBuffer() = default;

    TaskEventBuffer(const TaskEventBuffer&) = delete;
    TaskEventBuffer& operator=(const TaskEventBuffer&) = delete;
    TaskEventBuffer(TaskEventBuffer&&) = delete;
    TaskEventBuffer& operator=(TaskEventBuffer&&) = delete;

    template <typename Event>
    void enqueueEvent(Event event) {
        enqueueCommand([captured = std::move(event)](entt::dispatcher& dispatcher) mutable {
            using EventType = std::decay_t<Event>;
            dispatcher.enqueue<EventType>(std::move(captured));
        });
    }

    void enqueueCommand(Command command);
    void flushTo(entt::dispatcher& dispatcher);

    [[nodiscard]] bool empty() const;

private:
    mutable std::mutex mutex_{};
    std::vector<Command> commands_{};
};

} // namespace engine::system
