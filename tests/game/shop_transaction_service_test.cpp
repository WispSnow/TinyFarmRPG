// NOLINTBEGIN
#include <gtest/gtest.h>

#include "appearance_test_fixture_utils.h"
#include "game/component/inventory_component.h"
#include "game/component/player_wallet_component.h"
#include "game/data/item_catalog.h"
#include "game/data/shop_catalog.h"
#include "game/domain/inventory_domain_service.h"
#include "game/domain/shop_transaction_service.h"

#include <filesystem>
#include <limits>
#include <string_view>

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

namespace game::domain {
namespace {

using namespace entt::literals;

[[nodiscard]] std::filesystem::path writeShopConfig(std::string_view prefix, std::string_view body) {
    const auto temp_root = game::test::createUniqueTempDir(prefix);
    const auto config_path = temp_root / "shops.json";
    game::test::writeTextFile(config_path, body);
    return config_path;
}

[[nodiscard]] bool loadProjectItemCatalog(game::data::ItemCatalog& catalog) {
    const auto root = std::filesystem::path{PROJECT_SOURCE_DIR};
    return catalog.loadItemConfig((root / "assets/data/item_config.json").string());
}

[[nodiscard]] bool loadProjectShopCatalog(game::data::ShopCatalog& catalog) {
    const auto root = std::filesystem::path{PROJECT_SOURCE_DIR};
    return catalog.loadFromFile((root / "assets/data/shops.json").string());
}

[[nodiscard]] game::data::ShopCatalog loadShopCatalog(std::string_view body, std::string_view prefix) {
    const auto path = writeShopConfig(prefix, body);
    game::data::ShopCatalog catalog;
    EXPECT_TRUE(catalog.loadFromFile(path.string()));
    return catalog;
}

[[nodiscard]] entt::entity createPlayer(entt::registry& registry, const int gold) {
    const entt::entity player = registry.create();
    registry.emplace<game::component::InventoryComponent>(player);
    registry.emplace<game::component::PlayerWalletComponent>(player, game::component::PlayerWalletComponent{.gold_ = gold});
    return player;
}

constexpr std::string_view kShopConfig = R"json({
  "schema_version": 1,
  "shops": [
    {
      "id": "shop.alpha",
      "title": "Alpha",
      "buy_entries": [
        { "item_id": "potion", "buy_price": 30 },
        { "item_id": "strawberry_seed", "buy_price": 12 }
      ]
    }
  ],
  "sell_rules": [
    { "item_id": "potion", "sell_price": 15 },
    { "item_id": "material_timber", "sell_price": 5 }
  ]
})json";

constexpr std::string_view kOverflowShopConfig = R"json({
  "schema_version": 1,
  "shops": [
    {
      "id": "shop.overflow",
      "title": "Overflow",
      "buy_entries": [
        { "item_id": "potion", "buy_price": 2147483647 }
      ]
    }
  ],
  "sell_rules": [
    { "item_id": "potion", "sell_price": 2147483647 }
  ]
})json";

TEST(ShopTransactionServiceTest, BuyPreviewSucceedsWhenPlayerCanAffordAndHasSpace) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    game::data::ItemCatalog item_catalog;
    ASSERT_TRUE(loadProjectItemCatalog(item_catalog));
    auto shop_catalog = loadShopCatalog(kShopConfig, "shop_transaction_buy_preview_ok");
    InventoryDomainService inventory_domain_service(registry, dispatcher, item_catalog);
    ShopTransactionService service(registry, item_catalog, shop_catalog, inventory_domain_service);

    const entt::entity player = createPlayer(registry, 100);
    const auto preview = service.previewBuy(player, "shop.alpha", "potion"_hs, 2);

    EXPECT_TRUE(preview.canCommit());
    EXPECT_TRUE(preview.can_afford);
    EXPECT_TRUE(preview.has_space);
    EXPECT_EQ(preview.unit_price, 30);
    EXPECT_EQ(preview.total_price, 60);
    EXPECT_EQ(preview.final_gold_after, 40);
    EXPECT_EQ(preview.failure_reason, ShopTradeFailureReason::None);
}

