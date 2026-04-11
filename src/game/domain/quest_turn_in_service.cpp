#include "game/domain/quest_turn_in_service.h"

#include "game/component/inventory_component.h"
#include "game/component/player_wallet_component.h"
#include "game/data/item_catalog.h"
#include "game/data/quest_data.h"
#include "game/domain/inventory_domain_service.h"
#include "game/domain/quest_log_ops.h"
#include "game/system/inventory_helpers.h"

#include <entt/entity/registry.hpp>
#include <spdlog/spdlog.h>

#include <utility>
#include <vector>

namespace game::domain {
namespace {

constexpr const char* kInventoryFullMessage = "背包空间不足";
constexpr const char* kMissingWalletMessage = "缺少钱包组件";
constexpr const char* kMissingInventoryMessage = "缺少背包组件";

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

    if (inventory != nullptr && questNeedsInventory(quest)) {
        std::vector<game::component::ItemStack> simulated_slots = inventory->slots_;
        for (const auto& reward_item : quest.rewards_.items_) {
            const int stack_limit = game::system::detail::stackLimitOrDefault(&item_catalog_, reward_item.item_id_hash_);
            if (!game::system::detail::simulateAdd(simulated_slots, -1, reward_item.item_id_hash_, reward_item.count_, stack_limit)) {
                return makeFailureResult(QuestTurnInStatus::InventoryFull, kInventoryFullMessage);
            }
        }
    }

    QuestTurnInResult result{};
    result.status = QuestTurnInStatus::Completed;

    if (wallet != nullptr && quest.rewards_.gold_ > 0) {
        wallet->gold_ += quest.rewards_.gold_;
        result.gold_reward = quest.rewards_.gold_;
    }

    result.item_rewards.reserve(quest.rewards_.items_.size());
    for (const auto& reward_item : quest.rewards_.items_) {
        result.item_rewards.push_back(QuestTurnInItemReward{
            .item_id = reward_item.item_id_,
            .item_name = resolveRewardItemName(reward_item, item_catalog_),
            .count = reward_item.count_});

        const auto mutation = inventory_domain_service_.addItem(player, reward_item.item_id_hash_, reward_item.count_);
        if (mutation.accepted != reward_item.count_ || mutation.rejected != 0) {
            spdlog::warn("QuestTurnInService: 任务 '{}' 的 reward item '{}' 写回结果异常: accepted={}, rejected={}",
                         quest.id_,
                         reward_item.item_id_,
                         mutation.accepted,
                         mutation.rejected);
        }
    }

    if (!game::domain::quest_log_ops::completeQuest(quest_log, quest.id_)) {
        spdlog::warn("QuestTurnInService: 任务 '{}' 未能完成 active -> completed 状态迁移。", quest.id_);
    }
    game::domain::quest_log_ops::eraseQuestProgress(quest_log, quest.id_);

    return result;
}

} // namespace game::domain
