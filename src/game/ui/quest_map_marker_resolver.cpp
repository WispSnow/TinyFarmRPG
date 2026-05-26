#include "game/ui/quest_map_marker_resolver.h"

#include "game/data/quest_data.h"
#include "game/domain/quest_log_ops.h"
#include "game/ui/localized_text.h"
#include "game/ui/text_utils.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <string>
#include <unordered_set>

namespace game::ui {
namespace {

inline constexpr const char* kNoTitleFallback = "";

[[nodiscard]] int questProgressForObjective(const game::component::QuestLogComponent& quest_log,
                                            const game::data::QuestData& quest,
                                            const game::data::QuestObjectiveData& objective) {
    const std::string progress_key = game::data::makeQuestObjectiveProgressKey(quest.id_, objective.id_);
    const auto progress_it = quest_log.objective_progress.find(progress_key);
    const int progress = progress_it == quest_log.objective_progress.end() ? 0 : progress_it->second;
    return std::clamp(progress, 0, objective.required_count_);
}

[[nodiscard]] std::string localizedEnemyName(const game::runtime::LocalizationService* localization,
                                             const std::string_view enemy_id) {
    const std::string key = std::string{enemy_id} + ".name";
    if (localization && localization->hasText(key)) {
        return localization->tr(key);
    }
    return humanizeId(enemy_id, kNoTitleFallback);
}

[[nodiscard]] std::string objectiveMarkerTitle(const game::data::QuestObjectiveData& objective,
                                               const game::runtime::LocalizationService* localization) {
    if (objective.marker_ && !objective.marker_->label_.empty()) {
        return game::ui::tryLocalize(localization, objective.marker_->label_);
    }

    std::string title = humanizeId(objective.id_, kNoTitleFallback);
    if (!title.empty()) {
        return title;
    }

    title = humanizeId(objective.enemy_id_, kNoTitleFallback);
    if (!title.empty()) {
        title.append(" Target");
        return title;
    }

    return "Quest Objective";
}

[[nodiscard]] std::string objectiveProgressDescription(const game::data::QuestObjectiveData& objective,
                                                       const int current_progress,
                                                       const game::runtime::LocalizationService* localization) {
    std::string label = localizedEnemyName(localization, objective.enemy_id_);
    if (label.empty()) {
        label = objectiveMarkerTitle(objective, localization);
    }

    return game::ui::formatTextOrFallback(
        localization,
        "inventory.quest.progress.defeat_enemy_count",
        {
            {"enemy", label},
            {"current", std::to_string(current_progress)},
            {"count", std::to_string(objective.required_count_)},
        },
        [&label, current_progress, &objective] {
            return label + " " + std::to_string(current_progress) + "/" + std::to_string(objective.required_count_);
        });
}

[[nodiscard]] int questMarkerSortRank(const QuestRuntimeMarkerKind kind) {
    switch (kind) {
        case QuestRuntimeMarkerKind::TurnIn:
            return 0;
        case QuestRuntimeMarkerKind::Objective:
            return 1;
        case QuestRuntimeMarkerKind::Offer:
            return 2;
    }
    return 3;
}

void appendGiverMarker(std::vector<QuestRuntimeMarker>& out_markers,
                       const QuestGiverLocation& giver,
                       const game::data::QuestData& quest,
                       const QuestRuntimeMarkerKind kind,
                       const game::runtime::LocalizationService* localization) {
    QuestRuntimeMarker marker{};
    marker.kind = kind;
    marker.object_id = giver.object_id;
    marker.quest_id = quest.id_;
    marker.map_position = giver.map_position;
    marker.title = quest.title_.empty() ? quest.id_ : game::ui::tryLocalize(localization, quest.title_);
    if (kind == QuestRuntimeMarkerKind::TurnIn) {
        marker.type_label = game::ui::localizeTextOrFallback(localization, "map.quest.ready_to_turn_in", "Ready to Turn In");
        marker.description = game::ui::localizeTextOrFallback(localization, "map.quest.return_to_giver", "Return to the quest giver.");
    } else {
        marker.type_label = game::ui::localizeTextOrFallback(localization, "map.quest.available", "Quest Available");
        marker.description = quest.description_.empty()
                                 ? game::ui::localizeTextOrFallback(localization, "map.quest.talk_to_giver", "Talk to the quest giver.")
                                 : game::ui::tryLocalize(localization, quest.description_);
    }
    out_markers.push_back(std::move(marker));
}

void appendObjectiveMarkers(std::vector<QuestRuntimeMarker>& out_markers,
                            const entt::id_type current_map_id,
                            const game::component::QuestLogComponent& quest_log,
                            const game::data::QuestData& quest,
                            const game::runtime::LocalizationService* localization) {
    for (const auto& objective : quest.objectives_) {
        if (!objective.marker_ || objective.marker_->map_id_hash_ != current_map_id) {
            continue;
        }

        const int current_progress = questProgressForObjective(quest_log, quest, objective);
        // Hide individual completed objectives while the quest still has other work in progress.
        if (current_progress >= objective.required_count_) {
            continue;
        }

        QuestRuntimeMarker marker{};
        marker.kind = QuestRuntimeMarkerKind::Objective;
        marker.object_id = 0;
        marker.quest_id = quest.id_;
        marker.objective_id = objective.id_;
        marker.map_position = objective.marker_->position_;
        marker.title = objectiveMarkerTitle(objective, localization);
        marker.type_label = game::ui::localizeTextOrFallback(localization, "map.quest.objective", "Quest Objective");
        marker.description = objectiveProgressDescription(objective, current_progress, localization);
        out_markers.push_back(std::move(marker));
    }
}

} // namespace

std::vector<QuestRuntimeMarker> resolveQuestMapMarkers(
    const entt::id_type current_map_id,
    const game::component::QuestLogComponent& quest_log,
    const game::data::QuestCatalog& quest_catalog,
    const std::span<const QuestGiverLocation> quest_givers,
    const game::runtime::LocalizationService* localization) {
    std::vector<QuestRuntimeMarker> markers{};
    if (current_map_id == entt::null) {
        return markers;
    }

    for (const QuestGiverLocation& giver : quest_givers) {
        const auto* quest = quest_catalog.findQuest(giver.quest_id);
        if (!quest) {
            spdlog::warn("QuestMapMarkerResolver: giver object id={} references missing quest_id='{}'.",
                         giver.object_id,
                         giver.quest_id);
            continue;
        }

        if (game::domain::quest_log_ops::isQuestCompleted(quest_log, quest->id_hash_)) {
            continue;
        }

        if (game::domain::quest_log_ops::isQuestReadyToTurnIn(quest_log, *quest)) {
            appendGiverMarker(markers, giver, *quest, QuestRuntimeMarkerKind::TurnIn, localization);
            continue;
        }

        if (!game::domain::quest_log_ops::isQuestActive(quest_log, quest->id_hash_)) {
            appendGiverMarker(markers, giver, *quest, QuestRuntimeMarkerKind::Offer, localization);
        }
    }

    std::unordered_set<entt::id_type> processed_active_quests{};
    for (const std::string& active_quest_id : quest_log.active_quests) {
        const entt::id_type active_quest_id_hash = game::data::QuestCatalog::hashId(active_quest_id);
        if (active_quest_id_hash == entt::null || !processed_active_quests.insert(active_quest_id_hash).second ||
            game::domain::quest_log_ops::isQuestCompleted(quest_log, active_quest_id_hash)) {
            continue;
        }

        const auto* quest = quest_catalog.findQuest(active_quest_id_hash);
        if (!quest) {
            spdlog::warn("QuestMapMarkerResolver: active quest '{}' is missing from QuestCatalog.", active_quest_id);
            continue;
        }

        if (game::domain::quest_log_ops::isQuestReadyToTurnIn(quest_log, *quest)) {
            continue;
        }

        appendObjectiveMarkers(markers, current_map_id, quest_log, *quest, localization);
    }

    std::sort(markers.begin(), markers.end(), [](const QuestRuntimeMarker& lhs, const QuestRuntimeMarker& rhs) {
        const int lhs_rank = questMarkerSortRank(lhs.kind);
        const int rhs_rank = questMarkerSortRank(rhs.kind);
        if (lhs_rank != rhs_rank) {
            return lhs_rank < rhs_rank;
        }
        if (lhs.quest_id != rhs.quest_id) {
            return lhs.quest_id < rhs.quest_id;
        }
        if (lhs.objective_id != rhs.objective_id) {
            return lhs.objective_id < rhs.objective_id;
        }
        return lhs.object_id < rhs.object_id;
    });

    return markers;
}

} // namespace game::ui
