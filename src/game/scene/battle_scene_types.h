#pragma once

#include "game/battle/battle_types.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace game::data {
class AppearanceCatalog;
}

namespace game::factory {
class BlueprintManager;
}

namespace game::scene {

struct AppearanceSnapshot {
    std::string profile_id{};
    std::string gender{"male"};
    std::unordered_map<std::string, std::string> slot_variants{};
    bool valid{false};
};

struct BattleSpriteSeed {
    game::battle::BattleUnitId unit_id{0};
    std::optional<std::string> source_actor_id{};
    std::optional<std::string> source_enemy_id{};
    std::optional<AppearanceSnapshot> appearance{};
};

struct BattleScenePresentationOptions {
    std::vector<BattleSpriteSeed> sprite_seeds{};
    const game::factory::BlueprintManager* blueprint_manager{nullptr};
    const game::data::AppearanceCatalog* appearance_catalog{nullptr};
    std::string battle_background_id{};
};

} // namespace game::scene
