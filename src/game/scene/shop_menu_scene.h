#pragma once

#include "engine/scene/scene.h"
#include "engine/ui/rmlui/rml_document_controller.h"

#include <RmlUi/Core/Types.h>
#include <entt/entity/entity.hpp>

#include <string>
#include <string_view>

namespace engine::core {
enum class State;
}

namespace game::data {
class ItemCatalog;
class ShopCatalog;
}

namespace game::domain {
class ShopTransactionService;
}

namespace game::scene {

class ShopMenuScene final : public engine::scene::Scene {
    entt::registry& game_registry_;
    entt::entity player_{entt::null};
    std::string shop_id_{};
    const game::data::ShopCatalog* shop_catalog_{nullptr};
    game::data::ItemCatalog* item_catalog_{nullptr};
    game::domain::ShopTransactionService* shop_transaction_service_{nullptr};
    engine::core::State previous_state_{};
    bool context_pushed_{false};

    engine::ui::rmlui::RmlDocumentController document_controller_{};
    Rml::String shop_title_{"Shop"};
    Rml::String shop_greeting_{"Welcome."};

public:
    ShopMenuScene(std::string_view name,
                  engine::core::Context& context,
                  entt::registry& game_registry,
                  entt::entity player,
                  std::string shop_id,
                  const game::data::ShopCatalog* shop_catalog,
                  game::data::ItemCatalog* item_catalog,
                  game::domain::ShopTransactionService* shop_transaction_service);
    ~ShopMenuScene() override;

    [[nodiscard]] bool init() override;
    void clean() override;

private:
    [[nodiscard]] bool initUI();
    void shutdownUI();
    void disconnectRuntimeListeners();
    [[nodiscard]] bool onMenuCancelPressed();
    void onClose();
};

} // namespace game::scene
