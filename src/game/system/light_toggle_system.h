#pragma once

#include "engine/utils/defs.h"
#include <entt/entity/fwd.hpp>
#include <entt/signal/fwd.hpp>
#include <string_view>

namespace game::defs {
struct ToggleLightRequest;
}

namespace game::system {

class LightToggleSystem final {
    entt::registry& registry_;
    entt::dispatcher& dispatcher_;

    bool wanted_on_{false};
    float radius_{128.0f};
    glm::vec2 offset_{0.0f, -10.0f};
    engine::utils::PointLightOptions options_{};

public:
    LightToggleSystem(entt::registry& registry, entt::dispatcher& dispatcher, std::string_view config_path);
    ~LightToggleSystem();

    void update();

private:
    void subscribe();
    void unsubscribe();
    void onToggleLightRequest(const game::defs::ToggleLightRequest& evt);

    void applyToPlayer(entt::entity player);
    bool loadConfig(std::string_view config_path);
};

} // namespace game::system

