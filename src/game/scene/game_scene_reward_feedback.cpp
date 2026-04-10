#include "game/scene/game_scene_reward_feedback.h"

#include "game/data/item_catalog.h"

#include <spdlog/fmt/fmt.h>

#include <string>
#include <string_view>

namespace game::scene {
namespace {

constexpr std::string_view kVictoryText = "战斗胜利";

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

} // namespace

std::string formatRewardFeedback(const int gold_written_back,
                                 const std::vector<BattleRewardWritebackItemResult>& item_results,
                                 const game::data::ItemCatalog* item_catalog) {
    std::string text{};

    if (gold_written_back > 0) {
        appendLine(text, fmt::format("获得金币 {}", gold_written_back));
    }

    for (const auto& item_result : item_results) {
        const std::string item_name = resolveItemName(item_result, item_catalog);
        if (item_result.accepted > 0) {
            appendLine(text, fmt::format("获得 {} x{}", item_name, item_result.accepted));
        }
        if (item_result.rejected > 0) {
            appendLine(text, fmt::format("背包已满，未获得 {} x{}", item_name, item_result.rejected));
        }
    }

    if (text.empty()) {
        return std::string{kVictoryText};
    }
    return text;
}

} // namespace game::scene
