#pragma once

#include "engine/ui/rmlui/rml_document_controller.h"
#include "game/data/rpg_types.h"
#include "game/ui/menu_tab_content.h"

#include <RmlUi/Core/Types.h>
#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace Rml {
class DataModelConstructor;
class Event;
}

namespace engine::core {
class Context;
}

namespace game::data {
struct ItemData;
class ItemCatalog;
class RpgCatalog;
}

namespace game::defs {
struct EquipmentChanged;
struct InventoryChanged;
}

namespace game::runtime {
class LocalizationService;
}

namespace game::ui {

struct EquipmentSlotViewModel {
    int slot_index{0};
    Rml::String label{};
    Rml::String item_name{};
    Rml::String placeholder_decorator{"none"};
    Rml::String icon_decorator{"none"};
    bool has_item{false};
    bool is_selected{false};
};

struct EquipmentCandidateViewModel {
    int inventory_slot_index{-1};
    Rml::String item_name{};
    Rml::String category_label{};
    Rml::String icon_decorator{"none"};
    Rml::String delta_text{};
    bool has_delta{false};
};

[[nodiscard]] bool registerEquipmentTabDataTypes(Rml::DataModelConstructor& constructor);

class EquipmentTabContent final : public IMenuTabContent {
public:
    EquipmentTabContent(engine::core::Context& context,
                        engine::ui::rmlui::RmlDocumentController& document_controller,
                        entt::registry& game_registry,
                        entt::entity player,
                        game::data::ItemCatalog* item_catalog,
                        const game::data::RpgCatalog* rpg_catalog,
                        std::string_view selected_actor_id);
    ~EquipmentTabContent() override;

    [[nodiscard]] bool bindModel(Rml::DataModelConstructor& constructor) override;
    void onActivated() override;
    void onDeactivated() override;
    void update(float delta_time) override;
    [[nodiscard]] bool onCancel() override;
    void onLanguageChanged() override;

    void setSelectedActor(std::string_view actor_id);

private:
    engine::core::Context& context_;
    engine::ui::rmlui::RmlDocumentController& document_controller_;
    entt::registry& game_registry_;
    entt::entity player_{entt::null};
    game::data::ItemCatalog* item_catalog_{nullptr};
    const game::data::RpgCatalog* rpg_catalog_{nullptr};
    const game::runtime::LocalizationService* localization_{nullptr};
    std::string selected_actor_id_{};

    std::vector<EquipmentSlotViewModel> equipment_slots_{};
    std::vector<EquipmentCandidateViewModel> equipment_candidates_{};
    Rml::String equipment_actor_name_{};
    Rml::String equipment_actor_summary_{};
    Rml::String equipment_detail_name_{};
    Rml::String equipment_detail_category_{};
    Rml::String equipment_detail_description_{};
    bool has_equipment_candidates_{false};
    bool has_equipment_detail_{false};
    bool listeners_connected_{false};
    game::data::EquipmentSlotId selected_slot_{game::data::EquipmentSlotId::Weapon};

    void connectRuntimeListeners();
    void disconnectRuntimeListeners();
    void syncViewState();
    void syncActorHeader();
    void syncSlots();
    void syncCandidates();
    void syncDetail();
    void markEquipmentDirty();
    [[nodiscard]] std::string localizedSlotLabel(game::data::EquipmentSlotId slot) const;
    [[nodiscard]] std::string localizedItemName(const game::data::ItemData& item) const;
    [[nodiscard]] std::string localizedItemCategory(const game::data::ItemData& item) const;
    [[nodiscard]] std::string localizedItemDescription(const game::data::ItemData& item) const;

    [[nodiscard]] entt::id_type equippedItemForSelectedSlot() const;
    void selectSlotByIndex(int slot_index);
    void equipCandidateFromInventorySlot(int inventory_slot_index);
    void unequipSelectedSlot();

    void onEquipmentSlotClick(int slot_index, Rml::Event& event);
    void onEquipmentCandidateClick(int inventory_slot_index, Rml::Event& event);
    void onUnequipClicked();

    void onInventoryChanged(const game::defs::InventoryChanged& evt);
    void onEquipmentChanged(const game::defs::EquipmentChanged& evt);
};

} // namespace game::ui