TEST(ShopTransactionServiceTest, BuyPreviewFailsWhenGoldInsufficient) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    game::data::ItemCatalog item_catalog;
    ASSERT_TRUE(loadProjectItemCatalog(item_catalog));
    auto shop_catalog = loadShopCatalog(kShopConfig, "shop_transaction_buy_preview_gold");
    InventoryDomainService inventory_domain_service(registry, dispatcher, item_catalog);
    ShopTransactionService service(registry, item_catalog, shop_catalog, inventory_domain_service);

    const entt::entity player = createPlayer(registry, 20);
    const auto preview = service.previewBuy(player, "shop.alpha", "potion"_hs, 1);

    EXPECT_FALSE(preview.canCommit());
    EXPECT_FALSE(preview.can_afford);
    EXPECT_EQ(preview.failure_reason, ShopTradeFailureReason::InsufficientGold);
}

TEST(ShopTransactionServiceTest, BuyPreviewFailsWhenInventoryFull) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    game::data::ItemCatalog item_catalog;
    ASSERT_TRUE(loadProjectItemCatalog(item_catalog));
    auto shop_catalog = loadShopCatalog(kShopConfig, "shop_transaction_buy_preview_full");
    InventoryDomainService inventory_domain_service(registry, dispatcher, item_catalog);
    ShopTransactionService service(registry, item_catalog, shop_catalog, inventory_domain_service);

    const entt::entity player = createPlayer(registry, 100);
    auto& inventory = registry.get<game::component::InventoryComponent>(player);
    constexpr entt::id_type kFiller = "full_slot"_hs;
    for (auto& slot : inventory.slots_) {
        slot.item_id_ = kFiller;
        slot.count_ = 999;
    }

    const auto preview = service.previewBuy(player, "shop.alpha", "potion"_hs, 1);

    EXPECT_FALSE(preview.canCommit());
    EXPECT_FALSE(preview.has_space);
    EXPECT_EQ(preview.failure_reason, ShopTradeFailureReason::InventoryFull);
}

TEST(ShopTransactionServiceTest, BuyPreviewFailsWhenItemNotSoldHere) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    game::data::ItemCatalog item_catalog;
    ASSERT_TRUE(loadProjectItemCatalog(item_catalog));
    auto shop_catalog = loadShopCatalog(kShopConfig, "shop_transaction_buy_preview_missing");
    InventoryDomainService inventory_domain_service(registry, dispatcher, item_catalog);
    ShopTransactionService service(registry, item_catalog, shop_catalog, inventory_domain_service);

    const entt::entity player = createPlayer(registry, 100);
    const auto preview = service.previewBuy(player, "shop.alpha", "material_timber"_hs, 1);

    EXPECT_FALSE(preview.canCommit());
    EXPECT_EQ(preview.failure_reason, ShopTradeFailureReason::ItemNotSoldHere);
}

TEST(ShopTransactionServiceTest, BuyCommitRejectsOverflowedTotalPriceWithoutMutatingState) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    game::data::ItemCatalog item_catalog;
    ASSERT_TRUE(loadProjectItemCatalog(item_catalog));
    auto shop_catalog = loadShopCatalog(kOverflowShopConfig, "shop_transaction_buy_overflow");
    InventoryDomainService inventory_domain_service(registry, dispatcher, item_catalog);
    ShopTransactionService service(registry, item_catalog, shop_catalog, inventory_domain_service);

    const entt::entity player = createPlayer(registry, std::numeric_limits<int>::max());

    const auto preview = service.previewBuy(player, "shop.overflow", "potion"_hs, 2);
    EXPECT_FALSE(preview.canCommit());
    EXPECT_EQ(preview.failure_reason, ShopTradeFailureReason::InvalidQuantity);

    const auto result = service.commitBuy(player, "shop.overflow", "potion"_hs, 2);

    EXPECT_FALSE(result.completed());
    EXPECT_EQ(result.failure_reason, ShopTradeFailureReason::InvalidQuantity);
    EXPECT_EQ(registry.get<game::component::PlayerWalletComponent>(player).gold_, std::numeric_limits<int>::max());
    const auto& inventory = registry.get<game::component::InventoryComponent>(player);
    for (int i = 0; i < inventory.slotCount(); ++i) {
        EXPECT_TRUE(inventory.slot(i).empty()) << "slot=" << i;
    }
}

