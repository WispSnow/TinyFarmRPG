#pragma once

#include "game/battle/battle_log_formatter.h"
#include "game/battle/battle_session.h"
#include "game/scene/battle_scene_view_models.h"
#include "game/scene/battle_victory_flow_controller.h"

#include <cstddef>
#include <vector>

namespace game::data {
class ItemCatalog;
class RpgCatalog;
} // namespace game::data

namespace game::factory {
class BlueprintManager;
} // namespace game::factory

namespace game::scene {

struct BattlePartyHudViewModels {
    std::vector<BattlePartyStatusViewModel> party_status{};
    std::vector<BattleStateIconViewModel> party_state_icons{};
};

/// @brief 构造 BattleScene 使用的只读 RmlUi ViewModel。
class BattleViewModelBuilder final {
public:
    BattleViewModelBuilder(const game::data::RpgCatalog* rpg_catalog,
                           const game::data::ItemCatalog* item_catalog,
                           const game::factory::BlueprintManager* blueprint_manager);

    [[nodiscard]] std::vector<BattleTurnOrderEntryViewModel> buildTurnOrderEntries(
        const game::battle::BattleSession& session) const;
    [[nodiscard]] BattlePartyHudViewModels buildPartyHud(const game::battle::BattleSession& session) const;
    [[nodiscard]] std::vector<BattleLogEntryViewModel> buildBattleLogEntries(
        const std::vector<game::battle::BattleLogLine>& log_history,
        std::size_t visible_limit) const;
    [[nodiscard]] std::vector<BattleVictoryRewardItemViewModel> buildVictoryRewardItems(
        const BattleVictoryFlowSnapshot& snapshot) const;
    [[nodiscard]] std::vector<BattleVictoryLevelUpViewModel> buildVictoryLevelUps(
        const BattleVictoryFlowSnapshot& snapshot) const;

private:
    const game::data::RpgCatalog* rpg_catalog_{nullptr};
    const game::data::ItemCatalog* item_catalog_{nullptr};
    const game::factory::BlueprintManager* blueprint_manager_{nullptr};
};

} // namespace game::scene
