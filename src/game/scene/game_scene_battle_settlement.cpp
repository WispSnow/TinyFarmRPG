#include "game/scene/game_scene_battle_settlement.h"

#include "game/component/inventory_component.h"
#include "game/component/player_wallet_component.h"
#include "game/component/quest_log_component.h"
#include "game/component/tags.h"
#include "game/data/item_catalog.h"
#include "game/domain/inventory_domain_service.h"
#include "game/domain/quest_battle_progress_resolver.h"
#include "game/runtime/system_bundle.h"
#include "game/scene/game_scene_reward_feedback.h"
#include "game/system/system_helpers.h"

#include <spdlog/spdlog.h>

#include <cassert>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace game::scene {
namespace {

constexpr std::uint8_t NOTIFICATION_CHANNEL = 1;
constexpr float NOTIFICATION_SECONDS = 2.0f;

[[nodiscard]] entt::entity findPlayerEntityWithInventory(entt::registry& registry) {
    auto players = registry.view<game::component::PlayerTag, game::component::InventoryComponent>();
    if (players.begin() == players.end()) {
        return entt::null;
    }
    return *players.begin();
}

[[nodiscard]] entt::entity findPlayerEntity(entt::registry& registry) {
    auto players = registry.view<game::component::PlayerTag>();
    if (players.begin() == players.end()) {
        return entt::null;
    }
    return *players.begin();
}

void applyBattleItemStockDelta(entt::registry& registry,
                               game::runtime::GameRuntimeServices* services,
                               std::unordered_map<entt::id_type, int>& active_battle_initial_item_stocks,
                               bool& has_active_battle_item_stocks,
                               const std::unordered_map<entt::id_type, int>& remaining_item_stocks) {
    if (!has_active_battle_item_stocks) {
        return;
    }

    if (!services || !services->inventory_domain_service) {
        spdlog::warn("GameScene: InventoryDomainService 不可用，跳过战斗物品库存写回。");
        active_battle_initial_item_stocks.clear();
        has_active_battle_item_stocks = false;
        return;
    }

    const entt::entity player = findPlayerEntityWithInventory(registry);
    if (player == entt::null) {
        spdlog::warn("GameScene: 找不到带 InventoryComponent 的玩家，跳过战斗物品库存写回。");
        active_battle_initial_item_stocks.clear();
        has_active_battle_item_stocks = false;
        return;
    }

    std::unordered_map<entt::id_type, int> deltas{};
    for (const auto& [item_id, count] : remaining_item_stocks) {
        if (item_id != entt::null && count != 0) {
            deltas[item_id] += count;
        }
    }
    for (const auto& [item_id, count] : active_battle_initial_item_stocks) {
        if (item_id != entt::null && count != 0) {
            deltas[item_id] -= count;
        }
    }

    std::vector<std::pair<entt::id_type, int>> removals{};
    std::vector<std::pair<entt::id_type, int>> additions{};
    for (const auto& [item_id, delta] : deltas) {
        if (delta < 0) {
            removals.emplace_back(item_id, -delta);
        } else if (delta > 0) {
            additions.emplace_back(item_id, delta);
        }
    }

    for (const auto& [item_id, count] : removals) {
        const auto result = services->inventory_domain_service->removeItem(player, item_id, count);
        if (result.accepted != count) {
            spdlog::warn("GameScene: 战斗物品扣除不完整 item_id={}, expected={}, accepted={}.",
                         item_id,
                         count,
                         result.accepted);
        }
    }
    for (const auto& [item_id, count] : additions) {
        const auto result = services->inventory_domain_service->addItem(player, item_id, count);
        if (result.accepted != count) {
            spdlog::warn("GameScene: 战斗物品写回不完整 item_id={}, expected={}, accepted={}, rejected={}.",
                         item_id,
                         count,
                         result.accepted,
                         result.rejected);
        }
    }

    active_battle_initial_item_stocks.clear();
    has_active_battle_item_stocks = false;
}

[[nodiscard]] game::scene::BattleRewardWritebackResult applyVictoryRewards(
    entt::registry& registry,
    game::runtime::GameRuntimeServices* services,
    const game::defs::BattleEndedEvent& evt) {
    game::scene::BattleRewardWritebackResult writeback_result{};
    if (evt.outcome != game::battle::BattleOutcome::Victory) {
        return writeback_result;
    }

    if (!services) {
        spdlog::warn("GameScene: runtime services 不可用，跳过战斗奖励写回。");
        return writeback_result;
    }

    assert(evt.reward_summary.has_value() && "Victory BattleEndedEvent must carry reward_summary");
    if (!evt.reward_summary.has_value()) {
        spdlog::warn("GameScene: Victory BattleEndedEvent 缺少 reward_summary，跳过战斗奖励写回。");
        return writeback_result;
    }

    const entt::entity player = findPlayerEntity(registry);
    if (player == entt::null) {
        spdlog::warn("GameScene: 找不到玩家实体，跳过战斗奖励写回。");
        return writeback_result;
    }

    const game::battle::BattleRewardSummary& reward_summary = *evt.reward_summary;

    if (reward_summary.gold_total > 0) {
        if (auto* wallet = registry.try_get<game::component::PlayerWalletComponent>(player)) {
            wallet->gold_ += reward_summary.gold_total;
            writeback_result.gold_written_back = reward_summary.gold_total;
        } else {
            spdlog::warn("GameScene: 玩家缺少 PlayerWalletComponent，跳过金币写回。");
        }
    }

    writeback_result.item_results.reserve(reward_summary.item_drops.size());

    if (!reward_summary.item_drops.empty()) {
        if (!services->inventory_domain_service) {
            spdlog::warn("GameScene: InventoryDomainService 不可用，跳过掉落入包。");
        } else {
            for (const auto& drop : reward_summary.item_drops) {
                const auto add_result = services->inventory_domain_service->addItem(player, drop.item_id_hash, drop.count);
                if (add_result.accepted != drop.count) {
                    spdlog::warn("GameScene: 战斗掉落入包不完整 item_id={}, expected={}, accepted={}, rejected={}.",
                                 drop.item_id,
                                 drop.count,
                                 add_result.accepted,
                                 add_result.rejected);
                }
                writeback_result.item_results.push_back(game::scene::BattleRewardWritebackItemResult{
                    .drop = drop,
                    .accepted = add_result.accepted,
                    .rejected = add_result.rejected});
            }
        }
    }

    return writeback_result;
}

[[nodiscard]] game::domain::QuestBattleProgressSummary applyQuestBattleProgress(
    entt::registry& registry,
    game::runtime::GameRuntimeServices* services,
    const game::defs::BattleEndedEvent& evt) {
    game::domain::QuestBattleProgressSummary result{};
    if (evt.outcome != game::battle::BattleOutcome::Victory) {
        return result;
    }

    if (!services || !services->quest_catalog) {
        spdlog::warn("GameScene: QuestCatalog 不可用，跳过战斗任务推进。");
        return result;
    }

    const entt::entity player = findPlayerEntity(registry);
    if (player == entt::null) {
        spdlog::warn("GameScene: 找不到玩家实体，跳过战斗任务推进。");
        return result;
    }

    auto* quest_log = registry.try_get<game::component::QuestLogComponent>(player);
    if (!quest_log) {
        spdlog::warn("GameScene: 玩家缺少 QuestLogComponent，跳过战斗任务推进。");
        return result;
    }

    game::domain::QuestBattleProgressResolver resolver{};
    return resolver.apply(evt.outcome, evt.final_units, *services->quest_catalog, *quest_log);
}

} // namespace

