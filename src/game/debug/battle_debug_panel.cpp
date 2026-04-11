#include "battle_debug_panel.h"

#include "game/component/inventory_component.h"
#include "game/component/tags.h"
#include "game/data/rpg_catalog.h"
#include "game/data/rpg_data.h"
#include "game/defs/commands.h"

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <imgui.h>

#include <algorithm>
#include <string>
#include <vector>

namespace {

[[nodiscard]] entt::entity findPlayer(entt::registry& registry) {
    auto view = registry.view<game::component::PlayerTag>();
    if (view.begin() == view.end()) {
        return entt::null;
    }
    return *view.begin();
}

[[nodiscard]] bool hasBattlePotion(entt::registry& registry, entt::entity player_entity, entt::id_type potion_id) {
    if (!registry.valid(player_entity)) {
        return false;
    }

    const auto* inventory = registry.try_get<game::component::InventoryComponent>(player_entity);
    if (inventory == nullptr) {
        return false;
    }

    return std::any_of(inventory->slots_.begin(), inventory->slots_.end(), [potion_id](const auto& slot) {
        return slot.item_id_ == potion_id && slot.count_ > 0;
    });
}

[[nodiscard]] bool ensureDebugBattlePotion(entt::registry& registry, entt::dispatcher& dispatcher, entt::entity player_entity) {
    if (!registry.valid(player_entity)) {
        return false;
    }

    const entt::id_type potion_id = entt::hashed_string{"potion"}.value();
    if (hasBattlePotion(registry, player_entity, potion_id)) {
        return true;
    }

    dispatcher.trigger(game::defs::AddItemCommand{
        .target = player_entity,
        .item_id = potion_id,
        .count = 3});

    return hasBattlePotion(registry, player_entity, potion_id);
}

[[nodiscard]] std::vector<const game::data::TroopData*> collectTroops(const game::data::RpgCatalog& rpg_catalog) {
    std::vector<const game::data::TroopData*> troops = rpg_catalog.listTroops();
    troops.erase(
        std::remove_if(troops.begin(), troops.end(), [](const auto* troop) {
            return troop->members_.empty();
        }),
        troops.end());
    return troops;
}

void syncSelectedTroop(std::string& selected_troop_id, const std::vector<const game::data::TroopData*>& troops) {
    const auto found_it = std::find_if(troops.begin(), troops.end(), [&selected_troop_id](const auto* troop) {
        return troop->id_ == selected_troop_id;
    });
    if (found_it != troops.end()) {
        return;
    }

    if (!troops.empty()) {
        selected_troop_id = troops.front()->id_;
        return;
    }

    selected_troop_id.clear();
}

[[nodiscard]] std::string troopLabel(const game::data::TroopData& troop) {
    const std::string display_name = troop.display_name_.empty() ? troop.id_ : troop.display_name_;
    return display_name + "##" + troop.id_;
}

[[nodiscard]] std::string enemyLabel(const game::data::RpgCatalog& rpg_catalog,
                                     const game::data::TroopMemberData& member) {
    if (const auto* enemy = rpg_catalog.findEnemy(member.enemy_id_)) {
        if (!enemy->display_name_.empty()) {
            return enemy->display_name_;
        }
    }
    return member.enemy_id_;
}

} // namespace

namespace game::debug {

BattleDebugPanel::BattleDebugPanel(entt::registry& registry,
                                   entt::dispatcher& dispatcher,
                                   const game::data::RpgCatalog* rpg_catalog)
    : registry_(registry),
      dispatcher_(dispatcher),
      rpg_catalog_(rpg_catalog) {
}

std::string_view BattleDebugPanel::name() const {
    return "Battle";
}

void BattleDebugPanel::draw(bool& is_open) {
    if (!is_open) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(560.0f, 420.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Battle Debug", &is_open)) {
        ImGui::End();
        return;
    }

    const entt::entity player = findPlayer(registry_);
    if (player == entt::null) {
        ImGui::Text("未找到玩家实体");
        ImGui::End();
        return;
    }

    if (rpg_catalog_ == nullptr) {
        ImGui::Text("RpgCatalog 不可用");
        ImGui::End();
        return;
    }

    const std::vector<const game::data::TroopData*> troops = collectTroops(*rpg_catalog_);
    if (troops.empty()) {
        ImGui::Text("RpgCatalog 中没有可用 troop");
        ImGui::End();
        return;
    }

    syncSelectedTroop(selected_troop_id_, troops);
    auto* selected_troop = rpg_catalog_->findTroop(selected_troop_id_);
    if (selected_troop == nullptr) {
        ImGui::Text("未找到当前 troop 定义");
        ImGui::End();
        return;
    }

    ImGui::TextWrapped("使用当前 RPG catalog 的默认 actor 队伍，并允许手动指定 enemy troop。");

    const std::string current_label =
        selected_troop->display_name_.empty() ? selected_troop->id_ : selected_troop->display_name_;
    if (ImGui::BeginCombo("Troop", current_label.c_str())) {
        for (const auto* troop : troops) {
            const bool is_selected = (selected_troop_id_ == troop->id_);
            const std::string label = troopLabel(*troop);
            if (ImGui::Selectable(label.c_str(), is_selected)) {
                selected_troop_id_ = troop->id_;
            }
            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    selected_troop = rpg_catalog_->findTroop(selected_troop_id_);
    if (selected_troop == nullptr) {
        ImGui::Text("切换后未找到当前 troop 定义");
        ImGui::End();
        return;
    }

    ImGui::TextDisabled("%s", selected_troop->id_.c_str());
    ImGui::Separator();
    ImGui::Text("Members: %d", static_cast<int>(selected_troop->members_.size()));
    for (const auto& member : selected_troop->members_) {
        const std::string enemy_name = enemyLabel(*rpg_catalog_, member);
        ImGui::BulletText("%s (%s) @ %.0f, %.0f",
                          enemy_name.c_str(),
                          member.enemy_id_.c_str(),
                          member.x_,
                          member.y_);
    }

    ImGui::Separator();
    if (ImGui::Button("Start Battle")) {
        const bool has_potion = ensureDebugBattlePotion(registry_, dispatcher_, player);
        dispatcher_.trigger(game::defs::EnterBattleCommand{
            .troop_id = selected_troop_id_});

        status_ = "已发起战斗: " +
                  (selected_troop->display_name_.empty() ? selected_troop->id_ : selected_troop->display_name_);
        if (!has_potion) {
            status_ += " | 背包已满，未补测试药水";
        }
    }

    if (!status_.empty()) {
        ImGui::Separator();
        ImGui::TextWrapped("%s", status_.c_str());
    }

    ImGui::End();
}

} // namespace game::debug
