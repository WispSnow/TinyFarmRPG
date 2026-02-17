#include <gtest/gtest.h>

#include "game/component/inventory_component.h"
#include "game/data/item_catalog.h"
#include "game/defs/events.h"
#include "game/domain/inventory_domain_service.h"

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <algorithm>
#include <vector>

namespace {

struct InventoryChangedCapture {
    std::vector<game::defs::InventoryChanged> events{};
    void onEvent(const game::defs::InventoryChanged& evt) { events.push_back(evt); }
};

struct InventoryFullCapture {
    std::vector<game::defs::InventoryFullEvent> events{};
    void onEvent(const game::defs::InventoryFullEvent& evt) { events.push_back(evt); }
};

[[nodiscard]] bool hasSlotUpdate(const game::defs::InventoryChanged& evt, int slot_index, entt::id_type item_id, int count) {
    return std::any_of(evt.slots.begin(), evt.slots.end(), [=](const auto& update) {
        return update.slot_index == slot_index && update.item_id == item_id && update.count == count;
    });
}

} // namespace

namespace game::domain {

TEST(InventoryDomainServiceTest, AddItemPartial_EmitsChangedAndFullEvent) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    game::data::ItemCatalog catalog;
    InventoryDomainService service(registry, dispatcher, catalog);

    const entt::entity player = registry.create();
    auto& inventory = registry.emplace<game::component::InventoryComponent>(player);

    const entt::id_type item_id = entt::hashed_string{"apple_item"}.value();
    const entt::id_type filler_id = entt::hashed_string{"full_slot"}.value();

    for (int i = 0; i < inventory.slotCount(); ++i) {
        inventory.slot(i).item_id_ = filler_id;
        inventory.slot(i).count_ = 999;
    }
    inventory.slot(0).item_id_ = item_id;
    inventory.slot(0).count_ = 998;

    InventoryChangedCapture changed_capture{};
    InventoryFullCapture full_capture{};
    dispatcher.sink<game::defs::InventoryChanged>().connect<&InventoryChangedCapture::onEvent>(&changed_capture);
    dispatcher.sink<game::defs::InventoryFullEvent>().connect<&InventoryFullCapture::onEvent>(&full_capture);

    const auto result = service.addItem(player, item_id, 3);

    EXPECT_EQ(result.accepted, 1);
    EXPECT_EQ(result.rejected, 2);
    EXPECT_EQ(inventory.slot(0).item_id_, item_id);
    EXPECT_EQ(inventory.slot(0).count_, 999);

    ASSERT_EQ(changed_capture.events.size(), 1u);
    EXPECT_TRUE(changed_capture.events[0].from_add);
    EXPECT_TRUE(hasSlotUpdate(changed_capture.events[0], 0, item_id, 999));

    ASSERT_EQ(full_capture.events.size(), 1u);
    EXPECT_EQ(full_capture.events[0].target, player);
    EXPECT_EQ(full_capture.events[0].item_id, item_id);
    EXPECT_EQ(full_capture.events[0].rejected, 2);
}

TEST(InventoryDomainServiceTest, RemoveItemFromSpecificSlot_OnlyUpdatesThatSlot) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    game::data::ItemCatalog catalog;
    InventoryDomainService service(registry, dispatcher, catalog);

    const entt::entity player = registry.create();
    auto& inventory = registry.emplace<game::component::InventoryComponent>(player);

    const entt::id_type item_id = entt::hashed_string{"apple_item"}.value();
    inventory.slot(0).item_id_ = item_id;
    inventory.slot(0).count_ = 5;
    inventory.slot(1).item_id_ = item_id;
    inventory.slot(1).count_ = 2;

    InventoryChangedCapture changed_capture{};
    dispatcher.sink<game::defs::InventoryChanged>().connect<&InventoryChangedCapture::onEvent>(&changed_capture);

    const auto result = service.removeItem(player, item_id, 3, 0);

    EXPECT_EQ(result.accepted, 3);
    EXPECT_EQ(result.rejected, 0);
    EXPECT_EQ(inventory.slot(0).item_id_, item_id);
    EXPECT_EQ(inventory.slot(0).count_, 2);
    EXPECT_EQ(inventory.slot(1).item_id_, item_id);
    EXPECT_EQ(inventory.slot(1).count_, 2);

    ASSERT_EQ(changed_capture.events.size(), 1u);
    EXPECT_FALSE(changed_capture.events[0].from_add);
    ASSERT_EQ(changed_capture.events[0].slots.size(), 1u);
    EXPECT_TRUE(hasSlotUpdate(changed_capture.events[0], 0, item_id, 2));
}

TEST(InventoryDomainServiceTest, AddItemCreatesInventoryWhenMissing) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    game::data::ItemCatalog catalog;
    InventoryDomainService service(registry, dispatcher, catalog);

    const entt::entity player = registry.create();
    const entt::id_type item_id = entt::hashed_string{"apple_item"}.value();

    InventoryChangedCapture changed_capture{};
    dispatcher.sink<game::defs::InventoryChanged>().connect<&InventoryChangedCapture::onEvent>(&changed_capture);

    const auto result = service.addItem(player, item_id, 2);

    ASSERT_TRUE(registry.all_of<game::component::InventoryComponent>(player));
    const auto& inventory = registry.get<game::component::InventoryComponent>(player);
    EXPECT_EQ(result.accepted, 2);
    EXPECT_EQ(result.rejected, 0);
    EXPECT_EQ(inventory.slot(0).item_id_, item_id);
    EXPECT_EQ(inventory.slot(0).count_, 2);

    ASSERT_EQ(changed_capture.events.size(), 1u);
    EXPECT_TRUE(changed_capture.events[0].from_add);
    EXPECT_TRUE(hasSlotUpdate(changed_capture.events[0], 0, item_id, 2));
}

} // namespace game::domain
