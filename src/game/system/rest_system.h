#pragma once

#include <entt/entity/fwd.hpp>
#include <entt/signal/fwd.hpp>

namespace engine::core {
class Context;
}

namespace game::defs {
struct InteractCommand;
struct RestConfirmRequest;
}

namespace game::data {
class RpgCatalog;
}

namespace game::system {

class RestSystem final {
    entt::registry& registry_;
    engine::core::Context& context_;
    entt::dispatcher& dispatcher_;
    const game::data::RpgCatalog* rpg_catalog_{nullptr};

public:
    RestSystem(entt::registry& registry,
               engine::core::Context& context,
               const game::data::RpgCatalog* rpg_catalog);
    ~RestSystem();

private:
    void onInteractCommand(const game::defs::InteractCommand& event);
    void onRestConfirmRequest(const game::defs::RestConfirmRequest& event);
};

} // namespace game::system
