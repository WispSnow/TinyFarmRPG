#include <gtest/gtest.h>

#include "game/component/inventory_component.h"
#include "game/data/item_catalog.h"
#include "game/defs/events.h"
#include "game/system/inventory_system.h"
#include "game/system/item_use_system.h"

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

namespace {

struct DialogueCapture {
    std::vector<game::defs::DialogueShowEvent> shows{};
    void onShow(const game::defs::DialogueShowEvent& evt) { shows.push_back(evt); }
};

std::string testItemConfigPath() {
    return std::string(PROJECT_SOURCE_DIR) + "/tests/data/item_use_items.json";
}

} // namespace

namespace game::system {

TEST(ItemUseSystemTest, UseCropItem_CountOne_ReplacesWithSeeds) {
    entt::registry registry;
    entt::dispatcher dispatcher;

    game::data::ItemCatalog catalog;
    ASSERT_TRUE(catalog.loadItemConfig(testItemConfigPath()));

    InventorySystem inventory_system(registry, dispatcher, catalog);
    ItemUseSystem item_use_system(registry, dispatcher, catalog);

    const entt::entity player = registry.create();
    auto& inv = registry.emplace<game::component::InventoryComponent>(player);
    inv.slot(0).item_id_ = entt::hashed_string{"strawberry_item"}.value();
    inv.slot(0).count_ = 1;

    dispatcher.trigger(game::defs::UseItemRequest{player, 0, 1, false});

    EXPECT_EQ(inv.slot(0).item_id_, entt::hashed_string{"strawberry_seed"}.value());
    EXPECT_EQ(inv.slot(0).count_, 3);
}

TEST(ItemUseSystemTest, UseCropItem_CountTwo_NoSpace_DoesNotConsume) {
    entt::registry registry;
    entt::dispatcher dispatcher;

    game::data::ItemCatalog catalog;
    ASSERT_TRUE(catalog.loadItemConfig(testItemConfigPath()));

    InventorySystem inventory_system(registry, dispatcher, catalog);
    ItemUseSystem item_use_system(registry, dispatcher, catalog);

    const entt::entity player = registry.create();
    auto& inv = registry.emplace<game::component::InventoryComponent>(player);

    // Fill all slots so that adding seeds is impossible unless the source slot becomes empty.
    const entt::id_type dummy_id = entt::hashed_string{"dummy_full"}.value();
    for (int i = 0; i < inv.slotCount(); ++i) {
        inv.slot(i).item_id_ = dummy_id;
        inv.slot(i).count_ = 1;
    }

    inv.slot(0).item_id_ = entt::hashed_string{"strawberry_item"}.value();
    inv.slot(0).count_ = 2; // consume 1 still leaves the slot occupied

    dispatcher.trigger(game::defs::UseItemRequest{player, 0, 1, false});

    EXPECT_EQ(inv.slot(0).item_id_, entt::hashed_string{"strawberry_item"}.value());
    EXPECT_EQ(inv.slot(0).count_, 2);
    EXPECT_EQ(inv.slot(1).item_id_, dummy_id);
    EXPECT_EQ(inv.slot(1).count_, 1);
}

TEST(ItemUseSystemTest, UseCropItem_CountOne_FullInventoryStillSucceeds) {
    entt::registry registry;
    entt::dispatcher dispatcher;

    game::data::ItemCatalog catalog;
    ASSERT_TRUE(catalog.loadItemConfig(testItemConfigPath()));

    InventorySystem inventory_system(registry, dispatcher, catalog);
    ItemUseSystem item_use_system(registry, dispatcher, catalog);

    const entt::entity player = registry.create();
    auto& inv = registry.emplace<game::component::InventoryComponent>(player);

    const entt::id_type dummy_id = entt::hashed_string{"dummy_full"}.value();
    for (int i = 0; i < inv.slotCount(); ++i) {
        inv.slot(i).item_id_ = dummy_id;
        inv.slot(i).count_ = 1;
    }

    inv.slot(0).item_id_ = entt::hashed_string{"strawberry_item"}.value();
    inv.slot(0).count_ = 1; // consuming frees the slot for seeds

    dispatcher.trigger(game::defs::UseItemRequest{player, 0, 1, false});

    EXPECT_EQ(inv.slot(0).item_id_, entt::hashed_string{"strawberry_seed"}.value());
    EXPECT_EQ(inv.slot(0).count_, 3);
}

TEST(ItemUseSystemTest, UseCropItem_ShowPrompt_EnqueuesNotification) {
    entt::registry registry;
    entt::dispatcher dispatcher;

    game::data::ItemCatalog catalog;
    ASSERT_TRUE(catalog.loadItemConfig(testItemConfigPath()));

    InventorySystem inventory_system(registry, dispatcher, catalog);
    ItemUseSystem item_use_system(registry, dispatcher, catalog);

    DialogueCapture capture{};
    dispatcher.sink<game::defs::DialogueShowEvent>().connect<&DialogueCapture::onShow>(&capture);

    const entt::entity player = registry.create();
    auto& inv = registry.emplace<game::component::InventoryComponent>(player);
    inv.slot(0).item_id_ = entt::hashed_string{"potato_item"}.value();
    inv.slot(0).count_ = 1;

    dispatcher.trigger(game::defs::UseItemRequest{player, 0, 1, true});
    dispatcher.update();

    ASSERT_EQ(capture.shows.size(), 1u);
    EXPECT_EQ(capture.shows[0].channel, 2);
}

} // namespace game::system

