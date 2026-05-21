#include "game/scene/game_scene_reward_feedback.h"

#include "game/data/item_catalog.h"

#include <spdlog/fmt/fmt.h>

#include <string>
#include <string_view>
#include <unordered_set>

namespace game::scene {
namespace {

constexpr std::string_view kVictoryText = "Victory";

void appendLine(std::string& text, const std::string& line) {
    if (line.empty()) {
        return;
    }
    if (!text.empty()) {
        text.append("\n");
    }
    text.append(line);
}

[[nodiscard]] std::string resolveItemName(const BattleRewardWritebackItemResult& item_result,
                                          const game::data::ItemCatalog* item_catalog) {
    if (item_catalog) {
        if (const auto* item = item_catalog->findItem(item_result.drop.item_id_hash)) {
            if (!item->display_name_.empty()) {
                return item->display_name_;
            }
        }
    }
    return item_result.drop.item_id;
}

void appendQuestProgressLines(std::string& text, const game::domain::QuestBattleProgressSummary& quest_progress_summary) {
    std::unordered_set<entt::id_type> ready_quests{};
    ready_quests.reserve(quest_progress_summary.became_ready_to_turn_in_quests.size());
    for (const auto& quest : quest_progress_summary.became_ready_to_turn_in_quests) {
        if (quest.quest_id_hash != entt::null) {
            ready_quests.insert(quest.quest_id_hash);
        }
    }

    for (const auto& quest : quest_progress_summary.updated_quests) {
        if (quest.quest_id_hash != entt::null && ready_quests.contains(quest.quest_id_hash)) {
            continue;
        }
        appendLine(text, fmt::format("Quest Updated: {}", quest.quest_title));
    }

    for (const auto& quest : quest_progress_summary.became_ready_to_turn_in_quests) {
        appendLine(text, fmt::format("Ready: {}", quest.quest_title));
    }
}

void appendExperienceLines(std::string& text, const game::domain::PartyExperienceGrantResult* experience_result) {
    if (!experience_result || experience_result->exp_reward <= 0) {
        return;
    }

    appendLine(text, fmt::format("Gained EXP {}", experience_result->exp_reward));
    for (const auto& grant : experience_result->actors) {
        if (!grant.leveledUp()) {
            continue;
        }
        const std::string stat_text = formatLevelUpStatText(grant);
        appendLine(text, fmt::format(
            "{} Lv.{}{}",
            grant.display_name,
            grant.new_level,
            stat_text.empty() ? std::string{} : " " + stat_text));
    }
}

} // namespace

std::string formatLevelUpStatText(const game::domain::ActorExperienceGrant& grant) {
    std::string text{};
    if (grant.hp_max_delta > 0) {
        text += fmt::format("HP +{}", grant.hp_max_delta);
    }
    if (grant.mp_max_delta > 0) {
        if (!text.empty()) {
            text += " ";
        }
        text += fmt::format("MP +{}", grant.mp_max_delta);
    }
    return text;
}

std::string formatRewardFeedback(const int gold_written_back,
                                 const std::vector<BattleRewardWritebackItemResult>& item_results,
                                 const game::data::ItemCatalog* item_catalog,
                                 const game::domain::PartyExperienceGrantResult* experience_result) {
    std::string text{};

    if (gold_written_back > 0) {
        appendLine(text, fmt::format("Gained Gold {}", gold_written_back));
    }

    appendExperienceLines(text, experience_result);

    for (const auto& item_result : item_results) {
        const std::string item_name = resolveItemName(item_result, item_catalog);
        if (item_result.accepted > 0) {
            appendLine(text, fmt::format("Gained {} x{}", item_name, item_result.accepted));
        }
        if (item_result.rejected > 0) {
            appendLine(text, fmt::format("Inventory full, missed {} x{}", item_name, item_result.rejected));
        }
    }

    if (text.empty()) {
        return std::string{kVictoryText};
    }
    return text;
}

std::string formatBattleSettlementFeedback(const BattleRewardWritebackResult& reward_result,
                                           const game::domain::PartyExperienceGrantResult* experience_result,
                                           const game::domain::QuestBattleProgressSummary& quest_progress_summary,
                                           const game::data::ItemCatalog* item_catalog) {
    if (reward_result.empty() && (!experience_result || experience_result->empty()) && quest_progress_summary.empty()) {
        return formatRewardFeedback(0, {}, item_catalog, nullptr);
    }

    std::string text{};
    if (!reward_result.empty() || (experience_result && !experience_result->empty())) {
        text = formatRewardFeedback(
            reward_result.gold_written_back,
            reward_result.item_results,
            item_catalog,
            experience_result);
    }

    appendQuestProgressLines(text, quest_progress_summary);
    return text.empty() ? std::string{kVictoryText} : text;
}

std::string formatBattleSettlementFeedback(const BattleRewardWritebackResult& reward_result,
                                           const game::domain::QuestBattleProgressSummary& quest_progress_summary,
                                           const game::data::ItemCatalog* item_catalog) {
    return formatBattleSettlementFeedback(reward_result, nullptr, quest_progress_summary, item_catalog);
}

} // namespace game::scene