TEST(ShopTransactionServiceTest, SellPreviewSucceedsForMatchingSlotAndRule) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    game::data::ItemCatalog item_catalog;
    ASSERT_TRUE(loadProjectItemCatalog(item_catalog));
    auto shop_catalog = loadShopCatalog(kShopConfig, "shop_transaction_sell_preview_ok");
    InventoryDomainService inventory_domain_service(registry, dispatcher, item_catalog);
    ShopTransactionService service(registry, item_catalog, shop_catalog, inventory_domain_service);

    const entt::entity player = createPlayer(registry, 10);
    auto& inventory = registry.get<game::component::InventoryComponent>(player);
    inventory.slot(0).item_id_ = "potion"_hs;
    inventory.slot(0).count_ = 3;

    const auto preview = service.previewSell(player, "potion"_hs, 2, 0);

    EXPECT_TRUE(preview.canCommit());
    EXPECT_EQ(preview.unit_price, 15);
    EXPECT_EQ(preview.total_price, 30);
    EXPECT_EQ(preview.final_gold_after, 40);
    EXPECT_EQ(preview.failure_reason, ShopTradeFailureReason::None);
}

TEST(ShopTransactionServiceTest, SellPreviewFailsWhenItemNotSellable) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    game::data::ItemCatalog item_catalog;
    ASSERT_TRUE(loadProjectItemCatalog(item_catalog));
    auto shop_catalog = loadShopCatalog(kShopConfig, "shop_transaction_sell_preview_not_sellable");
    InventoryDomainService inventory_domain_service(registry, dispatcher, item_catalog);
    ShopTransactionService service(registry, item_catalog, shop_catalog, inventory_domain_service);

    const entt::entity player = createPlayer(registry, 10);
    auto& inventory = registry.get<game::component::InventoryComponent>(player);
    inventory.slot(0).item_id_ = "strawberry_seed"_hs;
    inventory.slot(0).count_ = 4;

    const auto preview = service.previewSell(player, "strawberry_seed"_hs, 1, 0);

    EXPECT_FALSE(preview.canCommit());
    EXPECT_EQ(preview.failure_reason, ShopTradeFailureReason::ItemNotSellable);
}

TEST(ShopTransactionServiceTest, SellPreviewFailsWhenSlotDoesNotMatch) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    game::data::ItemCatalog item_catalog;
    ASSERT_TRUE(loadProjectItemCatalog(item_catalog));
    auto shop_catalog = loadShopCatalog(kShopConfig, "shop_transaction_sell_preview_slot_mismatch");
    InventoryDomainService inventory_domain_service(registry, dispatcher, item_catalog);
    ShopTransactionService service(registry, item_catalog, shop_catalog, inventory_domain_service);

    const entt::entity player = createPlayer(registry, 10);
    auto& inventory = registry.get<game::component::InventoryComponent>(player);
    inventory.slot(0).item_id_ = "potion"_hs;
    inventory.slot(0).count_ = 3;

    const auto preview = service.previewSell(player, "material_timber"_hs, 1, 0);

    EXPECT_FALSE(preview.canCommit());
    EXPECT_EQ(preview.failure_reason, ShopTradeFailureReason::SlotMismatch);
}

TEST(ShopTransactionServiceTest, SellPreviewFailsWhenSlotCountInsufficient) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    game::data::ItemCatalog item_catalog;
    ASSERT_TRUE(loadProjectItemCatalog(item_catalog));
    auto shop_catalog = loadShopCatalog(kShopConfig, "shop_transaction_sell_preview_insufficient");
    InventoryDomainService inventory_domain_service(registry, dispatcher, item_catalog);
    ShopTransactionService service(registry, item_catalog, shop_catalog, inventory_domain_service);

    const entt::entity player = createPlayer(registry, 10);
    auto& inventory = registry.get<game::component::InventoryComponent>(player);
    inventory.slot(0).item_id_ = "potion"_hs;
    inventory.slot(0).count_ = 1;

    const auto preview = service.previewSell(player, "potion"_hs, 2, 0);

    EXPECT_FALSE(preview.canCommit());
    EXPECT_EQ(preview.failure_reason, ShopTradeFailureReason::InsufficientItemCount);
}

