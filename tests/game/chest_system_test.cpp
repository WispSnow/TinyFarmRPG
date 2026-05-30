// NOLINTBEGIN
#include <gtest/gtest.h>

#include "engine/component/transform_component.h"
#include "game/component/chest_component.h"
#include "game/component/inventory_component.h"
#include "game/component/map_component.h"
#include "game/component/tags.h"
#include "game/data/item_catalog.h"
#include "game/defs/commands.h"
#include "game/defs/events.h"
#include "game/domain/inventory_domain_service.h"
#include "game/runtime/localization_service.h"
#include "game/system/chest_system.h"
#include "game/system/inventory_system.h"
#include "game/world/world_state.h"

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <glm/vec2.hpp>

#include <filesystem>
#include <vector>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace game::system {
namespace {

struct DialogueCapture {
    std::vector<game::defs::DialogueShowEvent> shows{};

    void onShow(const game::defs::DialogueShowEvent& event) {
        shows.push_back(event);
    }
};

[[nodiscard]] game::runtime::LocalizationService loadLocalization(const std::string_view language_tag) {
    game::runtime::LocalizationService localization;
    const auto manifest_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "assets/i18n/languages.json").lexically_normal();
    EXPECT_TRUE(localization.loadLanguageIndex(manifest_path.string()));
    EXPECT_TRUE(localization.setLanguage(language_tag));
    return localization;
}

} // namespace

TEST(ChestSystemTest, LootNotificationLocalizesProjectCatalogItemKeys) {
    entt::registry registry;
    entt::dispatcher dispatcher;

    game::data::ItemCatalog catalog;
    ASSERT_TRUE(catalog.loadItemConfig(
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "assets/data/item_config.json").lexically_normal().string()));
    auto localization = loadLocalization("zh-Hans");
    registry.ctx().emplace<game::runtime::LocalizationService*>(&localization);

    game::domain::InventoryDomainService inventory_domain_service(registry, dispatcher, catalog);
    InventorySystem inventory_system(registry, dispatcher, inventory_domain_service);
    game::world::WorldState world_state;
    ChestSystem chest_system(registry, dispatcher, world_state, catalog, inventory_domain_service);

    const entt::entity player = registry.create();
    registry.emplace<game::component::PlayerTag>(player);
    registry.emplace<engine::component::TransformComponent>(player, glm::vec2{0.0F, 0.0F});
    registry.emplace<game::component::InventoryComponent>(player);

    const entt::id_type map_id = world_state.ensureExternalMap("test_map");
    world_state.setCurrentMap(map_id);

    const entt::entity chest = registry.create();
    registry.emplace<engine::component::TransformComponent>(chest, glm::vec2{16.0F, 0.0F});
    registry.emplace<game::component::MapId>(chest, map_id);
    registry.emplace<game::component::ChestComponent>(
        chest,
        game::component::ChestComponent{
            .chest_id_ = 7,
            .rewards_ = {
                game::component::ItemReward{
                    .item_id_ = entt::hashed_string{"tool_hoe"}.value(),
                    .count_ = 1,
                },
            },
        });

    DialogueCapture capture{};
    dispatcher.sink<game::defs::DialogueShowEvent>().connect<&DialogueCapture::onShow>(&capture);

    dispatcher.trigger(game::defs::InteractCommand{player, chest});

    ASSERT_EQ(capture.shows.size(), 1U);
    EXPECT_EQ(capture.shows.front().text, "锄头 x1");
    EXPECT_EQ(capture.shows.front().channel, game::defs::DialogueChannel::Notice);

    dispatcher.disconnect(&capture);
}

} // namespace game::system
// NOLINTEND
