#pragma once

#include "system_bundle.h"

#include <memory>

#include <entt/entity/fwd.hpp>

namespace engine::core {
class Context;
}

namespace engine::scene {
class Scene;
}

namespace game::data {
struct GameTime;
}

namespace game::runtime {

class GameRuntimeAssembler final {
public:
    struct ServiceBuildParams {
        engine::scene::Scene& scene;
        engine::core::Context& context;
        entt::registry& registry;
        std::shared_ptr<game::data::GameTime>& game_time;
        GameRuntimeServices& services;
        bool load_initial_map{true};
    };

    struct SystemBuildParams {
        engine::core::Context& context;
        entt::registry& registry;
        GameRuntimeServices& services;
        GameSystemBundle& systems;
    };

    [[nodiscard]] static bool assembleServices(ServiceBuildParams params);
    [[nodiscard]] static bool assembleSystems(SystemBuildParams params);
};

} // namespace game::runtime
