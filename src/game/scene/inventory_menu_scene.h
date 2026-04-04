#pragma once

#include "engine/scene/scene.h"
#include "engine/ui/rmlui/rml_document_controller.h"
#include "game/ui/menu_tab_content.h"

#include <RmlUi/Core/DataTypeRegister.h>
#include <RmlUi/Core/Types.h>
#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace engine::core {
enum class State;
}

namespace game::data {
class ItemCatalog;
}

namespace game::ui {
class IMenuTabContent;
}

namespace game::scene {

struct MenuTabIdHash {
    [[nodiscard]] std::size_t operator()(game::ui::MenuTabId id) const noexcept {
        return static_cast<std::size_t>(id);
    }
};
}

namespace game::scene {

class InventoryMenuScene final : public engine::scene::Scene {
    entt::registry& game_registry_;
    entt::entity player_{entt::null};
    game::data::ItemCatalog* item_catalog_{nullptr};
    engine::core::State previous_state_{};
    bool context_pushed_{false};

    engine::ui::rmlui::RmlDocumentController document_controller_{};
    Rml::DataTypeRegister type_register_{};
    bool data_types_registered_{false};

    std::unordered_map<game::ui::MenuTabId, std::unique_ptr<game::ui::IMenuTabContent>, MenuTabIdHash> tabs_{};
    game::ui::MenuTabId active_tab_id_{game::ui::MenuTabId::Inventory};
    int active_tab_id_bind_{static_cast<int>(game::ui::MenuTabId::Inventory)};

    // Character panel
    Rml::String char_name_{"Player"};
    Rml::String char_title_{"Lv.1 Farmer"};
    Rml::String gold_label_{"Gold: --"};
    Rml::String farm_label_{"TinyFarm"};

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
    void shutdownUI();
    void disconnectRuntimeListeners();
    void syncCharacterPanel();
    void switchTab(game::ui::MenuTabId new_tab);
    void switchTabByIndex(int tab_index);
    [[nodiscard]] game::ui::IMenuTabContent* activeTab();

    bool onMenuCancelPressed();
};

} // namespace game::scene
