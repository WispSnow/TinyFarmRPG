#pragma once

#include <entt/entity/fwd.hpp>
#include <entt/signal/fwd.hpp>

#include <memory>

namespace engine::core {
class Context;
}

namespace game::data {
class AppearanceCatalog;
}

namespace game::defs {
struct InteractCommand;
}

namespace game::system {

class ClosetInteractionSystem final {
    entt::registry& registry_;
    engine::core::Context& context_;
    entt::dispatcher& dispatcher_;
    std::shared_ptr<game::data::AppearanceCatalog> appearance_catalog_{};

public:
    ClosetInteractionSystem(entt::registry& registry,
                            engine::core::Context& context,
                            std::shared_ptr<game::data::AppearanceCatalog> appearance_catalog);
    ~ClosetInteractionSystem();

private:
    void onInteractCommand(const game::defs::InteractCommand& event);
};

} // namespace game::system
