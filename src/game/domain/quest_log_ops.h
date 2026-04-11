#pragma once

#include "game/component/quest_log_component.h"
#include "game/data/quest_data.h"

#include <entt/core/fwd.hpp>

namespace game::domain::quest_log_ops {

[[nodiscard]] bool isQuestActive(const game::component::QuestLogComponent& quest_log, entt::id_type quest_id_hash);
[[nodiscard]] bool isQuestCompleted(const game::component::QuestLogComponent& quest_log, entt::id_type quest_id_hash);
[[nodiscard]] bool isQuestReadyToTurnIn(const game::component::QuestLogComponent& quest_log,
                                        const game::data::QuestData& quest);
[[nodiscard]] bool tryAcceptQuest(game::component::QuestLogComponent& quest_log, const game::data::QuestData& quest);
[[nodiscard]] bool completeQuest(game::component::QuestLogComponent& quest_log, std::string_view quest_id);
void eraseQuestProgress(game::component::QuestLogComponent& quest_log, std::string_view quest_id);

} // namespace game::domain::quest_log_ops
