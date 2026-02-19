#include <gtest/gtest.h>

#include "engine/component/transform_component.h"
#include "game/component/inventory_component.h"
#include "game/component/tags.h"
#include "game/data/item_catalog.h"
#include "game/domain/inventory_domain_service.h"
#include "game/script/script_host.h"
#include "game/system/inventory_system.h"

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <filesystem>
#include <string>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace {

[[nodiscard]] std::string itemConfigPath() {
    return (std::filesystem::path{PROJECT_SOURCE_DIR} / "tests/data/item_use_items.json").string();
}

[[nodiscard]] std::string commandScriptPath() {
    return (std::filesystem::path{PROJECT_SOURCE_DIR} / "tests/data/scripts/test_command.lua").string();
}

} // namespace

namespace game::script {

TEST(ScriptHostCommandBridgeTest, ScriptCanEmitCommandAndProduceDomainEffect) {
    entt::registry registry;
    entt::dispatcher dispatcher;

    game::data::ItemCatalog catalog;
    ASSERT_TRUE(catalog.loadItemConfig(itemConfigPath()));
    game::domain::InventoryDomainService inventory_domain_service(registry, dispatcher, catalog);
    game::system::InventorySystem inventory_system(registry, dispatcher, catalog, inventory_domain_service);

    const entt::entity player = registry.create();
    registry.emplace<game::component::PlayerTag>(player);
    registry.emplace<engine::component::TransformComponent>(player, glm::vec2{0.0f, 0.0f});
    registry.emplace<game::component::InventoryComponent>(player);

    ScriptHost host(registry, dispatcher);
    ASSERT_TRUE(host.init());
    ASSERT_TRUE(host.loadFile(commandScriptPath()));
    ASSERT_TRUE(host.exec("assert(issue_add_item('strawberry_seed', 2))"));

    const auto& inventory = registry.get<game::component::InventoryComponent>(player);
    const entt::id_type seed_item_id = entt::hashed_string{"strawberry_seed"}.value();
    int total_seed_count = 0;
    for (int i = 0; i < inventory.slotCount(); ++i) {
        const auto& slot = inventory.slot(i);
        if (slot.item_id_ == seed_item_id) {
            total_seed_count += slot.count_;
        }
    }

    EXPECT_EQ(total_seed_count, 2);
}

} // namespace game::script