TEST(ShopTransactionServiceTest, SellCommitRejectsOverflowedTotalPriceWithoutMutatingState) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    game::data::ItemCatalog item_catalog;
    ASSERT_TRUE(loadProjectItemCatalog(item_catalog));
    auto shop_catalog = loadShopCatalog(kOverflowShopConfig, "shop_transaction_sell_total_overflow");
    InventoryDomainService inventory_domain_service(registry, dispatcher, item_catalog);
    ShopTransactionService service(registry, item_catalog, shop_catalog, inventory_domain_service);

    const entt::entity player = createPlayer(registry, 0);
    auto& inventory = registry.get<game::component::InventoryComponent>(player);
    inventory.slot(0).item_id_ = "potion"_hs;
    inventory.slot(0).count_ = 2;

    const auto preview = service.previewSell(player, "potion"_hs, 2, 0);
    EXPECT_FALSE(preview.canCommit());
    EXPECT_EQ(preview.failure_reason, ShopTradeFailureReason::InvalidQuantity);

    const auto result = service.commitSell(player, "potion"_hs, 2, 0);

    EXPECT_FALSE(result.completed());
    EXPECT_EQ(result.failure_reason, ShopTradeFailureReason::InvalidQuantity);
    EXPECT_EQ(registry.get<game::component::PlayerWalletComponent>(player).gold_, 0);
    EXPECT_EQ(inventory.slot(0).item_id_, "potion"_hs);
    EXPECT_EQ(inventory.slot(0).count_, 2);
}

TEST(ShopTransactionServiceTest, SellCommitRejectsOverflowedFinalGoldWithoutMutatingState) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    game::data::ItemCatalog item_catalog;
    ASSERT_TRUE(loadProjectItemCatalog(item_catalog));
    auto shop_catalog = loadShopCatalog(kOverflowShopConfig, "shop_transaction_sell_gold_overflow");
    InventoryDomainService inventory_domain_service(registry, dispatcher, item_catalog);
    ShopTransactionService service(registry, item_catalog, shop_catalog, inventory_domain_service);

    const entt::entity player = createPlayer(registry, 1);
    auto& inventory = registry.get<game::component::InventoryComponent>(player);
    inventory.slot(0).item_id_ = "potion"_hs;
    inventory.slot(0).count_ = 1;

    const auto preview = service.previewSell(player, "potion"_hs, 1, 0);
    EXPECT_FALSE(preview.canCommit());
    EXPECT_EQ(preview.failure_reason, ShopTradeFailureReason::InvalidQuantity);

    const auto result = service.commitSell(player, "potion"_hs, 1, 0);

    EXPECT_FALSE(result.completed());
    EXPECT_EQ(result.failure_reason, ShopTradeFailureReason::InvalidQuantity);
    EXPECT_EQ(registry.get<game::component::PlayerWalletComponent>(player).gold_, 1);
    EXPECT_EQ(inventory.slot(0).item_id_, "potion"_hs);
    EXPECT_EQ(inventory.slot(0).count_, 1);
}

TEST(ShopTransactionServiceTest, BuyCommitWritesInventoryAndGold) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    game::data::ItemCatalog item_catalog;
    ASSERT_TRUE(loadProjectItemCatalog(item_catalog));
    auto shop_catalog = loadShopCatalog(kShopConfig, "shop_transaction_buy_commit_ok");
    InventoryDomainService inventory_domain_service(registry, dispatcher, item_catalog);
    ShopTransactionService service(registry, item_catalog, shop_catalog, inventory_domain_service);

    const entt::entity player = createPlayer(registry, 100);
    const auto result = service.commitBuy(player, "shop.alpha", "potion"_hs, 2);

    EXPECT_TRUE(result.completed());
    EXPECT_EQ(result.total_price, 60);
    EXPECT_EQ(result.final_gold_after, 40);
    EXPECT_EQ(registry.get<game::component::PlayerWalletComponent>(player).gold_, 40);
    const auto& inventory = registry.get<game::component::InventoryComponent>(player);
    EXPECT_EQ(inventory.slot(0).item_id_, "potion"_hs);
    EXPECT_EQ(inventory.slot(0).count_, 2);
}

