#include "quest_debug_panel_helpers.h"

#include "game/component/quest_log_component.h"
#include "game/data/quest_catalog.h"
#include "game/data/quest_data.h"
#include "game/domain/quest_log_ops.h"

namespace game::debug {

std::string_view questDebugFilterLabel(const QuestDebugFilter filter) {
    switch (filter) {
        case QuestDebugFilter::All: return "All";
        case QuestDebugFilter::Offerable: return questDebugStateLabel(QuestDebugState::Offerable);
        case QuestDebugFilter::Active: return questDebugStateLabel(QuestDebugState::Active);
        case QuestDebugFilter::ReadyToTurnIn: return questDebugStateLabel(QuestDebugState::ReadyToTurnIn);
        case QuestDebugFilter::Completed: return questDebugStateLabel(QuestDebugState::Completed);
        default: return "Unknown";
    }
}

std::string_view questDebugStateLabel(const QuestDebugState state) {
    switch (state) {
        case QuestDebugState::Offerable: return "Offerable";
        case QuestDebugState::Active: return "Active";
        case QuestDebugState::ReadyToTurnIn: return "ReadyToTurnIn";
        case QuestDebugState::Completed: return "Completed";
        default: return "Unknown";
    }
}

namespace detail {

QuestDebugState resolveQuestDebugState(const game::component::QuestLogComponent& quest_log,
                                       const game::data::QuestData& quest) {
    if (game::domain::quest_log_ops::isQuestCompleted(quest_log, quest.id_hash_)) {
        return QuestDebugState::Completed;
    }
    if (game::domain::quest_log_ops::isQuestReadyToTurnIn(quest_log, quest)) {
        return QuestDebugState::ReadyToTurnIn;
    }
    if (game::domain::quest_log_ops::isQuestActive(quest_log, quest.id_hash_)) {
        return QuestDebugState::Active;
    }
    return QuestDebugState::Offerable;
}

bool matchesQuestDebugFilter(const QuestDebugState state, const QuestDebugFilter filter) {
    switch (filter) {
        case QuestDebugFilter::All: return true;
        case QuestDebugFilter::Offerable: return state == QuestDebugState::Offerable;
        case QuestDebugFilter::Active: return state == QuestDebugState::Active;
        case QuestDebugFilter::ReadyToTurnIn: return state == QuestDebugState::ReadyToTurnIn;
        case QuestDebugFilter::Completed: return state == QuestDebugState::Completed;
        default: return false;
    }
}

int clampQuestObjectiveProgress(const game::component::QuestLogComponent& quest_log,
                                const game::data::QuestData& quest,
                                const game::data::QuestObjectiveData& objective) {
    if (objective.required_count_ <= 0) {
        return 0;
    }

    const std::string progress_key = game::data::makeQuestObjectiveProgressKey(quest.id_, objective.id_);
    const auto it = quest_log.objective_progress.find(progress_key);
    const int progress = (it != quest_log.objective_progress.end()) ? it->second : 0;
    if (progress <= 0) {
        return 0;
    }
    return (progress > objective.required_count_) ? objective.required_count_ : progress;
}

QuestDebugSelectionResult resolveQuestDebugSelection(const game::data::QuestCatalog* quest_catalog,
                                                     const std::string_view quest_id) {
    if (quest_catalog == nullptr) {
        return QuestDebugSelectionResult{nullptr, "QuestCatalog 不可用"};
    }
    if (quest_id.empty()) {
        return QuestDebugSelectionResult{nullptr, "未选择任务"};
    }

    if (const auto* quest = quest_catalog->findQuest(quest_id)) {
        return QuestDebugSelectionResult{quest, {}};
    }
    return QuestDebugSelectionResult{nullptr, "未找到任务定义: " + std::string(quest_id)};
}

} // namespace detail

} // namespace game::debug
