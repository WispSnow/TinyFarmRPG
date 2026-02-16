#pragma once

#include "engine/debug/debug_panel.h"
#include <entt/entity/entity.hpp>
#include <entt/signal/fwd.hpp>
#include <entt/core/hashed_string.hpp>

namespace game::data {
    class ItemCatalog;
    struct ItemData;
}

namespace game::debug {

class InventoryDebugPanel final : public engine::debug::DebugPanel {
    entt::registry& registry_;
    entt::dispatcher& dispatcher_;
    data::ItemCatalog* catalog_{nullptr};

    int add_count_{1};
    int remove_count_{1};
    entt::id_type selected_item_id_{entt::null};

public:
    InventoryDebugPanel(entt::registry& registry, entt::dispatcher& dispatcher, data::ItemCatalog* catalog);

    std::string_view name() const override;
    void draw(bool& is_open) override;
};

} // namespace game::debug
