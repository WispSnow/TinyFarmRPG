#pragma once

#include "battle_action_resolver.h"
#include "battle_runtime_types.h"
#include "battle_types.h"
#include "turn_core.h"

#include <entt/core/fwd.hpp>

#include <optional>
#include <unordered_map>
#include <vector>

namespace game::data {
class RpgCatalog;
class ItemCatalog;
} // namespace game::data

namespace game::battle {

struct BattleSessionOptions {
    const game::data::RpgCatalog* rpg_catalog{nullptr};
    const game::data::ItemCatalog* item_catalog{nullptr};
    std::unordered_map<entt::id_type, int> item_stocks{};
};

class BattleSession final {
    TurnCore turn_core_;
    BattleActionResolver resolver_{};
    BattleRuntimeState runtime_state_{};

public:
    explicit BattleSession(std::vector<BattleUnit> units, BattleSessionOptions options = {});

    [[nodiscard]] const std::vector<BattleUnit>& units() const { return turn_core_.units(); }
    [[nodiscard]] const BattleUnit* findUnit(BattleUnitId id) const { return turn_core_.findUnit(id); }
    [[nodiscard]] std::optional<BattleUnitId> currentActorId() const { return turn_core_.currentActorId(); }
    [[nodiscard]] BattleOutcome outcome() const { return turn_core_.outcome(); }
    [[nodiscard]] BattleSnapshot snapshot() const;

    [[nodiscard]] BattleActionResult submitAction(const BattleAction& action);

private:
    void fillSnapshot(BattleActionResult& result) const;
};

} // namespace game::battle
