#pragma once

#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <entt/entity/registry.hpp>

#include <functional>
#include <mutex>
#include <utility>
#include <vector>

namespace engine::system {

class DeferredCommands final {
public:
    using Command = std::function<void(entt::registry&)>;

    DeferredCommands() = default;
    ~DeferredCommands() = default;

    DeferredCommands(const DeferredCommands&) = delete;
    DeferredCommands& operator=(const DeferredCommands&) = delete;
    DeferredCommands(DeferredCommands&&) = delete;
    DeferredCommands& operator=(DeferredCommands&&) = delete;

    template <typename Component, typename... Args>
    void emplaceOrReplace(const entt::entity entity, Args&&... args) {
        enqueue([entity, ... captured = std::forward<Args>(args)](entt::registry& registry) mutable {
            registry.emplace_or_replace<Component>(entity, std::move(captured)...);
        });
    }

    template <typename Component>
    void remove(const entt::entity entity) {
        enqueue([entity](entt::registry& registry) {
            registry.remove<Component>(entity);
        });
    }

    void destroy(entt::entity entity);
    void drain(entt::registry& registry);

    [[nodiscard]] bool empty() const;

private:
    void enqueue(Command command);

    mutable std::mutex mutex_{};
    std::vector<Command> commands_{};
};

} // namespace engine::system

