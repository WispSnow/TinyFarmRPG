#include "game/domain/quest_turn_in_service.h"

#include "game/component/inventory_component.h"
#include "game/component/player_wallet_component.h"
#include "game/data/item_catalog.h"
#include "game/data/quest_data.h"
#include "game/domain/inventory_domain_service.h"
#include "game/domain/quest_log_ops.h"

#include <entt/entity/registry.hpp>
#include <spdlog/spdlog.h>

#include <utility>
#include <vector>

namespace game::domain {
namespace {

constexpr const char* kInventoryFullMessage = "Inventory full";
constexpr const char* kMissingWalletMessage = "Missing wallet component";
constexpr const char* kMissingInventoryMessage = "Missing inventory component";

[[nodiscard]] bool questNeedsWallet(const game::data::QuestData& quest) {
    return quest.rewards_.gold_ > 0;
}

[[nodiscard]] bool questNeedsInventory(const game::data::QuestData& quest) {
    return !quest.rewards_.items_.empty();
}

[[nodiscard]] std::string resolveRewardItemName(const game::data::QuestRewardItemData& reward_item,
                                                game::data::ItemCatalog& item_catalog) {
    if (const auto* item = item_catalog.findItem(reward_item.item_id_hash_)) {
        if (!item->display_name_.empty()) {
            return item->display_name_;
        }
    }
    return reward_item.item_id_;
}

[[nodiscard]] QuestTurnInResult makeFailureResult(const QuestTurnInStatus status, std::string message) {
    QuestTurnInResult result{};
    result.status = status;
    result.failure_message = std::move(message);
    return result;
}

} // namespace

QuestTurnInService::QuestTurnInService(entt::registry& registry,
                                       game::data::ItemCatalog& item_catalog,
                                       game::domain::InventoryDomainService& inventory_domain_service)
    : registry_(registry),
      item_catalog_(item_catalog),
      inventory_domain_service_(inventory_domain_service) {
}

QuestTurnInResult QuestTurnInService::turnIn(entt::entity player,
                                             const game::data::QuestData& quest,
                                             game::component::QuestLogComponent& quest_log) const {
    if (player == entt::null || !registry_.valid(player) || !game::domain::quest_log_ops::isQuestReadyToTurnIn(quest_log, quest)) {
        return makeFailureResult(QuestTurnInStatus::NotReady, {});
    }

    auto* wallet = registry_.try_get<game::component::PlayerWalletComponent>(player);
    if (questNeedsWallet(quest) && wallet == nullptr) {
        spdlog::warn("QuestTurnInService: 玩家缺少 PlayerWalletComponent，无法交付任务 '{}'.", quest.id_);
        return makeFailureResult(QuestTurnInStatus::MissingWallet, kMissingWalletMessage);
    }

    auto* inventory = registry_.try_get<game::component::InventoryComponent>(player);
    if (questNeedsInventory(quest) && inventory == nullptr) {
        spdlog::warn("QuestTurnInService: 玩家缺少 InventoryComponent，无法交付任务 '{}'.", quest.id_);
        return makeFailureResult(QuestTurnInStatus::MissingInventory, kMissingInventoryMessage);
    }

    game::component::QuestLogComponent next_quest_log = quest_log;
    if (!game::domain::quest_log_ops::completeQuest(next_quest_log, quest.id_)) {
        spdlog::warn("QuestTurnInService: 任务 '{}' 未能完成 active -> completed 状态迁移。", quest.id_);
        return makeFailureResult(QuestTurnInStatus::NotReady, {});
    }
    game::domain::quest_log_ops::eraseQuestProgress(next_quest_log, quest.id_);

    std::vector<game::domain::InventoryItemGrant> item_grants{};
    item_grants.reserve(quest.rewards_.items_.size());
    std::vector<QuestTurnInItemReward> item_rewards{};
    item_rewards.reserve(quest.rewards_.items_.size());
    int requested_item_count = 0;
    for (const auto& reward_item : quest.rewards_.items_) {
        item_grants.push_back(game::domain::InventoryItemGrant{
            .item_id = reward_item.item_id_hash_,
            .count = reward_item.count_});
        requested_item_count += reward_item.count_;
        item_rewards.push_back(QuestTurnInItemReward{
            .item_id = reward_item.item_id_,
            .item_name = resolveRewardItemName(reward_item, item_catalog_),
            .count = reward_item.count_});
    }

    if (!item_grants.empty()) {
        const auto mutation = inventory_domain_service_.addItemsAtomically(player, item_grants);
        if (mutation.accepted != requested_item_count || mutation.rejected != 0) {
            spdlog::warn("QuestTurnInService: 任务 '{}' 的 reward item 批量写回失败: requested={}, accepted={}, rejected={}",
                         quest.id_,
                         requested_item_count,
                         mutation.accepted,
                         mutation.rejected);
            return makeFailureResult(QuestTurnInStatus::InventoryFull, kInventoryFullMessage);
        }
    }

    QuestTurnInResult result{};
    result.status = QuestTurnInStatus::Completed;

    if (wallet != nullptr && quest.rewards_.gold_ > 0) {
        wallet->gold_ += quest.rewards_.gold_;
        result.gold_reward = quest.rewards_.gold_;
    }
    result.item_rewards = std::move(item_rewards);
    quest_log = std::move(next_quest_log);

    return result;
}

} // namespace game::domain
