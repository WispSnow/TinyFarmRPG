#pragma once

#include "engine/debug/debug_panel.h"
#include "game/debug/quest_debug_panel_helpers.h"

#include <string>

#include <entt/entity/fwd.hpp>

namespace game::data {
class ItemCatalog;
}

namespace game::domain {
class QuestTurnInService;
}

namespace game::debug {

class QuestDebugPanel final : public engine::debug::DebugPanel {
    entt::registry& registry_;
    const game::data::QuestCatalog* quest_catalog_{nullptr};
    const game::data::ItemCatalog* item_catalog_{nullptr};
    game::domain::QuestTurnInService* quest_turn_in_service_{nullptr};

    QuestDebugFilter filter_{QuestDebugFilter::All};
    std::string selected_quest_id_{};
    int selected_objective_index_{0};
    int progress_step_{1};
    std::string status_{};

public:
    QuestDebugPanel(entt::registry& registry,
                    const game::data::QuestCatalog* quest_catalog,
                    const game::data::ItemCatalog* item_catalog,
                    game::domain::QuestTurnInService* quest_turn_in_service);

    std::string_view name() const override;
    void draw(bool& is_open) override;
};

} // namespace game::debug
