#pragma once

#include "game/battle/battle_types.h"
#include "game/component/party_equipment_component.h"
#include "game/component/party_runtime_stats_component.h"
#include "game/scene/battle_enemy_hp_bar_controller.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace game::data {
class AppearanceCatalog;
}

namespace engine::vfx {
class VfxService;
}

namespace game::factory {
class BlueprintManager;
}

namespace game::runtime {
class UserSettingsService;
}

namespace game::ui {
class PlayerPortraitService;
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
    std::unordered_map<std::string, game::component::ActorRuntimeState> actor_runtime_states{};
    std::unordered_map<std::string, game::component::ActorEquipmentLoadout> actor_equipment{};
    std::unordered_map<std::string, std::string> actor_display_name_overrides{};
    const game::factory::BlueprintManager* blueprint_manager{nullptr};
    const game::data::AppearanceCatalog* appearance_catalog{nullptr};
    engine::vfx::VfxService* vfx_service{nullptr};
    std::string battle_background_id{};
    BattleEnemyHpBarConfig enemy_hp_bar_config{};
    game::runtime::UserSettingsService* user_settings_service{nullptr};
    const game::ui::PlayerPortraitService* player_portrait_service{nullptr};
};

} // namespace game::scene
