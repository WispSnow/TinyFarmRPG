#include "game/domain/quest_battle_progress_resolver.h"

#include "game/data/quest_catalog.h"
#include "game/domain/quest_log_ops.h"

#include <entt/core/hashed_string.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace game::domain {
namespace {

[[nodiscard]] std::unordered_map<entt::id_type, int> collectDefeatedEnemyCounts(
    const std::vector<game::battle::BattleUnit>& final_units) {
    std::unordered_map<entt::id_type, int> counts{};

    for (const auto& unit : final_units) {
        if (unit.side != game::battle::BattleSide::Enemy || unit.isAlive() || !unit.source_enemy_id.has_value()) {
            continue;
        }

        const std::string_view enemy_id = *unit.source_enemy_id;
        if (enemy_id.empty()) {
            continue;
        }

        counts[entt::hashed_string{enemy_id.data(), enemy_id.size()}.value()] += 1;
    }

    return counts;
}

void appendQuestEntry(std::vector<QuestBattleProgressQuestEntry>& entries,
                      std::unordered_set<entt::id_type>& seen_hashes,
                      const game::data::QuestData& quest) {
    if (!seen_hashes.insert(quest.id_hash_).second) {
        return;
    }

    entries.push_back(QuestBattleProgressQuestEntry{
        .quest_id = quest.id_,
        .quest_id_hash = quest.id_hash_,
        .quest_title = quest.title_});
}

} // namespace

QuestBattleProgressSummary QuestBattleProgressResolver::apply(const game::battle::BattleOutcome outcome,
                                                              const std::vector<game::battle::BattleUnit>& final_units,
                                                              const game::data::QuestCatalog& quest_catalog,
                                                              game::component::QuestLogComponent& quest_log) const {
    QuestBattleProgressSummary summary{};
    if (outcome != game::battle::BattleOutcome::Victory) {
        return summary;
    }

    const auto defeated_enemy_counts = collectDefeatedEnemyCounts(final_units);
    std::unordered_set<entt::id_type> processed_active_quests{};
    std::unordered_set<entt::id_type> updated_hashes{};
    std::unordered_set<entt::id_type> ready_hashes{};

    for (const std::string& active_quest_id : quest_log.active_quests) {
        const entt::id_type quest_id_hash = game::data::QuestCatalog::hashId(active_quest_id);
        if (quest_id_hash == entt::null || !processed_active_quests.insert(quest_id_hash).second) {
            continue;
        }
        if (game::domain::quest_log_ops::isQuestCompleted(quest_log, quest_id_hash)) {
            continue;
        }

        const auto* quest = quest_catalog.findQuest(quest_id_hash);
        if (!quest) {
            spdlog::warn("QuestBattleProgressResolver: active quest '{}' 未在 QuestCatalog 中找到。", active_quest_id);
            continue;
        }

        const bool was_ready_to_turn_in = game::domain::quest_log_ops::isQuestReadyToTurnIn(quest_log, *quest);
        bool quest_updated = false;

        for (const auto& objective : quest->objectives_) {
            if (objective.kind_ != game::data::QuestObjectiveKind::DefeatEnemyCount) {
                continue;
            }

            const std::string progress_key = game::data::makeQuestObjectiveProgressKey(quest->id_, objective.id_);
            auto [progress_it, inserted] = quest_log.objective_progress.try_emplace(progress_key, 0);
            if (inserted) {
                spdlog::debug("QuestBattleProgressResolver: quest '{}' objective '{}' 缺少 progress key，按 0 补写回。",
                              quest->id_,
                              objective.id_);
            }

            const int clamped_before = std::clamp(progress_it->second, 0, objective.required_count_);
            if (clamped_before != progress_it->second) {
                progress_it->second = clamped_before;
            }

            const auto count_it = defeated_enemy_counts.find(objective.enemy_id_hash_);
            if (count_it == defeated_enemy_counts.end() || count_it->second <= 0) {
                continue;
            }

            const int after = std::clamp(clamped_before + count_it->second, 0, objective.required_count_);
            if (after <= clamped_before) {
                continue;
            }

            progress_it->second = after;
            quest_updated = true;
        }

        if (!quest_updated) {
            continue;
        }

        appendQuestEntry(summary.updated_quests, updated_hashes, *quest);

        const bool is_ready_to_turn_in = game::domain::quest_log_ops::isQuestReadyToTurnIn(quest_log, *quest);
        if (!was_ready_to_turn_in && is_ready_to_turn_in) {
            appendQuestEntry(summary.became_ready_to_turn_in_quests, ready_hashes, *quest);
        }
    }

    return summary;
}

} // namespace game::domain
