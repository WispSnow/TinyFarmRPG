#pragma once

#include "engine/scene/scene.h"

#include <entt/entity/fwd.hpp>

#include <memory>
#include <string_view>

namespace engine::core {
enum class State;
}

namespace game::data {
class ItemCatalog;
}

namespace game::ui {
class InventoryMenuUI;
class ItemTooltipUI;
}

namespace game::scene {

class InventoryMenuScene final : public engine::scene::Scene {
private:
    entt::entity target_{entt::null};
    game::data::ItemCatalog* item_catalog_{nullptr};
    engine::core::State previous_state_{};
    bool context_pushed_{false};

    std::unique_ptr<game::ui::InventoryMenuUI> menu_ui_{};
    std::unique_ptr<game::ui::ItemTooltipUI> tooltip_ui_{};

public:
    InventoryMenuScene(std::string_view name,
                       engine::core::Context& context,
                       entt::entity target,
                       game::data::ItemCatalog* item_catalog);
    ~InventoryMenuScene() override;

    bool init() override;
    void update(float delta_time) override;
    void clean() override;

private:
    [[nodiscard]] bool initUI();
    void disconnectRuntimeListeners();
    bool onMenuCancelPressed();
};

} // namespace game::scene
