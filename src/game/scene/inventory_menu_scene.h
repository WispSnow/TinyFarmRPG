#pragma once

#include "engine/scene/scene.h"
#include "engine/ui/rmlui/rml_document_controller.h"
#include "engine/ui/rmlui/rml_generated_image_registry.h"
#include "game/scene/inventory_menu_character_panel.h"
#include "game/ui/menu_tab_content.h"

#include <RmlUi/Core/DataTypeRegister.h>
#include <RmlUi/Core/Types.h>
#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Rml {
class Element;
}

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

namespace game::defs {
struct LanguageChangedEvent;
}

namespace game::ui {
class EquipmentTabContent;
}

namespace game::runtime {
class LocalizationService;
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
    game::ui::MenuTabId initial_tab_id_{game::ui::MenuTabId::Inventory};
    game::ui::MenuTabId active_tab_id_{game::ui::MenuTabId::Inventory};
    game::ui::EquipmentTabContent* equipment_tab_{nullptr};
    Rml::Element* tab_shortcut_tooltip_panel_{nullptr};
    Rml::Element* tab_shortcut_tooltip_text_{nullptr};
    bool tab_shortcut_tooltip_visible_{false};
    glm::vec2 tab_shortcut_tooltip_size_{0.0F, 0.0F};
    glm::vec2 tab_shortcut_tooltip_offset_{12.0F, 16.0F};

    // Party panel
    std::vector<PartyMemberPanelViewModel> party_members_{};
    std::vector<engine::ui::rmlui::RmlGeneratedImageRegistry::Registration> party_portrait_registrations_{};
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
                       game::runtime::UserSettingsService* user_settings_service,
                       game::ui::MenuTabId initial_tab = game::ui::MenuTabId::Inventory);
    ~InventoryMenuScene() override;

    bool init() override;
    void update(float delta_time) override;
    void clean() override;

private:
    [[nodiscard]] bool initUI();
    void shutdownUI();
    void disconnectRuntimeListeners();
    void syncPartyPanel();
    void registerPartyPortraitImages(std::vector<PartyMemberPanelViewModel>& members);
    void clearPartyPortraitImages();
    void beginActorTargetSelection(int inventory_slot_index);
    void cancelActorTargetSelection();
    void onPartyMemberClick(int party_slot_index);
    void switchTab(game::ui::MenuTabId new_tab);
    void switchTabFromTabsetIndex(int tab_index);
    [[nodiscard]] bool activateRmlTab(game::ui::MenuTabId tab_id);
    [[nodiscard]] game::ui::IMenuTabContent* activeTab();
    [[nodiscard]] const game::runtime::LocalizationService* localization() const noexcept;
    void cacheTabShortcutTooltipElements();
    void showTabShortcutTooltip(int tab_index);
    void hideTabShortcutTooltip();
    void refreshTabShortcutTooltipMetrics();
    void updateTabShortcutTooltipPosition();

    bool onMenuCancelPressed();
    bool handleTabShortcut(game::ui::MenuTabId target_tab);
    bool onInventoryTabShortcut();
    bool onEquipmentTabShortcut();
    bool onQuestsTabShortcut();
    bool onMapTabShortcut();
    bool onOptionsTabShortcut();
    void onLanguageChanged(const game::defs::LanguageChangedEvent& event);
};

} // namespace game::scene
