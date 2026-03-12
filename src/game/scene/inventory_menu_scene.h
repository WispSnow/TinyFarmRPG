#pragma once

#include "engine/scene/scene.h"
#include "engine/ui/rmlui/rml_data_bridge.h"
#include "engine/ui/rmlui/rml_event_bridge.h"

#include <RmlUi/Core/DataTypeRegister.h>
#include <RmlUi/Core/Types.h>
#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace engine::core {
enum class State;
}

namespace Rml {
class ElementDocument;
}

namespace engine::ui::rmlui {
class HoverFocusSyncListener;
}

namespace game::data {
class ItemCatalog;
}

namespace game::defs {
struct InventoryChanged;
}

namespace game::scene {

class InventoryMenuScene final : public engine::scene::Scene {
    struct SlotViewModel {
        int slot_index{0};
        Rml::String icon_decorator{"none"};
        Rml::String count_text{};
        bool has_item{false};
        bool has_count{false};
    };

    entt::registry& game_registry_;
    entt::entity player_{entt::null};
    game::data::ItemCatalog* item_catalog_{nullptr};
    engine::core::State previous_state_{};
    bool context_pushed_{false};

    engine::ui::rmlui::RmlDataBridge data_bridge_{};
    engine::ui::rmlui::RmlEventBridge event_bridge_{};
    Rml::DataTypeRegister type_register_{};
    std::unique_ptr<engine::ui::rmlui::HoverFocusSyncListener> hover_focus_listener_{};
    Rml::ElementDocument* document_{nullptr};
    bool click_listener_registered_{false};
    bool hover_listener_registered_{false};

    std::vector<SlotViewModel> backpack_slots_{};
    bool data_types_registered_{false};

public:
    InventoryMenuScene(std::string_view name,
                       engine::core::Context& context,
                       entt::registry& game_registry,
                       entt::entity player,
                       game::data::ItemCatalog* item_catalog);
    ~InventoryMenuScene() override;

    bool init() override;
    void update(float delta_time) override;
    void clean() override;

private:
    [[nodiscard]] bool initUI();
    void removeEventListeners();
    void disconnectRuntimeListeners();

    void syncFromInventory();
    void refreshSlot(int slot_index);
    void markSlotsDirty();

    bool onMenuCancelPressed();
    void onCloseClicked();
    void onInventoryChanged(const game::defs::InventoryChanged& evt);
};

} // namespace game::scene
