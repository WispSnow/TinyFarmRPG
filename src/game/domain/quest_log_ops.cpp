#include "game/domain/quest_log_ops.h"

#include <entt/core/hashed_string.hpp>

#include <algorithm>
#include <string_view>

namespace game::domain::quest_log_ops {
namespace {

[[nodiscard]] entt::id_type hashQuestId(const std::string_view quest_id) {
    if (quest_id.empty()) {
        return entt::null;
    }
    return entt::hashed_string{quest_id.data(), quest_id.size()}.value();
}

[[nodiscard]] bool containsQuestId(const std::vector<std::string>& ids, const entt::id_type quest_id_hash) {
    return std::any_of(ids.begin(), ids.end(), [quest_id_hash](const std::string& id) {
        return hashQuestId(id) == quest_id_hash;
    });
}

} // namespace

bool isQuestActive(const game::component::QuestLogComponent& quest_log, const entt::id_type quest_id_hash) {
    return containsQuestId(quest_log.active_quests, quest_id_hash);
}

bool isQuestCompleted(const game::component::QuestLogComponent& quest_log, const entt::id_type quest_id_hash) {
    return containsQuestId(quest_log.completed_quests, quest_id_hash);
}

bool isQuestReadyToTurnIn(const game::component::QuestLogComponent& quest_log, const game::data::QuestData& quest) {
    if (quest.id_hash_ == entt::null || !isQuestActive(quest_log, quest.id_hash_)) {
        return false;
    }

    for (const auto& objective : quest.objectives_) {
        const std::string progress_key = game::data::makeQuestObjectiveProgressKey(quest.id_, objective.id_);
        const auto progress_it = quest_log.objective_progress.find(progress_key);
        if (progress_it == quest_log.objective_progress.end() || progress_it->second < objective.required_count_) {
            return false;
        }
    }

    return true;
}

bool tryAcceptQuest(game::component::QuestLogComponent& quest_log, const game::data::QuestData& quest) {
    if (quest.id_.empty() || quest.id_hash_ == entt::null) {
        return false;
    }
    if (isQuestActive(quest_log, quest.id_hash_) || isQuestCompleted(quest_log, quest.id_hash_)) {
        return false;
    }

    quest_log.active_quests.push_back(quest.id_);
    for (const auto& objective : quest.objectives_) {
        quest_log.objective_progress.try_emplace(
            game::data::makeQuestObjectiveProgressKey(quest.id_, objective.id_),
            0);
    }
    return true;
}

bool completeQuest(game::component::QuestLogComponent& quest_log, const std::string_view quest_id) {
    const entt::id_type quest_id_hash = hashQuestId(quest_id);
    if (quest_id_hash == entt::null || !isQuestActive(quest_log, quest_id_hash)) {
        return false;
    }

    quest_log.active_quests.erase(
        std::remove_if(
            quest_log.active_quests.begin(),
            quest_log.active_quests.end(),
            [quest_id_hash](const std::string& active_id) {
                return hashQuestId(active_id) == quest_id_hash;
            }),
        quest_log.active_quests.end());

    if (!isQuestCompleted(quest_log, quest_id_hash)) {
        quest_log.completed_quests.push_back(std::string(quest_id));
    }

    return true;
}

void eraseQuestProgress(game::component::QuestLogComponent& quest_log, const std::string_view quest_id) {
    if (quest_id.empty()) {
        return;
    }

    const std::string prefix = std::string(quest_id) + std::string(game::data::kQuestObjectiveProgressKeySeparator);
    for (auto it = quest_log.objective_progress.begin(); it != quest_log.objective_progress.end();) {
        if (it->first.rfind(prefix, 0) == 0) {
            it = quest_log.objective_progress.erase(it);
            continue;
        }
        ++it;
    }
}

} // namespace game::domain::quest_log_ops
