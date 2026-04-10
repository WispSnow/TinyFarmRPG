#pragma once

#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>

#include <string>

namespace game::scene {

struct InventoryMenuCharacterPanelData {
    std::string char_name;
    std::string char_title;
    std::string gold_label;
    std::string farm_label;
};

[[nodiscard]] InventoryMenuCharacterPanelData buildInventoryMenuCharacterPanelData(
    const entt::registry& registry,
    entt::entity player);

} // namespace game::scene
