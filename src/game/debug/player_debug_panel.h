#pragma once

#include "engine/debug/debug_panel.h"
#include <entt/entity/fwd.hpp>
#include <entt/signal/fwd.hpp>

namespace game::data {
class AppearanceCatalog;
class RpgCatalog;
}

namespace game::debug {

class PlayerDebugPanel final : public engine::debug::DebugPanel {
    entt::registry& registry_;
    entt::dispatcher& dispatcher_;
    const game::data::AppearanceCatalog* appearance_catalog_{nullptr};
    const game::data::RpgCatalog* rpg_catalog_{nullptr};

public:
    explicit PlayerDebugPanel(entt::registry& registry,
                              entt::dispatcher& dispatcher,
                              const game::data::AppearanceCatalog* appearance_catalog = nullptr,
                              const game::data::RpgCatalog* rpg_catalog = nullptr);

    [[nodiscard]] std::string_view name() const override;
    void draw(bool& is_open) override;
};

} // namespace game::debug
