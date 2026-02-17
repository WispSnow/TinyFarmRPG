#include <gtest/gtest.h>

#include "game/component/hotbar_component.h"
#include "game/component/inventory_component.h"
#include "game/data/item_catalog.h"
#include "game/defs/commands.h"
#include "game/defs/events.h"
#include "game/domain/inventory_domain_service.h"
#include "game/system/hotbar_system.h"
#include "game/system/inventory_system.h"

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <vector>

namespace {

struct InventoryChangedCapture {
    std::vector<game::defs::InventoryChanged> events{};
    void onEvent(const game::defs::InventoryChanged& evt) { events.push_back(evt); }
};

struct MoveTestContext {
    entt::registry registry{};
    entt::dispatcher dispatcher{};
    game::data::ItemCatalog catalog{};
    game::domain::InventoryDomainService inventory_domain_service;
    game::system::InventorySystem inventory_system;
    game::system::HotbarSystem hotbar_system;

    MoveTestContext()
        : inventory_domain_service(registry, dispatcher, catalog),
          inventory_system(registry, dispatcher, catalog, inventory_domain_service),
          hotbar_system(registry, dispatcher) {
    }
};

entt::entity createPlayer(MoveTestContext& ctx) {
    const entt::entity player = ctx.registry.create();
    ctx.registry.emplace<game::component::InventoryComponent>(player);
    ctx.registry.emplace<game::component::HotbarComponent>(player);
    return player;
}

} // namespace

