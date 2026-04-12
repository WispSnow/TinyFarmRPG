#include <gtest/gtest.h>

#include "appearance_test_fixture_utils.h"
#include "game/component/hotbar_component.h"
#include "game/component/inventory_component.h"
#include "game/component/player_wallet_component.h"
#include "game/data/item_catalog.h"
#include "game/data/shop_catalog.h"
#include "game/defs/commands.h"
#include "game/defs/events.h"
#include "game/domain/inventory_domain_service.h"
#include "game/domain/shop_transaction_service.h"
#include "game/system/hotbar_system.h"
#include "game/system/inventory_system.h"

#include <algorithm>
#include <filesystem>
#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <string>
#include <string_view>

namespace game::system {

namespace {

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

std::string testItemConfigPath() {
    return std::string(PROJECT_SOURCE_DIR) + "/tests/data/item_use_items.json";
}

std::string projectItemConfigPath() {
    return std::string(PROJECT_SOURCE_DIR) + "/assets/data/item_config.json";
}

struct InventoryChangedCapture {
    std::vector<game::defs::InventoryChanged> events{};
    void onEvent(const game::defs::InventoryChanged& evt) { events.push_back(evt); }
};

struct HotbarChangedCapture {
    std::vector<game::defs::HotbarChanged> events{};
    void onEvent(const game::defs::HotbarChanged& evt) { events.push_back(evt); }
};

[[nodiscard]] std::filesystem::path writeShopConfig(std::string_view prefix, std::string_view body) {
    const auto temp_root = game::test::createUniqueTempDir(prefix);
    const auto config_path = temp_root / "shops.json";
    game::test::writeTextFile(config_path, body);
    return config_path;
}

[[nodiscard]] game::data::ShopCatalog loadShopCatalog(std::string_view body, std::string_view prefix) {
    const auto path = writeShopConfig(prefix, body);
    game::data::ShopCatalog catalog;
    EXPECT_TRUE(catalog.loadFromFile(path.string()));
    return catalog;
}

constexpr std::string_view kSellShopConfig = R"json({
  "schema_version": 1,
  "shops": [
    {
      "id": "shop.alpha",
      "title": "Alpha",
      "buy_entries": [
        { "item_id": "potion", "buy_price": 30 }
      ]
    }
  ],
  "sell_rules": [
    { "item_id": "potion", "sell_price": 15 }
  ]
})json";

} // namespace

TEST(InventoryHotbarConsistencyTest, MergeIntoReferencedSlot_KeepsTargetHotkeyAndClearsSource) {
    entt::registry registry;
    entt::dispatcher dispatcher;

    game::data::ItemCatalog catalog;
    game::domain::InventoryDomainService inventory_domain_service(registry, dispatcher, catalog);
    InventorySystem inventory_system(registry, dispatcher, catalog, inventory_domain_service);
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

    dispatcher.trigger(game::defs::InventoryMoveCommand{player, 0, 1, true});

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

    dispatcher.trigger(game::defs::HotbarSyncCommand{player, true});

    EXPECT_EQ(hotbar.slot(0).inventory_slot_index_, -1);
    EXPECT_EQ(hotbar.slot(1).inventory_slot_index_, -1);
    EXPECT_EQ(hotbar.slot(2).inventory_slot_index_, 3);
}

TEST(InventoryHotbarConsistencyTest, MergePartial_KeepsHotbarMappingsUnchanged) {
    entt::registry registry;
    entt::dispatcher dispatcher;

    game::data::ItemCatalog catalog;
    game::domain::InventoryDomainService inventory_domain_service(registry, dispatcher, catalog);
    InventorySystem inventory_system(registry, dispatcher, catalog, inventory_domain_service);
    HotbarSystem hotbar_system(registry, dispatcher);

    const entt::entity player = registry.create();
    auto& inv = registry.emplace<game::component::InventoryComponent>(player);
    auto& hotbar = registry.emplace<game::component::HotbarComponent>(player);

    const entt::id_type item_id = entt::hashed_string{"partial_merge_item"}.value();
    inv.slot(0).item_id_ = item_id;
    inv.slot(0).count_ = 5;
    inv.slot(1).item_id_ = item_id;
    inv.slot(1).count_ = 998; // stack_limit 默认为 999，触发部分合并

    hotbar.slot(0).inventory_slot_index_ = 0;
    hotbar.slot(1).inventory_slot_index_ = 1;

    dispatcher.trigger(game::defs::InventoryMoveCommand{player, 0, 1, true});

    EXPECT_EQ(inv.slot(0).item_id_, item_id);
    EXPECT_EQ(inv.slot(0).count_, 4);
    EXPECT_EQ(inv.slot(1).item_id_, item_id);
    EXPECT_EQ(inv.slot(1).count_, 999);

    EXPECT_EQ(hotbar.slot(0).inventory_slot_index_, 0);
    EXPECT_EQ(hotbar.slot(1).inventory_slot_index_, 1);
}

