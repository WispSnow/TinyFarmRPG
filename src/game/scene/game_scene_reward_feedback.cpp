#include "game/scene/game_scene_reward_feedback.h"

#include "game/data/item_catalog.h"
#include "game/ui/localized_text.h"

#include <spdlog/fmt/fmt.h>

#include <string>
#include <unordered_set>

namespace game::scene {
namespace {

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
                                          const game::data::ItemCatalog* item_catalog,
                                          const game::runtime::LocalizationService* localization) {
    if (item_catalog) {
        if (const auto* item = item_catalog->findItem(item_result.drop.item_id_hash)) {
            if (!item->display_name_.empty()) {
                return game::ui::tryLocalize(localization, item->display_name_);
            }
        }
    }
    return item_result.drop.item_id;
}

void appendQuestProgressLines(std::string& text,
                              const game::domain::QuestBattleProgressSummary& quest_progress_summary,
                              const game::runtime::LocalizationService* localization) {
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
        const std::string quest_title = game::ui::tryLocalize(localization, quest.quest_title);
        appendLine(
            text,
            game::ui::formatTextOrFallback(
                localization,
                "reward.quest_updated",
                {{"quest", quest_title}},
                [&quest_title] { return fmt::format("Quest Updated: {}", quest_title); }));
    }

    for (const auto& quest : quest_progress_summary.became_ready_to_turn_in_quests) {
        const std::string quest_title = game::ui::tryLocalize(localization, quest.quest_title);
        appendLine(
            text,
            game::ui::formatTextOrFallback(
                localization,
                "reward.quest_ready",
                {{"quest", quest_title}},
                [&quest_title] { return fmt::format("Ready: {}", quest_title); }));
    }
}

void appendExperienceLines(std::string& text,
                           const game::domain::PartyExperienceGrantResult* experience_result,
                           const game::runtime::LocalizationService* localization) {
    if (!experience_result || experience_result->exp_reward <= 0) {
        return;
    }

    appendLine(
        text,
        game::ui::formatTextOrFallback(
            localization,
            "reward.exp_gained",
            {{"amount", std::to_string(experience_result->exp_reward)}},
            [experience_result] { return fmt::format("Gained EXP {}", experience_result->exp_reward); }));
    for (const auto& grant : experience_result->actors) {
        if (!grant.leveledUp()) {
            continue;
        }
        const std::string stat_text = formatLevelUpStatText(grant);
        const std::string actor_name = game::ui::tryLocalize(localization, grant.display_name);
        appendLine(text, fmt::format(
                             "{} Lv.{}{}",
                             actor_name,
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
                                 const game::domain::PartyExperienceGrantResult* experience_result,
                                 const game::runtime::LocalizationService* localization) {
    std::string text{};

    if (gold_written_back > 0) {
        appendLine(
            text,
            game::ui::formatTextOrFallback(
                localization,
                "reward.gold_gained",
                {{"amount", std::to_string(gold_written_back)}},
                [gold_written_back] { return fmt::format("Gained Gold {}", gold_written_back); }));
    }

    appendExperienceLines(text, experience_result, localization);

    for (const auto& item_result : item_results) {
        const std::string item_name = resolveItemName(item_result, item_catalog, localization);
        if (item_result.accepted > 0) {
            appendLine(
                text,
                game::ui::formatTextOrFallback(
                    localization,
                    "reward.item_gained",
                    {{"item", item_name}, {"count", std::to_string(item_result.accepted)}},
                    [&item_name, &item_result] { return fmt::format("Gained {} x{}", item_name, item_result.accepted); }));
        }
        if (item_result.rejected > 0) {
            appendLine(
                text,
                game::ui::formatTextOrFallback(
                    localization,
                    "reward.item_missed",
                    {{"item", item_name}, {"count", std::to_string(item_result.rejected)}},
                    [&item_name, &item_result] { return fmt::format("Inventory full, missed {} x{}", item_name, item_result.rejected); }));
        }
    }

    if (text.empty()) {
        return game::ui::localizeTextOrFallback(localization, "reward.victory", "Victory");
    }
    return text;
}

std::string formatBattleSettlementFeedback(const BattleRewardWritebackResult& reward_result,
                                           const game::domain::PartyExperienceGrantResult* experience_result,
                                           const game::domain::QuestBattleProgressSummary& quest_progress_summary,
                                           const game::data::ItemCatalog* item_catalog,
                                           const game::runtime::LocalizationService* localization) {
    if (reward_result.empty() && (!experience_result || experience_result->empty()) && quest_progress_summary.empty()) {
        return formatRewardFeedback(0, {}, item_catalog, nullptr, localization);
    }

    std::string text{};
    if (!reward_result.empty() || (experience_result && !experience_result->empty())) {
        text = formatRewardFeedback(
            reward_result.gold_written_back,
            reward_result.item_results,
            item_catalog,
            experience_result,
            localization);
    }

    appendQuestProgressLines(text, quest_progress_summary, localization);
    return text.empty() ? game::ui::localizeTextOrFallback(localization, "reward.victory", "Victory") : text;
}

std::string formatBattleSettlementFeedback(const BattleRewardWritebackResult& reward_result,
                                           const game::domain::QuestBattleProgressSummary& quest_progress_summary,
                                           const game::data::ItemCatalog* item_catalog,
                                           const game::runtime::LocalizationService* localization) {
    return formatBattleSettlementFeedback(reward_result, nullptr, quest_progress_summary, item_catalog, localization);
}

} // namespace game::scene
