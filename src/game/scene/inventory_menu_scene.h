#pragma once

#include "engine/scene/scene.h"
#include "engine/ui/rmlui/rml_document_controller.h"
#include "game/scene/inventory_menu_character_panel.h"
#include "game/ui/menu_tab_content.h"

#include <RmlUi/Core/DataTypeRegister.h>
#include <RmlUi/Core/Types.h>
#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace engine::core {
enum class State;
}

namespace game::data {
class ItemCatalog;
class QuestCatalog;
class RpgCatalog;
class ShopCatalog;
}

namespace game::world {
class WorldState;
}

namespace game::ui {
class EquipmentTabContent;
}

namespace game::runtime {
class UserSettingsService;
}

namespace game::scene {

struct MenuTabIdHash {
    [[nodiscard]] std::size_t operator()(game::ui::MenuTabId id) const noexcept {
        return static_cast<std::size_t>(id);
    }
};

class InventoryMenuScene final : public engine::scene::Scene {
    entt::registry& game_registry_;
    entt::entity player_{entt::null};
    game::data::ItemCatalog* item_catalog_{nullptr};
    const game::data::RpgCatalog* rpg_catalog_{nullptr};
    const game::data::QuestCatalog* quest_catalog_{nullptr};
    const game::data::ShopCatalog* shop_catalog_{nullptr};
    const game::world::WorldState* world_state_{nullptr};
    game::runtime::UserSettingsService* user_settings_service_{nullptr};
    engine::core::State previous_state_{};
    bool context_pushed_{false};

    engine::ui::rmlui::RmlDocumentController document_controller_{};
    Rml::DataTypeRegister type_register_{};
    bool data_types_registered_{false};

    std::unordered_map<game::ui::MenuTabId, std::unique_ptr<game::ui::IMenuTabContent>, MenuTabIdHash> tabs_{};
    game::ui::MenuTabId active_tab_id_{game::ui::MenuTabId::Inventory};
    game::ui::EquipmentTabContent* equipment_tab_{nullptr};

    // Party panel
    std::vector<PartyMemberPanelViewModel> party_members_{};
    std::string selected_actor_id_{};
    bool actor_target_mode_{false};
    int pending_actor_target_inventory_slot_{-1};
    Rml::String gold_label_{"Gold: 0"};
    Rml::String farm_label_{"TinyFarm"};

public:
    InventoryMenuScene(std::string_view name,
                       engine::core::Context& context,
                       entt::registry& game_registry,
                       entt::entity player,
                       game::data::ItemCatalog* item_catalog,
                       const game::data::RpgCatalog* rpg_catalog,
                       const game::data::QuestCatalog* quest_catalog,
                       const game::data::ShopCatalog* shop_catalog,
                       const game::world::WorldState* world_state,
                       game::runtime::UserSettingsService* user_settings_service);
    ~InventoryMenuScene() override;

    bool init() override;
    void update(float delta_time) override;
    void clean() override;

private:
    [[nodiscard]] bool initUI();
    void shutdownUI();
    void disconnectRuntimeListeners();
    void syncPartyPanel();
    void beginActorTargetSelection(int inventory_slot_index);
    void cancelActorTargetSelection();
    void onPartyMemberClick(int party_slot_index);
    void switchTab(game::ui::MenuTabId new_tab);
    void switchTabFromTabsetIndex(int tab_index);
    [[nodiscard]] game::ui::IMenuTabContent* activeTab();

    bool onMenuCancelPressed();
};

} // namespace game::scene