TEST(InventoryHotbarConsistencyTest, Sort_RemapsHotbarMappingsAndEmitsFullSync) {
    entt::registry registry;
    entt::dispatcher dispatcher;

    game::data::ItemCatalog catalog;
    ASSERT_TRUE(catalog.loadItemConfig(testItemConfigPath()));
    game::domain::InventoryDomainService inventory_domain_service(registry, dispatcher, catalog);
    InventorySystem inventory_system(registry, dispatcher, catalog, inventory_domain_service);
    HotbarSystem hotbar_system(registry, dispatcher);

    const entt::entity player = registry.create();
    auto& inv = registry.emplace<game::component::InventoryComponent>(player);
    auto& hotbar = registry.emplace<game::component::HotbarComponent>(player);

    const entt::id_type crop_item_id = entt::hashed_string{"strawberry_item"}.value();
    const entt::id_type seed_item_id = entt::hashed_string{"potato_seed"}.value();

    inv.slot(0).item_id_ = crop_item_id;
    inv.slot(0).count_ = 1;
    inv.slot(1).item_id_ = seed_item_id;
    inv.slot(1).count_ = 4;

    hotbar.slot(0).inventory_slot_index_ = 0;
    hotbar.slot(1).inventory_slot_index_ = 1;

    InventoryChangedCapture inventory_capture{};
    HotbarChangedCapture hotbar_capture{};
    dispatcher.sink<game::defs::InventoryChanged>().connect<&InventoryChangedCapture::onEvent>(&inventory_capture);
    dispatcher.sink<game::defs::HotbarChanged>().connect<&HotbarChangedCapture::onEvent>(&hotbar_capture);

    dispatcher.trigger(game::defs::InventorySortCommand{player});

    EXPECT_EQ(inv.slot(0).item_id_, seed_item_id);
    EXPECT_EQ(inv.slot(0).count_, 4);
    EXPECT_EQ(inv.slot(1).item_id_, crop_item_id);
    EXPECT_EQ(inv.slot(1).count_, 1);

    EXPECT_EQ(hotbar.slot(0).inventory_slot_index_, 1);
    EXPECT_EQ(hotbar.slot(1).inventory_slot_index_, 0);

    ASSERT_FALSE(inventory_capture.events.empty());
    EXPECT_TRUE(inventory_capture.events.back().full_sync);
    EXPECT_EQ(inventory_capture.events.back().slots.size(), static_cast<std::size_t>(inv.slotCount()));

    ASSERT_FALSE(hotbar_capture.events.empty());
    EXPECT_TRUE(hotbar_capture.events.back().full_sync);
    ASSERT_GE(hotbar_capture.events.back().slots.size(), 2u);
    EXPECT_EQ(hotbar_capture.events.back().slots[0].inventory_slot_index, 1);
    EXPECT_EQ(hotbar_capture.events.back().slots[1].inventory_slot_index, 0);
}

