#pragma once

#include "game/component/quest_log_component.h"

#include <cstdint>
#include <string>
#include <vector>

#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>

namespace game::data {
class ItemCatalog;
struct QuestData;
}

namespace game::domain {

class InventoryDomainService;

enum class QuestTurnInStatus : std::uint8_t {
    Completed = 0,
    NotReady,
    InventoryFull,
    MissingWallet,
    MissingInventory
};

struct QuestTurnInItemReward {
    std::string item_id{};
    std::string item_name{};
    int count{0};
};

struct QuestTurnInResult {
    QuestTurnInStatus status{QuestTurnInStatus::NotReady};
    int gold_reward{0};
    std::vector<QuestTurnInItemReward> item_rewards{};
    std::string failure_message{};

    [[nodiscard]] bool completed() const {
        return status == QuestTurnInStatus::Completed;
    }
};

class QuestTurnInService final {
public:
    QuestTurnInService(entt::registry& registry,
                       game::data::ItemCatalog& item_catalog,
                       game::domain::InventoryDomainService& inventory_domain_service);

    [[nodiscard]] QuestTurnInResult turnIn(entt::entity player,
                                           const game::data::QuestData& quest,
                                           game::component::QuestLogComponent& quest_log) const;

private:
    entt::registry& registry_;
    game::data::ItemCatalog& item_catalog_;
    game::domain::InventoryDomainService& inventory_domain_service_;
};

} // namespace game::domain