void processBattleEndedForGameScene(
    entt::registry& registry,
    entt::dispatcher& dispatcher,
    game::runtime::GameRuntimeServices* services,
    std::unordered_map<entt::id_type, int>& active_battle_initial_item_stocks,
    bool& has_active_battle_item_stocks,
    game::system::helpers::NotificationTimer& reward_notification,
    const game::defs::BattleEndedEvent& evt) {
    applyBattleItemStockDelta(
        registry,
        services,
        active_battle_initial_item_stocks,
        has_active_battle_item_stocks,
        evt.remaining_item_stocks);

    if (evt.outcome != game::battle::BattleOutcome::Victory) {
        return;
    }

    const game::scene::BattleRewardWritebackResult reward_result = applyVictoryRewards(registry, services, evt);
    const game::domain::QuestBattleProgressSummary quest_result = applyQuestBattleProgress(registry, services, evt);

    const entt::entity player = findPlayerEntity(registry);
    if (player == entt::null) {
        return;
    }

    const game::data::ItemCatalog* item_catalog = services && services->item_catalog ? services->item_catalog.get() : nullptr;
    const std::string feedback = game::scene::formatBattleSettlementFeedback(reward_result, quest_result, item_catalog);
    game::system::helpers::showTimedNotification(
        registry,
        dispatcher,
        NOTIFICATION_CHANNEL,
        reward_notification,
        player,
        std::string{},
        feedback,
        NOTIFICATION_SECONDS);
}

} // namespace game::scene