TEST(InventoryHotbarConsistencyTest, ShopSellCommit_KeepsHotbarMappingWhenSlotStillHasItems) {
    entt::registry registry;
    entt::dispatcher dispatcher;

    game::data::ItemCatalog catalog;
    ASSERT_TRUE(catalog.loadItemConfig(projectItemConfigPath()));
    auto shop_catalog = loadShopCatalog(kSellShopConfig, "hotbar_sell_partial");
    game::domain::InventoryDomainService inventory_domain_service(registry, dispatcher, catalog);
    HotbarSystem hotbar_system(registry, dispatcher);
    game::domain::ShopTransactionService shop_transaction_service(registry, catalog, shop_catalog, inventory_domain_service);

    InventoryChangedCapture inventory_capture{};
    HotbarChangedCapture hotbar_capture{};
    dispatcher.sink<game::defs::InventoryChanged>().connect<&InventoryChangedCapture::onEvent>(&inventory_capture);
    dispatcher.sink<game::defs::HotbarChanged>().connect<&HotbarChangedCapture::onEvent>(&hotbar_capture);

    const entt::entity player = registry.create();
    auto& inventory = registry.emplace<game::component::InventoryComponent>(player);
    auto& hotbar = registry.emplace<game::component::HotbarComponent>(player);
    auto& wallet =
        registry.emplace<game::component::PlayerWalletComponent>(player, game::component::PlayerWalletComponent{.gold_ = 10});

    inventory.slot(0).item_id_ = entt::hashed_string{"potion"}.value();
    inventory.slot(0).count_ = 3;
    hotbar.slot(0).inventory_slot_index_ = 0;

    const auto result = shop_transaction_service.commitSell(player, entt::hashed_string{"potion"}.value(), 2, 0);

    ASSERT_TRUE(result.completed());
    EXPECT_EQ(wallet.gold_, 40);
    EXPECT_EQ(inventory.slot(0).item_id_, entt::hashed_string{"potion"}.value());
    EXPECT_EQ(inventory.slot(0).count_, 1);
    EXPECT_EQ(hotbar.slot(0).inventory_slot_index_, 0);

    ASSERT_FALSE(inventory_capture.events.empty());
    EXPECT_FALSE(inventory_capture.events.back().from_add);

    ASSERT_FALSE(hotbar_capture.events.empty());
    const auto& hotbar_event = hotbar_capture.events.back();
    const auto it = std::find_if(hotbar_event.slots.begin(), hotbar_event.slots.end(), [](const auto& update) {
        return update.hotbar_index == 0;
    });
    ASSERT_NE(it, hotbar_event.slots.end());
    EXPECT_EQ(it->inventory_slot_index, 0);
    EXPECT_EQ(it->item_id, entt::hashed_string{"potion"}.value());
    EXPECT_EQ(it->count, 1);
}

TEST(InventoryHotbarConsistencyTest, ShopSellCommit_ClearsHotbarMappingWhenSlotBecomesEmpty) {
    entt::registry registry;
    entt::dispatcher dispatcher;

    game::data::ItemCatalog catalog;
    ASSERT_TRUE(catalog.loadItemConfig(projectItemConfigPath()));
    auto shop_catalog = loadShopCatalog(kSellShopConfig, "hotbar_sell_clear");
    game::domain::InventoryDomainService inventory_domain_service(registry, dispatcher, catalog);
    HotbarSystem hotbar_system(registry, dispatcher);
    game::domain::ShopTransactionService shop_transaction_service(registry, catalog, shop_catalog, inventory_domain_service);

    InventoryChangedCapture inventory_capture{};
    HotbarChangedCapture hotbar_capture{};
    dispatcher.sink<game::defs::InventoryChanged>().connect<&InventoryChangedCapture::onEvent>(&inventory_capture);
    dispatcher.sink<game::defs::HotbarChanged>().connect<&HotbarChangedCapture::onEvent>(&hotbar_capture);

    const entt::entity player = registry.create();
    auto& inventory = registry.emplace<game::component::InventoryComponent>(player);
    auto& hotbar = registry.emplace<game::component::HotbarComponent>(player);
    auto& wallet =
        registry.emplace<game::component::PlayerWalletComponent>(player, game::component::PlayerWalletComponent{.gold_ = 10});

    inventory.slot(0).item_id_ = entt::hashed_string{"potion"}.value();
    inventory.slot(0).count_ = 2;
    hotbar.slot(0).inventory_slot_index_ = 0;

    const auto result = shop_transaction_service.commitSell(player, entt::hashed_string{"potion"}.value(), 2, 0);

    ASSERT_TRUE(result.completed());
    EXPECT_EQ(wallet.gold_, 40);
    EXPECT_TRUE(inventory.slot(0).empty());
    EXPECT_EQ(hotbar.slot(0).inventory_slot_index_, -1);

    ASSERT_FALSE(inventory_capture.events.empty());
    EXPECT_FALSE(inventory_capture.events.back().from_add);

    ASSERT_FALSE(hotbar_capture.events.empty());
    const auto& hotbar_event = hotbar_capture.events.back();
    const auto it = std::find_if(hotbar_event.slots.begin(), hotbar_event.slots.end(), [](const auto& update) {
        return update.hotbar_index == 0;
    });
    ASSERT_NE(it, hotbar_event.slots.end());
    EXPECT_EQ(it->inventory_slot_index, -1);
    EXPECT_EQ(it->item_id, static_cast<entt::id_type>(entt::null));
    EXPECT_EQ(it->count, 0);
}

} // namespace game::system
