// NOLINTBEGIN
#include <gtest/gtest.h>

#include "appearance_test_fixture_utils.h"
#include "game/component/inventory_component.h"
#include "game/component/party_component.h"
#include "game/component/party_equipment_component.h"
#include "game/data/item_catalog.h"
#include "game/data/rpg_catalog.h"
#include "game/domain/equipment_domain_service.h"
#include "game/domain/inventory_domain_service.h"

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <filesystem>
#include <string>

namespace game::domain {
namespace {

struct FixturePaths {
    std::filesystem::path items{};
    std::filesystem::path classes{};
    std::filesystem::path actors{};
    std::filesystem::path equipment{};
};

[[nodiscard]] FixturePaths createEquipmentFixture() {
    const auto temp_root = game::test::createUniqueTempDir("equipment_domain_fixture");
    const auto data_root = temp_root / "rpg";
    std::filesystem::create_directories(data_root);

    game::test::writeTextFile(
        data_root / "items.json",
        R"json({
  "items": [
    { "id": "equip.alpha_sword", "display_name": "Alpha Sword", "category": "equipment", "stack_limit": 1 },
    { "id": "equip.beta_sword", "display_name": "Beta Sword", "category": "equipment", "stack_limit": 1 },
    { "id": "filler.stone", "display_name": "Stone", "category": "material", "stack_limit": 1 }
  ]
})json");

    game::test::writeTextFile(
        data_root / "classes.json",
        R"json({
  "classes": [
    { "id": "class.hero", "display_name": "Hero", "base_params": [100, 20, 10, 10, 10, 10, 10, 10] }
  ]
})json");

    game::test::writeTextFile(
        data_root / "actors.json",
        R"json({
  "actors": [
    { "id": "actor.hero", "display_name": "Hero", "class_id": "class.hero", "initial_level": 1, "max_level": 99 }
  ]
})json");

    game::test::writeTextFile(
        data_root / "equipment.json",
        R"json({
  "equipment": [
    { "item_id": "equip.alpha_sword", "slot": "weapon", "param_bonuses": { "atk": 2 } },
    { "item_id": "equip.beta_sword", "slot": "weapon", "param_bonuses": { "atk": 5 } }
  ]
})json");

    return FixturePaths{
        .items = data_root / "items.json",
        .classes = data_root / "classes.json",
        .actors = data_root / "actors.json",
        .equipment = data_root / "equipment.json",
    };
}

struct LoadedCatalogs {
    game::data::ItemCatalog items{};
    game::data::RpgCatalog rpg{};
};

[[nodiscard]] LoadedCatalogs loadCatalogs(const FixturePaths& paths) {
    LoadedCatalogs catalogs{};
    EXPECT_TRUE(catalogs.items.loadItemConfig(paths.items.string()));
    EXPECT_TRUE(catalogs.rpg.loadClasses(paths.classes.string()));
    EXPECT_TRUE(catalogs.rpg.loadActors(paths.actors.string()));
    EXPECT_TRUE(catalogs.rpg.loadEquipment(paths.equipment.string()));
    std::string error{};
    EXPECT_TRUE(catalogs.rpg.validateReferences(error, &catalogs.items)) << error;
    return catalogs;
}

[[nodiscard]] entt::entity createPlayer(entt::registry& registry) {
    const entt::entity player = registry.create();
    registry.emplace<game::component::InventoryComponent>(player);
    registry.emplace<game::component::PartyComponent>(
        player,
        game::component::PartyComponent{
            .recruited_actor_ids_ = {"actor.hero"},
            .active_actor_ids_ = {"actor.hero"},
            .max_active_members_ = 4,
        });
    return player;
}

} // namespace

TEST(EquipmentDomainServiceTest, EquipItemMovesInventoryItemIntoActorLoadout) {
    const auto paths = createEquipmentFixture();
    auto catalogs = loadCatalogs(paths);
    entt::registry registry;
    entt::dispatcher dispatcher;
    InventoryDomainService inventory_domain(registry, dispatcher, catalogs.items);
    EquipmentDomainService equipment_domain(registry, dispatcher, catalogs.rpg, catalogs.items, inventory_domain);

    const entt::entity player = createPlayer(registry);
    auto& inventory = registry.get<game::component::InventoryComponent>(player);
    inventory.slot(0).item_id_ = entt::hashed_string{"equip.alpha_sword"}.value();
    inventory.slot(0).count_ = 1;

    const auto result = equipment_domain.equipItem(
        player,
        "actor.hero",
        0,
        game::data::EquipmentSlotId::Weapon);

    ASSERT_TRUE(result.success) << result.message;
    EXPECT_TRUE(inventory.slot(0).empty());
    const auto& equipment = registry.get<game::component::PartyEquipmentComponent>(player);
    EXPECT_EQ(
        equipment.loadouts_by_actor_id_.at("actor.hero").equipped_item_ids_.at(game::data::EquipmentSlotId::Weapon),
        entt::hashed_string{"equip.alpha_sword"}.value());
}

