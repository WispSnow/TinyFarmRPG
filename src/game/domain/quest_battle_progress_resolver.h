#pragma once

#include "game/battle/battle_types.h"
#include "game/component/quest_log_component.h"

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>

#include <string>
#include <vector>

namespace game::data {
class QuestCatalog;
}

namespace game::domain {

struct QuestBattleProgressQuestEntry {
    std::string quest_id{};
    entt::id_type quest_id_hash{entt::null};
    std::string quest_title{};
};

struct QuestBattleProgressSummary {
    std::vector<QuestBattleProgressQuestEntry> updated_quests{};
    std::vector<QuestBattleProgressQuestEntry> became_ready_to_turn_in_quests{};

    [[nodiscard]] bool empty() const {
        return updated_quests.empty() && became_ready_to_turn_in_quests.empty();
    }
};

class QuestBattleProgressResolver final {
public:
    [[nodiscard]] QuestBattleProgressSummary apply(game::battle::BattleOutcome outcome,
                                                   const std::vector<game::battle::BattleUnit>& final_units,
                                                   const game::data::QuestCatalog& quest_catalog,
                                                   game::component::QuestLogComponent& quest_log) const;
};

} // namespace game::domain
