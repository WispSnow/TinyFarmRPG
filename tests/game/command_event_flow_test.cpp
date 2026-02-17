#include <gtest/gtest.h>

#include "game/component/hotbar_component.h"
#include "game/component/inventory_component.h"
#include "game/data/item_catalog.h"
#include "game/defs/commands.h"
#include "game/defs/events.h"
#include "game/system/hotbar_system.h"
#include "game/system/inventory_system.h"
#include "game/system/item_use_system.h"

#include <entt/core/hashed_string.hpp>
#include <algorithm>
#include <string>
#include <vector>

namespace {

struct InventoryChangedCapture {
    std::vector<game::defs::InventoryChanged> events{};
    void onEvent(const game::defs::InventoryChanged& evt) { events.push_back(evt); }
};

struct HotbarChangedCapture {
    std::vector<game::defs::HotbarChanged> events{};
    void onEvent(const game::defs::HotbarChanged& evt) { events.push_back(evt); }
};

std::string testItemConfigPath() {
    return std::string(PROJECT_SOURCE_DIR) + "/tests/data/item_use_items.json";
}

bool containsInventorySlot(const game::defs::InventoryChanged& evt, int slot_index) {
    return std::any_of(evt.slots.begin(), evt.slots.end(), [slot_index](const auto& slot) {
        return slot.slot_index == slot_index;
    });
}

bool containsHotbarSlot(const game::defs::HotbarChanged& evt,
                        int hotbar_index,
                        entt::id_type item_id,
                        int count) {
    return std::any_of(evt.slots.begin(), evt.slots.end(), [=](const auto& slot) {
        return slot.hotbar_index == hotbar_index && slot.item_id == item_id && slot.count == count;
    });
}

} // namespace

namespace game::system {

TEST(CommandEventFlowTest, UseItemCommand_EmitsDomainEventsAndUpdatesHotbarView) {
    entt::registry registry;
    entt::dispatcher dispatcher;

    game::data::ItemCatalog catalog;
    ASSERT_TRUE(catalog.loadItemConfig(testItemConfigPath()));

    InventorySystem inventory_system(registry, dispatcher, catalog);
    HotbarSystem hotbar_system(registry, dispatcher);
    ItemUseSystem item_use_system(registry, dispatcher, catalog);

    const entt::entity player = registry.create();
    auto& inv = registry.emplace<game::component::InventoryComponent>(player);
    registry.emplace<game::component::HotbarComponent>(player);

    const entt::id_type crop_item_id = entt::hashed_string{"strawberry_item"}.value();
    const entt::id_type seed_item_id = entt::hashed_string{"strawberry_seed"}.value();
    inv.slot(0).item_id_ = crop_item_id;
    inv.slot(0).count_ = 1;

    InventoryChangedCapture inventory_capture{};
    HotbarChangedCapture hotbar_capture{};
    dispatcher.sink<game::defs::InventoryChanged>().connect<&InventoryChangedCapture::onEvent>(&inventory_capture);
    dispatcher.sink<game::defs::HotbarChanged>().connect<&HotbarChangedCapture::onEvent>(&hotbar_capture);

    dispatcher.trigger(game::defs::HotbarBindCommand{player, 0, 0});
    inventory_capture.events.clear();
    hotbar_capture.events.clear();

    dispatcher.trigger(game::defs::UseItemCommand{player, 0, 1, false});

    EXPECT_EQ(inv.slot(0).item_id_, seed_item_id);
    EXPECT_EQ(inv.slot(0).count_, 3);

    ASSERT_FALSE(inventory_capture.events.empty());
    EXPECT_TRUE(std::any_of(inventory_capture.events.begin(),
                            inventory_capture.events.end(),
                            [](const auto& evt) { return containsInventorySlot(evt, 0); }));

    ASSERT_FALSE(hotbar_capture.events.empty());
    EXPECT_TRUE(std::any_of(hotbar_capture.events.begin(),
                            hotbar_capture.events.end(),
                            [=](const auto& evt) { return containsHotbarSlot(evt, 0, seed_item_id, 3); }));
}

} // namespace game::system
