#pragma once

#include "engine/debug/debug_panel.h"

#include <string>

#include <entt/entity/fwd.hpp>
#include <entt/signal/fwd.hpp>

namespace game::data {
class RpgCatalog;
}

namespace game::debug {

class BattleDebugPanel final : public engine::debug::DebugPanel {
    entt::registry& registry_;
    entt::dispatcher& dispatcher_;
    const game::data::RpgCatalog* rpg_catalog_{nullptr};

    std::string selected_troop_id_{};
    std::string status_{};

public:
    BattleDebugPanel(entt::registry& registry,
                     entt::dispatcher& dispatcher,
                     const game::data::RpgCatalog* rpg_catalog);

    [[nodiscard]] std::string_view name() const override;
    void draw(bool& is_open) override;
};

} // namespace game::debug
