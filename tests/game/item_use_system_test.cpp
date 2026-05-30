#include <gtest/gtest.h>

#include "game/component/inventory_component.h"
#include "game/component/party_component.h"
#include "game/component/party_runtime_stats_component.h"
#include "game/data/item_catalog.h"
#include "game/data/rpg_catalog.h"
#include "game/defs/commands.h"
#include "game/defs/events.h"
#include "game/domain/inventory_domain_service.h"
#include "game/runtime/localization_service.h"
#include "game/system/inventory_system.h"
#include "game/system/item_use_system.h"

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <filesystem>
#include <string_view>

namespace {

struct DialogueCapture {
    std::vector<game::defs::DialogueShowEvent> shows{};
    void onShow(const game::defs::DialogueShowEvent& evt) { shows.push_back(evt); }
};

std::string testItemConfigPath() {
    return std::string(PROJECT_SOURCE_DIR) + "/tests/data/item_use_items.json";
}

std::string projectItemConfigPath() {
    return std::string(PROJECT_SOURCE_DIR) + "/assets/data/item_config.json";
}

game::data::RpgCatalog loadProjectActorCatalog() {
    const std::string rpg_root = std::string(PROJECT_SOURCE_DIR) + "/assets/data/rpg";
    game::data::RpgCatalog catalog;
    EXPECT_TRUE(catalog.loadClasses(rpg_root + "/classes.json"));
    EXPECT_TRUE(catalog.loadActors(rpg_root + "/actors.json"));
    return catalog;
}

game::runtime::LocalizationService loadLocalization(const std::string_view language_tag) {
    game::runtime::LocalizationService localization;
    const auto manifest_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "assets/i18n/languages.json").lexically_normal();
    EXPECT_TRUE(localization.loadLanguageIndex(manifest_path.string()));
    EXPECT_TRUE(localization.setLanguage(language_tag));
    return localization;
}

} // namespace

