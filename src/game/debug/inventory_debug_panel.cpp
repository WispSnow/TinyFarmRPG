#include "inventory_debug_panel.h"
#include "game/component/inventory_component.h"
#include "game/component/hotbar_component.h"
#include "game/component/tags.h"
#include "game/defs/events.h"
#include "game/data/item_catalog.h"
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <string>

namespace game::debug {

InventoryDebugPanel::InventoryDebugPanel(entt::registry& registry, entt::dispatcher& dispatcher, data::ItemCatalog* catalog)
    : registry_(registry), dispatcher_(dispatcher), catalog_(catalog) {
}

std::string_view InventoryDebugPanel::name() const {
    return "Inventory";
}

void InventoryDebugPanel::draw(bool& is_open) {
    if (!is_open) return;
    if (!ImGui::Begin("Inventory Debug", &is_open, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    auto view = registry_.view<game::component::InventoryComponent, game::component::PlayerTag>();
    if (view.begin() == view.end()) {
        ImGui::Text("未找到带 InventoryComponent 的玩家实体");
        ImGui::End();
        return;
    }
    const entt::entity player = *view.begin();

    if (!catalog_) {
        ImGui::Text("ItemCatalog 不可用");
        ImGui::End();
        return;
    }

    auto items = catalog_->listItems();
    if (items.empty()) {
        ImGui::Text("物品列表为空");
        ImGui::End();
        return;
    }

    if (selected_item_id_ == entt::null) {
        selected_item_id_ = items.front()->id_;
    }

    const auto findItemIndex = [&](entt::id_type id) {
        for (std::size_t i = 0; i < items.size(); ++i) {
            if (items[i]->id_ == id) return static_cast<int>(i);
        }
        return 0;
    };

    int current_index = findItemIndex(selected_item_id_);
    std::string current_label = items[current_index]->display_name_;
    if (ImGui::BeginCombo("Item", current_label.c_str())) {
        for (int i = 0; i < static_cast<int>(items.size()); ++i) {
            bool selected = (i == current_index);
            if (ImGui::Selectable(items[static_cast<std::size_t>(i)]->display_name_.c_str(), selected)) {
                selected_item_id_ = items[static_cast<std::size_t>(i)]->id_;
                current_index = i;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::InputInt("Add Count", &add_count_);
    ImGui::InputInt("Remove Count", &remove_count_);

    if (ImGui::Button("Add Item")) {
        dispatcher_.trigger(game::defs::AddItemRequest{player, selected_item_id_, std::max(1, add_count_)});
    }

    ImGui::SameLine();
    if (ImGui::Button("Remove Item")) {
        dispatcher_.trigger(game::defs::RemoveItemRequest{player, selected_item_id_, std::max(1, remove_count_)});
    }

    ImGui::Separator();
    if (ImGui::Button("Sync Inventory")) {
        dispatcher_.trigger(game::defs::InventorySyncRequest{player});
    }
    ImGui::SameLine();
    if (ImGui::Button("Sync Hotbar")) {
        dispatcher_.trigger(game::defs::HotbarSyncRequest{player});
    }

    ImGui::End();
}

} // namespace game::debug
