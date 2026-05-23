#pragma once

#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace game::data {
class RpgCatalog;
}

namespace game::scene {

struct PartyMemberPanelViewModel {
    int slot_index{0};
    std::string actor_id{};
    std::string display_name{};
    std::string class_label{};
    std::string level_label{};
    std::string exp_label{};
    std::string hp_text{};
    std::string mp_text{};
    std::string portrait_decorator{"none"};
    std::string portrait_src{};
    bool selected{false};
    bool empty{true};
    bool targetable{false};
    bool has_portrait{false};
};

struct InventoryMenuPartyPanelData {
    std::vector<PartyMemberPanelViewModel> party_members;
    std::string gold_label;
    std::string farm_label;
};

[[nodiscard]] InventoryMenuPartyPanelData buildInventoryMenuPartyPanelData(
    const entt::registry& registry,
    entt::entity player,
    const game::data::RpgCatalog* rpg_catalog,
    std::string_view selected_actor_id,
    bool target_mode);

} // namespace game::scene