TEST(ShopTransactionServiceTest, SellCommitWritesInventoryAndGold) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    game::data::ItemCatalog item_catalog;
    ASSERT_TRUE(loadProjectItemCatalog(item_catalog));
    auto shop_catalog = loadShopCatalog(kShopConfig, "shop_transaction_sell_commit_ok");
    InventoryDomainService inventory_domain_service(registry, dispatcher, item_catalog);
    ShopTransactionService service(registry, item_catalog, shop_catalog, inventory_domain_service);

    const entt::entity player = createPlayer(registry, 10);
    auto& inventory = registry.get<game::component::InventoryComponent>(player);
    inventory.slot(0).item_id_ = "potion"_hs;
    inventory.slot(0).count_ = 3;

    const auto result = service.commitSell(player, "potion"_hs, 2, 0);

    EXPECT_TRUE(result.completed());
    EXPECT_EQ(result.total_price, 30);
    EXPECT_EQ(result.final_gold_after, 40);
    EXPECT_EQ(registry.get<game::component::PlayerWalletComponent>(player).gold_, 40);
    EXPECT_EQ(inventory.slot(0).item_id_, "potion"_hs);
    EXPECT_EQ(inventory.slot(0).count_, 1);
}

TEST(ShopTransactionServiceTest, BuyCommitFailsWhenStateChangesAfterPreview) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    game::data::ItemCatalog item_catalog;
    ASSERT_TRUE(loadProjectItemCatalog(item_catalog));
    auto shop_catalog = loadShopCatalog(kShopConfig, "shop_transaction_buy_commit_changed");
    InventoryDomainService inventory_domain_service(registry, dispatcher, item_catalog);
    ShopTransactionService service(registry, item_catalog, shop_catalog, inventory_domain_service);

    const entt::entity player = createPlayer(registry, 100);
    const auto preview = service.previewBuy(player, "shop.alpha", "potion"_hs, 1);
    ASSERT_TRUE(preview.canCommit());

    auto& inventory = registry.get<game::component::InventoryComponent>(player);
    constexpr entt::id_type kFiller = "full_slot"_hs;
    for (auto& slot : inventory.slots_) {
        slot.item_id_ = kFiller;
        slot.count_ = 999;
    }

    const auto result = service.commitBuy(player, "shop.alpha", "potion"_hs, 1);

    EXPECT_FALSE(result.completed());
    EXPECT_EQ(result.failure_reason, ShopTradeFailureReason::InventoryFull);
    EXPECT_EQ(registry.get<game::component::PlayerWalletComponent>(player).gold_, 100);
}

TEST(ShopTransactionServiceTest, BuyCommitRejectsPartialCapacityWithoutMutatingInventoryOrGold) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    game::data::ItemCatalog item_catalog;
    ASSERT_TRUE(loadProjectItemCatalog(item_catalog));
    auto shop_catalog = loadShopCatalog(kShopConfig, "shop_transaction_buy_commit_partial_capacity");
    InventoryDomainService inventory_domain_service(registry, dispatcher, item_catalog);
    ShopTransactionService service(registry, item_catalog, shop_catalog, inventory_domain_service);

    const entt::entity player = createPlayer(registry, 100);
    auto& inventory = registry.get<game::component::InventoryComponent>(player);
    inventory.slot(0).item_id_ = "potion"_hs;
    inventory.slot(0).count_ = 98;
    for (int i = 1; i < inventory.slotCount(); ++i) {
        inventory.slot(i).item_id_ = "strawberry_seed"_hs;
        inventory.slot(i).count_ = 999;
    }

    const auto result = service.commitBuy(player, "shop.alpha", "potion"_hs, 2);

    EXPECT_FALSE(result.completed());
    EXPECT_EQ(result.failure_reason, ShopTradeFailureReason::InventoryFull);
    EXPECT_EQ(registry.get<game::component::PlayerWalletComponent>(player).gold_, 100);
    EXPECT_EQ(inventory.slot(0).item_id_, "potion"_hs);
    EXPECT_EQ(inventory.slot(0).count_, 98);
    for (int i = 1; i < inventory.slotCount(); ++i) {
        EXPECT_EQ(inventory.slot(i).item_id_, "strawberry_seed"_hs) << "slot=" << i;
        EXPECT_EQ(inventory.slot(i).count_, 999) << "slot=" << i;
    }
}

