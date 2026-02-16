#include <gtest/gtest.h>

#include "game/component/hotbar_component.h"
#include "game/component/inventory_component.h"
#include "game/data/item_catalog.h"
#include "game/defs/events.h"
#include "game/system/hotbar_system.h"
#include "game/system/inventory_system.h"

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

namespace game::system {

TEST(InventoryHotbarConsistencyTest, MergeIntoReferencedSlot_KeepsTargetHotkeyAndClearsSource) {
    entt::registry registry;
    entt::dispatcher dispatcher;

    game::data::ItemCatalog catalog;
    InventorySystem inventory_system(registry, dispatcher, catalog);
    HotbarSystem hotbar_system(registry, dispatcher);

    const entt::entity player = registry.create();
    auto& inv = registry.emplace<game::component::InventoryComponent>(player);
    auto& hotbar = registry.emplace<game::component::HotbarComponent>(player);

    const entt::id_type item_id = entt::hashed_string{"test_item"}.value();
    inv.slot(0).item_id_ = item_id;
    inv.slot(0).count_ = 5;
    inv.slot(1).item_id_ = item_id;
    inv.slot(1).count_ = 5;

    hotbar.slot(0).inventory_slot_index_ = 0; // source
    hotbar.slot(1).inventory_slot_index_ = 1; // target

    dispatcher.trigger(game::defs::InventoryMoveRequest{player, 0, 1, true});

    EXPECT_TRUE(inv.slot(0).empty());
    EXPECT_EQ(inv.slot(1).item_id_, item_id);
    EXPECT_EQ(inv.slot(1).count_, 10);

    EXPECT_EQ(hotbar.slot(1).inventory_slot_index_, 1);  // keep target hotkey
    EXPECT_EQ(hotbar.slot(0).inventory_slot_index_, -1); // source hotkey cleared
}

TEST(InventoryHotbarConsistencyTest, HotbarSync_ClearsOutOfRangeMappings) {
    entt::registry registry;
    entt::dispatcher dispatcher;

    HotbarSystem hotbar_system(registry, dispatcher);

    const entt::entity player = registry.create();
    auto& inv = registry.emplace<game::component::InventoryComponent>(player);
    (void)inv;
    auto& hotbar = registry.emplace<game::component::HotbarComponent>(player);

    hotbar.slot(0).inventory_slot_index_ = 999; // out-of-range
    hotbar.slot(1).inventory_slot_index_ = -2;  // invalid negative
    hotbar.slot(2).inventory_slot_index_ = 3;   // valid

    dispatcher.trigger(game::defs::HotbarSyncRequest{player, true});

    EXPECT_EQ(hotbar.slot(0).inventory_slot_index_, -1);
    EXPECT_EQ(hotbar.slot(1).inventory_slot_index_, -1);
    EXPECT_EQ(hotbar.slot(2).inventory_slot_index_, 3);
}

} // namespace game::system