TEST(EquipmentDomainServiceTest, ReplaceItemReturnsOldEquipmentToInventory) {
    const auto paths = createEquipmentFixture();
    auto catalogs = loadCatalogs(paths);
    entt::registry registry;
    entt::dispatcher dispatcher;
    InventoryDomainService inventory_domain(registry, dispatcher, catalogs.items);
    EquipmentDomainService equipment_domain(registry, dispatcher, catalogs.rpg, catalogs.items, inventory_domain);

    const entt::entity player = createPlayer(registry);
    auto& inventory = registry.get<game::component::InventoryComponent>(player);
    inventory.slot(0).item_id_ = entt::hashed_string{"equip.beta_sword"}.value();
    inventory.slot(0).count_ = 1;
    auto& equipment = registry.emplace<game::component::PartyEquipmentComponent>(player);
    equipment.loadouts_by_actor_id_["actor.hero"].equipped_item_ids_[game::data::EquipmentSlotId::Weapon] =
        entt::hashed_string{"equip.alpha_sword"}.value();

    const auto result = equipment_domain.equipItem(
        player,
        "actor.hero",
        0,
        game::data::EquipmentSlotId::Weapon);

    ASSERT_TRUE(result.success) << result.message;
    EXPECT_EQ(inventory.slot(0).item_id_, entt::hashed_string{"equip.alpha_sword"}.value());
    EXPECT_EQ(inventory.slot(0).count_, 1);
    EXPECT_EQ(
        equipment.loadouts_by_actor_id_.at("actor.hero").equipped_item_ids_.at(game::data::EquipmentSlotId::Weapon),
        entt::hashed_string{"equip.beta_sword"}.value());
}

TEST(EquipmentDomainServiceTest, UnequipItemReturnsItemAndClearsLoadout) {
    const auto paths = createEquipmentFixture();
    auto catalogs = loadCatalogs(paths);
    entt::registry registry;
    entt::dispatcher dispatcher;
    InventoryDomainService inventory_domain(registry, dispatcher, catalogs.items);
    EquipmentDomainService equipment_domain(registry, dispatcher, catalogs.rpg, catalogs.items, inventory_domain);

    const entt::entity player = createPlayer(registry);
    auto& inventory = registry.get<game::component::InventoryComponent>(player);
    auto& equipment = registry.emplace<game::component::PartyEquipmentComponent>(player);
    equipment.loadouts_by_actor_id_["actor.hero"].equipped_item_ids_[game::data::EquipmentSlotId::Weapon] =
        entt::hashed_string{"equip.alpha_sword"}.value();

    const auto result = equipment_domain.unequipItem(
        player,
        "actor.hero",
        game::data::EquipmentSlotId::Weapon,
        0);

    ASSERT_TRUE(result.success) << result.message;
    EXPECT_EQ(inventory.slot(0).item_id_, entt::hashed_string{"equip.alpha_sword"}.value());
    EXPECT_EQ(inventory.slot(0).count_, 1);
    EXPECT_FALSE(equipment.loadouts_by_actor_id_.at("actor.hero").equipped_item_ids_.contains(game::data::EquipmentSlotId::Weapon));
}

TEST(EquipmentDomainServiceTest, UnequipItemFailsWhenInventoryIsFullAndKeepsLoadout) {
    const auto paths = createEquipmentFixture();
    auto catalogs = loadCatalogs(paths);
    entt::registry registry;
    entt::dispatcher dispatcher;
    InventoryDomainService inventory_domain(registry, dispatcher, catalogs.items);
    EquipmentDomainService equipment_domain(registry, dispatcher, catalogs.rpg, catalogs.items, inventory_domain);

    const entt::entity player = createPlayer(registry);
    auto& inventory = registry.get<game::component::InventoryComponent>(player);
    for (int i = 0; i < inventory.slotCount(); ++i) {
        inventory.slot(i).item_id_ = entt::hashed_string{"filler.stone"}.value();
        inventory.slot(i).count_ = 1;
    }
    auto& equipment = registry.emplace<game::component::PartyEquipmentComponent>(player);
    equipment.loadouts_by_actor_id_["actor.hero"].equipped_item_ids_[game::data::EquipmentSlotId::Weapon] =
        entt::hashed_string{"equip.alpha_sword"}.value();

    const auto result = equipment_domain.unequipItem(
        player,
        "actor.hero",
        game::data::EquipmentSlotId::Weapon);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.message, "inventory is full");
    EXPECT_EQ(
        equipment.loadouts_by_actor_id_.at("actor.hero").equipped_item_ids_.at(game::data::EquipmentSlotId::Weapon),
        entt::hashed_string{"equip.alpha_sword"}.value());
}

} // namespace game::domain
// NOLINTEND