TEST(ShopTransactionServiceTest, SellCommitFailsWhenStateChangesAfterPreview) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    game::data::ItemCatalog item_catalog;
    ASSERT_TRUE(loadProjectItemCatalog(item_catalog));
    auto shop_catalog = loadShopCatalog(kShopConfig, "shop_transaction_sell_commit_changed");
    InventoryDomainService inventory_domain_service(registry, dispatcher, item_catalog);
    ShopTransactionService service(registry, item_catalog, shop_catalog, inventory_domain_service);

    const entt::entity player = createPlayer(registry, 10);
    auto& inventory = registry.get<game::component::InventoryComponent>(player);
    inventory.slot(0).item_id_ = "potion"_hs;
    inventory.slot(0).count_ = 2;

    const auto preview = service.previewSell(player, "potion"_hs, 2, 0);
    ASSERT_TRUE(preview.canCommit());

    inventory.slot(0).count_ = 1;

    const auto result = service.commitSell(player, "potion"_hs, 2, 0);

    EXPECT_FALSE(result.completed());
    EXPECT_EQ(result.failure_reason, ShopTradeFailureReason::InsufficientItemCount);
    EXPECT_EQ(registry.get<game::component::PlayerWalletComponent>(player).gold_, 10);
}

TEST(ShopTransactionServiceTest, BuyCommitWritesEquipmentInventoryAndGoldFromProjectShopData) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    game::data::ItemCatalog item_catalog;
    ASSERT_TRUE(loadProjectItemCatalog(item_catalog));
    game::data::ShopCatalog shop_catalog;
    ASSERT_TRUE(loadProjectShopCatalog(shop_catalog));
    InventoryDomainService inventory_domain_service(registry, dispatcher, item_catalog);
    ShopTransactionService service(registry, item_catalog, shop_catalog, inventory_domain_service);

    const entt::entity player = createPlayer(registry, 300);
    const auto result = service.commitBuy(player, "shop.village.general.day", "equip_wooden_sword"_hs, 1);

    EXPECT_TRUE(result.completed());
    EXPECT_EQ(result.total_price, 60);
    EXPECT_EQ(result.final_gold_after, 240);
    EXPECT_EQ(registry.get<game::component::PlayerWalletComponent>(player).gold_, 240);

    const auto& inventory = registry.get<game::component::InventoryComponent>(player);
    EXPECT_EQ(inventory.slot(0).item_id_, "equip_wooden_sword"_hs);
    EXPECT_EQ(inventory.slot(0).count_, 1);
}

TEST(ShopTransactionServiceTest, SellCommitWritesEquipmentInventoryAndGoldFromProjectShopData) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    game::data::ItemCatalog item_catalog;
    ASSERT_TRUE(loadProjectItemCatalog(item_catalog));
    game::data::ShopCatalog shop_catalog;
    ASSERT_TRUE(loadProjectShopCatalog(shop_catalog));
    InventoryDomainService inventory_domain_service(registry, dispatcher, item_catalog);
    ShopTransactionService service(registry, item_catalog, shop_catalog, inventory_domain_service);

    const entt::entity player = createPlayer(registry, 10);
    auto& inventory = registry.get<game::component::InventoryComponent>(player);
    inventory.slot(0).item_id_ = "equip_wooden_sword"_hs;
    inventory.slot(0).count_ = 1;

    const auto result = service.commitSell(player, "equip_wooden_sword"_hs, 1, 0);

    EXPECT_TRUE(result.completed());
    EXPECT_EQ(result.total_price, 24);
    EXPECT_EQ(result.final_gold_after, 34);
    EXPECT_EQ(registry.get<game::component::PlayerWalletComponent>(player).gold_, 34);
    EXPECT_TRUE(inventory.slot(0).empty());
}

} // namespace
} // namespace game::domain
// NOLINTEND
