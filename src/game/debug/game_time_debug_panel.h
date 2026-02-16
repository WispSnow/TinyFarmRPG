#pragma once

#include "engine/debug/debug_panel.h"
#include <entt/entity/fwd.hpp>
#include <entt/signal/fwd.hpp>

namespace game::debug {

class GameTimeDebugPanel final : public engine::debug::DebugPanel {
    entt::registry& registry_;
    entt::dispatcher& dispatcher_;

public:
    GameTimeDebugPanel(entt::registry& registry, entt::dispatcher& dispatcher);

    [[nodiscard]] std::string_view name() const override;
    void draw(bool& is_open) override;
};

} // namespace game::debug
