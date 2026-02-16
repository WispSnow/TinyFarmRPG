#pragma once

#include <entt/entity/fwd.hpp>
#include <entt/signal/fwd.hpp>

namespace engine::core {
class Context;
}

namespace game::defs {
struct InteractRequest;
}

namespace game::system {

class RestSystem final {
    entt::registry& registry_;
    engine::core::Context& context_;
    entt::dispatcher& dispatcher_;

public:
    RestSystem(entt::registry& registry, engine::core::Context& context);
    ~RestSystem();

private:
    void onInteractRequest(const game::defs::InteractRequest& event);
};

} // namespace game::system
