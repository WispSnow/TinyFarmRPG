#include "game/scene/inventory_menu_character_panel.h"

#include "engine/component/name_component.h"
#include "game/component/player_wallet_component.h"

#include <entt/entity/registry.hpp>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

namespace game::scene {

InventoryMenuCharacterPanelData buildInventoryMenuCharacterPanelData(
    const entt::registry& registry,
    entt::entity player) {
    InventoryMenuCharacterPanelData data{
        .char_name = "Player",
        .char_title = "Lv.1 Farmer",
        .gold_label = "Gold: 0",
        .farm_label = "TinyFarm",
    };

    if (const auto* name = registry.try_get<engine::component::NameComponent>(player);
        name && !name->name_.empty()) {
        data.char_name = name->name_;
    }

    if (const auto* wallet = registry.try_get<game::component::PlayerWalletComponent>(player)) {
        data.gold_label = fmt::format("Gold: {}", wallet->gold_);
    } else {
        spdlog::warn("InventoryMenuScene: 玩家缺少 PlayerWalletComponent，金币显示回退为 0。");
    }

    return data;
}

} // namespace game::scene