namespace game::system {

TEST(ItemUseSystemTest, UseCropItem_CountOne_ReplacesWithSeeds) {
    entt::registry registry;
    entt::dispatcher dispatcher;

    game::data::ItemCatalog catalog;
    ASSERT_TRUE(catalog.loadItemConfig(testItemConfigPath()));
    game::domain::InventoryDomainService inventory_domain_service(registry, dispatcher, catalog);

    InventorySystem inventory_system(registry, dispatcher, inventory_domain_service);
    ItemUseSystem item_use_system(registry, dispatcher, catalog, inventory_domain_service);

    const entt::entity player = registry.create();
    auto& inv = registry.emplace<game::component::InventoryComponent>(player);
    inv.slot(0).item_id_ = entt::hashed_string{"strawberry_item"}.value();
    inv.slot(0).count_ = 1;

    dispatcher.trigger(game::defs::UseItemCommand{player, 0, 1, false});

    EXPECT_EQ(inv.slot(0).item_id_, entt::hashed_string{"strawberry_seed"}.value());
    EXPECT_EQ(inv.slot(0).count_, 3);
}

TEST(ItemUseSystemTest, UseCropItem_CountTwo_NoSpace_DoesNotConsume) {
    entt::registry registry;
    entt::dispatcher dispatcher;

    game::data::ItemCatalog catalog;
    ASSERT_TRUE(catalog.loadItemConfig(testItemConfigPath()));
    game::domain::InventoryDomainService inventory_domain_service(registry, dispatcher, catalog);

    InventorySystem inventory_system(registry, dispatcher, inventory_domain_service);
    ItemUseSystem item_use_system(registry, dispatcher, catalog, inventory_domain_service);

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

    dispatcher.trigger(game::defs::UseItemCommand{player, 0, 1, false});

    EXPECT_EQ(inv.slot(0).item_id_, entt::hashed_string{"strawberry_item"}.value());
    EXPECT_EQ(inv.slot(0).count_, 2);
    EXPECT_EQ(inv.slot(1).item_id_, dummy_id);
    EXPECT_EQ(inv.slot(1).count_, 1);
}

TEST(ItemUseSystemTest, UseCropItem_CountTwo_NoSpaceLocalizesPrompt) {
    entt::registry registry;
    entt::dispatcher dispatcher;

    game::data::ItemCatalog catalog;
    ASSERT_TRUE(catalog.loadItemConfig(testItemConfigPath()));
    auto localization = loadLocalization("zh-Hans");
    registry.ctx().emplace<game::runtime::LocalizationService*>(&localization);
    game::domain::InventoryDomainService inventory_domain_service(registry, dispatcher, catalog);

    InventorySystem inventory_system(registry, dispatcher, inventory_domain_service);
    ItemUseSystem item_use_system(registry, dispatcher, catalog, inventory_domain_service);

    DialogueCapture capture{};
    dispatcher.sink<game::defs::DialogueShowEvent>().connect<&DialogueCapture::onShow>(&capture);

    const entt::entity player = registry.create();
    auto& inv = registry.emplace<game::component::InventoryComponent>(player);

    const entt::id_type dummy_id = entt::hashed_string{"dummy_full"}.value();
    for (int i = 0; i < inv.slotCount(); ++i) {
        inv.slot(i).item_id_ = dummy_id;
        inv.slot(i).count_ = 1;
    }

    inv.slot(0).item_id_ = entt::hashed_string{"strawberry_item"}.value();
    inv.slot(0).count_ = 2;

    dispatcher.trigger(game::defs::UseItemCommand{player, 0, 1, true});
    dispatcher.update();

    ASSERT_EQ(capture.shows.size(), 1u);
    EXPECT_EQ(capture.shows[0].text, localization.tr("inventory.full"));
}

TEST(ItemUseSystemTest, UseCropItem_CountOne_FullInventoryStillSucceeds) {
    entt::registry registry;
    entt::dispatcher dispatcher;

    game::data::ItemCatalog catalog;
    ASSERT_TRUE(catalog.loadItemConfig(testItemConfigPath()));
    game::domain::InventoryDomainService inventory_domain_service(registry, dispatcher, catalog);

    InventorySystem inventory_system(registry, dispatcher, inventory_domain_service);
    ItemUseSystem item_use_system(registry, dispatcher, catalog, inventory_domain_service);

    const entt::entity player = registry.create();
    auto& inv = registry.emplace<game::component::InventoryComponent>(player);

    const entt::id_type dummy_id = entt::hashed_string{"dummy_full"}.value();
    for (int i = 0; i < inv.slotCount(); ++i) {
        inv.slot(i).item_id_ = dummy_id;
        inv.slot(i).count_ = 1;
    }

    inv.slot(0).item_id_ = entt::hashed_string{"strawberry_item"}.value();
    inv.slot(0).count_ = 1; // consuming frees the slot for seeds

    dispatcher.trigger(game::defs::UseItemCommand{player, 0, 1, false});

    EXPECT_EQ(inv.slot(0).item_id_, entt::hashed_string{"strawberry_seed"}.value());
    EXPECT_EQ(inv.slot(0).count_, 3);
}

TEST(ItemUseSystemTest, UseCropItem_ShowPrompt_EnqueuesNotification) {
    entt::registry registry;
    entt::dispatcher dispatcher;

    game::data::ItemCatalog catalog;
    ASSERT_TRUE(catalog.loadItemConfig(testItemConfigPath()));
    game::domain::InventoryDomainService inventory_domain_service(registry, dispatcher, catalog);

    InventorySystem inventory_system(registry, dispatcher, inventory_domain_service);
    ItemUseSystem item_use_system(registry, dispatcher, catalog, inventory_domain_service);

    DialogueCapture capture{};
    dispatcher.sink<game::defs::DialogueShowEvent>().connect<&DialogueCapture::onShow>(&capture);

    const entt::entity player = registry.create();
    auto& inv = registry.emplace<game::component::InventoryComponent>(player);
    inv.slot(0).item_id_ = entt::hashed_string{"potato_item"}.value();
    inv.slot(0).count_ = 1;

    dispatcher.trigger(game::defs::UseItemCommand{player, 0, 1, true});
    dispatcher.update();

    ASSERT_EQ(capture.shows.size(), 1u);
    EXPECT_EQ(capture.shows[0].channel, game::defs::DialogueChannel::ItemNotice);
}

TEST(ItemUseSystemTest, UseProjectCatalogKeyLocalizesPromptNotification) {
    entt::registry registry;
    entt::dispatcher dispatcher;

    game::data::ItemCatalog catalog;
    ASSERT_TRUE(catalog.loadItemConfig(projectItemConfigPath()));
    auto localization = loadLocalization("zh-Hans");
    registry.ctx().emplace<game::runtime::LocalizationService*>(&localization);
    game::domain::InventoryDomainService inventory_domain_service(registry, dispatcher, catalog);

    InventorySystem inventory_system(registry, dispatcher, inventory_domain_service);
    ItemUseSystem item_use_system(registry, dispatcher, catalog, inventory_domain_service);

    DialogueCapture capture{};
    dispatcher.sink<game::defs::DialogueShowEvent>().connect<&DialogueCapture::onShow>(&capture);

    const entt::entity player = registry.create();
    auto& inv = registry.emplace<game::component::InventoryComponent>(player);
    inv.slot(0).item_id_ = entt::hashed_string{"strawberry_item"}.value();
    inv.slot(0).count_ = 1;

    dispatcher.trigger(game::defs::UseItemCommand{player, 0, 1, true});

    ASSERT_EQ(capture.shows.size(), 1u);
    const auto* seed = catalog.findItem(entt::hashed_string{"strawberry_seed"}.value());
    ASSERT_NE(seed, nullptr);
    EXPECT_EQ(capture.shows[0].text,
              localization.format(
                  "reward.item_gained",
                  {{"item", localization.tr(seed->display_name_)}, {"count", "3"}}));
}

TEST(ItemUseSystemTest, UseBattleItemOnActor_RecoversFromExistingRuntimeHp) {
    entt::registry registry;
    entt::dispatcher dispatcher;

    game::data::ItemCatalog catalog;
    ASSERT_TRUE(catalog.loadItemConfig(projectItemConfigPath()));
    auto rpg_catalog = loadProjectActorCatalog();
    game::domain::InventoryDomainService inventory_domain_service(registry, dispatcher, catalog);

    InventorySystem inventory_system(registry, dispatcher, inventory_domain_service);
    ItemUseSystem item_use_system(registry, dispatcher, catalog, inventory_domain_service, &rpg_catalog);

    const entt::entity player = registry.create();
    auto& inv = registry.emplace<game::component::InventoryComponent>(player);
    inv.slot(0).item_id_ = entt::hashed_string{"potion"}.value();
    inv.slot(0).count_ = 2;

    registry.emplace<game::component::PartyComponent>(
        player,
        game::component::PartyComponent{
            .recruited_actor_ids_ = {"actor.player"},
            .active_actor_ids_ = {"actor.player"},
            .max_active_members_ = 4,
        });
    auto& runtime_stats = registry.emplace<game::component::PartyRuntimeStatsComponent>(player);
    runtime_stats.states_by_actor_id_["actor.player"] = game::component::ActorRuntimeState{
        .current_hp = 0,
        .current_mp = 10,
    };

    dispatcher.trigger(game::defs::UseItemCommand{
        .target = player,
        .inventory_slot_index = 0,
        .count = 1,
        .show_prompt = false,
        .actor_target_id = "actor.player",
    });

    EXPECT_EQ(inv.slot(0).item_id_, entt::hashed_string{"potion"}.value());
    EXPECT_EQ(inv.slot(0).count_, 1);
    EXPECT_EQ(runtime_stats.states_by_actor_id_.at("actor.player").current_hp, 50);
    EXPECT_EQ(runtime_stats.states_by_actor_id_.at("actor.player").current_mp, 10);
}

} // namespace game::system
