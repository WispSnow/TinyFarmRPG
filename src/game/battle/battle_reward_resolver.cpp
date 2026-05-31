#include "game/battle/battle_reward_resolver.h"

#include "game/data/rpg_catalog.h"
#include "game/data/rpg_data.h"

#include <algorithm>
#include <limits>
#include <random>
#include <string_view>
#include <utility>

namespace game::battle {
namespace {

BattleRewardItemDrop* findItemDrop(BattleRewardSummary& summary, const std::string_view item_id) {
    const auto it = std::find_if(summary.item_drops.begin(), summary.item_drops.end(), [item_id](const BattleRewardItemDrop& drop) {
        return drop.item_id == item_id;
    });
    return it != summary.item_drops.end() ? &(*it) : nullptr;
}

void appendItemDrop(BattleRewardSummary& summary, const std::string_view item_id) {
    if (item_id.empty()) {
        return;
    }

    if (auto* existing = findItemDrop(summary, item_id)) {
        existing->count += 1;
        return;
    }

    summary.item_drops.push_back(BattleRewardItemDrop{
        .item_id = std::string{item_id},
        .item_id_hash = game::data::RpgCatalog::hashId(item_id),
        .count = 1});
}

[[nodiscard]] int checkedRewardSum(const int current, const int reward) {
    if (reward <= 0) {
        return current;
    }
    if (current > std::numeric_limits<int>::max() - reward) {
        return std::numeric_limits<int>::max();
    }
    return current + reward;
}

} // namespace

BattleRewardResolver::BattleRewardResolver(DropRollFn drop_roll_override)
    : drop_roll_override_(std::move(drop_roll_override)) {}

float BattleRewardResolver::nextDropRoll() {
    if (drop_roll_override_) {
        return drop_roll_override_();
    }

    std::uniform_real_distribution<float> distribution(0.0F, 1.0F);
    return distribution(random_engine_);
}

BattleRewardSummary BattleRewardResolver::resolve(const BattleOutcome outcome,
                                                  const std::vector<BattleUnit>& final_units,
                                                  const game::data::RpgCatalog& rpg_catalog) {
    BattleRewardSummary summary{};
    if (outcome != BattleOutcome::Victory) {
        return summary;
    }

    for (const auto& unit : final_units) {
        if (unit.side != BattleSide::Enemy || unit.isAlive() || !unit.source_enemy_id.has_value()) {
            continue;
        }

        const auto* enemy = rpg_catalog.findEnemy(*unit.source_enemy_id);
        if (!enemy) {
            continue;
        }

        summary.gold_total = checkedRewardSum(summary.gold_total, enemy->gold_reward_);
        summary.exp_total = checkedRewardSum(summary.exp_total, enemy->exp_reward_);

        for (const auto& drop : enemy->drops_) {
            if (nextDropRoll() >= drop.chance_) {
                continue;
            }
            appendItemDrop(summary, drop.item_id_);
        }
    }

    return summary;
}

} // namespace game::battle