namespace game::system {

TEST(InventoryMoveHotbarFollowTest, MoveToEmpty_SourceHotkeyFollowsToDestination) {
    MoveTestContext ctx;
    InventoryChangedCapture capture{};
    ctx.dispatcher.sink<game::defs::InventoryChanged>().connect<&InventoryChangedCapture::onEvent>(&capture);

    const entt::entity player = createPlayer(ctx);
    auto& inv = ctx.registry.get<game::component::InventoryComponent>(player);
    auto& hotbar = ctx.registry.get<game::component::HotbarComponent>(player);

    const entt::id_type item_id = entt::hashed_string{"move_item"}.value();
    inv.slot(0).item_id_ = item_id;
    inv.slot(0).count_ = 2;
    hotbar.slot(0).inventory_slot_index_ = 0;

    ctx.dispatcher.trigger(game::defs::InventoryMoveCommand{player, 0, 1, true});

    EXPECT_TRUE(inv.slot(0).empty());
    EXPECT_EQ(inv.slot(1).item_id_, item_id);
    EXPECT_EQ(inv.slot(1).count_, 2);
    EXPECT_EQ(hotbar.slot(0).inventory_slot_index_, 1);

    ASSERT_EQ(capture.events.size(), 1u);
    EXPECT_EQ(capture.events[0].move_kind, game::defs::InventoryMoveKind::MoveToEmpty);
    EXPECT_EQ(capture.events[0].move_from_slot, 0);
    EXPECT_EQ(capture.events[0].move_to_slot, 1);
}

TEST(InventoryMoveHotbarFollowTest, Swap_SwapsHotbarMappings) {
    MoveTestContext ctx;
    InventoryChangedCapture capture{};
    ctx.dispatcher.sink<game::defs::InventoryChanged>().connect<&InventoryChangedCapture::onEvent>(&capture);

    const entt::entity player = createPlayer(ctx);
    auto& inv = ctx.registry.get<game::component::InventoryComponent>(player);
    auto& hotbar = ctx.registry.get<game::component::HotbarComponent>(player);

    const entt::id_type item_a = entt::hashed_string{"swap_item_a"}.value();
    const entt::id_type item_b = entt::hashed_string{"swap_item_b"}.value();
    inv.slot(0).item_id_ = item_a;
    inv.slot(0).count_ = 1;
    inv.slot(1).item_id_ = item_b;
    inv.slot(1).count_ = 1;
    hotbar.slot(0).inventory_slot_index_ = 0;
    hotbar.slot(1).inventory_slot_index_ = 1;

    ctx.dispatcher.trigger(game::defs::InventoryMoveCommand{player, 0, 1, true});

    EXPECT_EQ(inv.slot(0).item_id_, item_b);
    EXPECT_EQ(inv.slot(1).item_id_, item_a);
    EXPECT_EQ(hotbar.slot(0).inventory_slot_index_, 1);
    EXPECT_EQ(hotbar.slot(1).inventory_slot_index_, 0);

    ASSERT_EQ(capture.events.size(), 1u);
    EXPECT_EQ(capture.events[0].move_kind, game::defs::InventoryMoveKind::Swap);
    EXPECT_EQ(capture.events[0].move_from_slot, 0);
    EXPECT_EQ(capture.events[0].move_to_slot, 1);
}

TEST(InventoryMoveHotbarFollowTest, MergeIntoReferencedSlot_KeepsTargetHotkeyAndClearsSource) {
    MoveTestContext ctx;
    InventoryChangedCapture capture{};
    ctx.dispatcher.sink<game::defs::InventoryChanged>().connect<&InventoryChangedCapture::onEvent>(&capture);

    const entt::entity player = createPlayer(ctx);
    auto& inv = ctx.registry.get<game::component::InventoryComponent>(player);
    auto& hotbar = ctx.registry.get<game::component::HotbarComponent>(player);

    const entt::id_type item_id = entt::hashed_string{"merge_item_referenced"}.value();
    inv.slot(0).item_id_ = item_id;
    inv.slot(0).count_ = 5;
    inv.slot(1).item_id_ = item_id;
    inv.slot(1).count_ = 5;
    hotbar.slot(0).inventory_slot_index_ = 0;
    hotbar.slot(1).inventory_slot_index_ = 1;

    ctx.dispatcher.trigger(game::defs::InventoryMoveCommand{player, 0, 1, true});

    EXPECT_TRUE(inv.slot(0).empty());
    EXPECT_EQ(inv.slot(1).count_, 10);
    EXPECT_EQ(hotbar.slot(1).inventory_slot_index_, 1);
    EXPECT_EQ(hotbar.slot(0).inventory_slot_index_, -1);

    ASSERT_EQ(capture.events.size(), 1u);
    EXPECT_EQ(capture.events[0].move_kind, game::defs::InventoryMoveKind::Merge);
}

TEST(InventoryMoveHotbarFollowTest, MergeIntoUnreferencedSlot_SourceHotkeyFollowsToTarget) {
    MoveTestContext ctx;
    InventoryChangedCapture capture{};
    ctx.dispatcher.sink<game::defs::InventoryChanged>().connect<&InventoryChangedCapture::onEvent>(&capture);

    const entt::entity player = createPlayer(ctx);
    auto& inv = ctx.registry.get<game::component::InventoryComponent>(player);
    auto& hotbar = ctx.registry.get<game::component::HotbarComponent>(player);

    const entt::id_type item_id = entt::hashed_string{"merge_item_unref_target"}.value();
    inv.slot(0).item_id_ = item_id;
    inv.slot(0).count_ = 5;
    inv.slot(1).item_id_ = item_id;
    inv.slot(1).count_ = 5;
    hotbar.slot(0).inventory_slot_index_ = 0;

    ctx.dispatcher.trigger(game::defs::InventoryMoveCommand{player, 0, 1, true});

    EXPECT_TRUE(inv.slot(0).empty());
    EXPECT_EQ(inv.slot(1).count_, 10);
    EXPECT_EQ(hotbar.slot(0).inventory_slot_index_, 1);

    ASSERT_EQ(capture.events.size(), 1u);
    EXPECT_EQ(capture.events[0].move_kind, game::defs::InventoryMoveKind::Merge);
}

TEST(InventoryMoveHotbarFollowTest, MergePartial_SourceNotEmpty_KeepsMappingsUnchanged) {
    MoveTestContext ctx;
    InventoryChangedCapture capture{};
    ctx.dispatcher.sink<game::defs::InventoryChanged>().connect<&InventoryChangedCapture::onEvent>(&capture);

    const entt::entity player = createPlayer(ctx);
    auto& inv = ctx.registry.get<game::component::InventoryComponent>(player);
    auto& hotbar = ctx.registry.get<game::component::HotbarComponent>(player);

    const entt::id_type item_id = entt::hashed_string{"merge_item_partial"}.value();
    inv.slot(0).item_id_ = item_id;
    inv.slot(0).count_ = 5;
    inv.slot(1).item_id_ = item_id;
    inv.slot(1).count_ = 998; // stack_limit 默认为 999，触发部分合并
    hotbar.slot(0).inventory_slot_index_ = 0;
    hotbar.slot(1).inventory_slot_index_ = 1;

    ctx.dispatcher.trigger(game::defs::InventoryMoveCommand{player, 0, 1, true});

    EXPECT_EQ(inv.slot(0).item_id_, item_id);
    EXPECT_EQ(inv.slot(0).count_, 4);
    EXPECT_EQ(inv.slot(1).item_id_, item_id);
    EXPECT_EQ(inv.slot(1).count_, 999);
    EXPECT_EQ(hotbar.slot(0).inventory_slot_index_, 0);
    EXPECT_EQ(hotbar.slot(1).inventory_slot_index_, 1);

    ASSERT_EQ(capture.events.size(), 1u);
    EXPECT_EQ(capture.events[0].move_kind, game::defs::InventoryMoveKind::Merge);
}

} // namespace game::system
