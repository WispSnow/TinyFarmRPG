#include <gtest/gtest.h>

#include "game/system/farm_system.h"

#include "engine/component/tags.h"
#include "engine/component/transform_component.h"
#include "engine/spatial/spatial_index_manager.h"

#include "game/component/crop_component.h"
#include "game/component/inventory_component.h"
#include "game/component/tags.h"
#include "game/data/item_catalog.h"
#include "game/defs/constants.h"
#include "game/defs/events.h"
#include "game/defs/spatial_layers.h"
#include "game/factory/blueprint_manager.h"
#include "game/factory/entity_factory.h"

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <glm/vec2.hpp>

#include <string>
#include <vector>

using namespace entt::literals;

namespace {

constexpr glm::ivec2 MAP_SIZE(4, 4);
constexpr glm::ivec2 TILE_SIZE(16, 16);
const glm::vec2 WORLD_MIN(0.0f, 0.0f);
const glm::vec2 WORLD_MAX(64.0f, 64.0f);

glm::vec2 tileWorldPos(glm::ivec2 tile_coord) {
    return glm::vec2(tile_coord.x * TILE_SIZE.x, tile_coord.y * TILE_SIZE.y);
}

struct DialogueEvents {
    std::vector<game::defs::DialogueShowEvent> shows{};
    void onShow(const game::defs::DialogueShowEvent& evt) { shows.push_back(evt); }
};

} // namespace

namespace game::system {

TEST(FarmSystemHarvestInventoryFullTest, HarvestWhenInventoryFull_ShowsBubbleAndKeepsCrop) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    engine::spatial::SpatialIndexManager spatial;
    spatial.initialize(registry,
                       /*map_size*/ MAP_SIZE,
                       /*tile_size*/ TILE_SIZE,
                       /*world_bounds_min*/ WORLD_MIN,
                       /*world_bounds_max*/ WORLD_MAX,
                       /*dynamic_cell_size*/ glm::vec2(static_cast<float>(TILE_SIZE.x), static_cast<float>(TILE_SIZE.y)));

    factory::BlueprintManager blueprint_manager;
    ASSERT_TRUE(blueprint_manager.loadCropBlueprints(std::string(PROJECT_SOURCE_DIR) + "/assets/data/crop_config.json"));

    data::ItemCatalog item_catalog;
    ASSERT_TRUE(item_catalog.loadItemConfig(std::string(PROJECT_SOURCE_DIR) + "/assets/data/item_config.json"));

    factory::EntityFactory entity_factory(registry, blueprint_manager, &spatial, nullptr);
    FarmSystem farm(registry, dispatcher, spatial, entity_factory, blueprint_manager, &item_catalog);

    const entt::entity player = registry.create();
    registry.emplace<game::component::PlayerTag>(player);
    registry.emplace<engine::component::TransformComponent>(player, glm::vec2{0.0f, 0.0f});
    auto& inv = registry.emplace<game::component::InventoryComponent>(player);

    const entt::id_type strawberry_item = entt::hashed_string{"strawberry_item"}.value();
    for (auto& slot : inv.slots_) {
        slot.item_id_ = strawberry_item;
        slot.count_ = 999;
    }

    const glm::ivec2 crop_tile{1, 1};
    const glm::vec2 crop_world_pos = tileWorldPos(crop_tile);
    const entt::entity crop = registry.create();
    auto& crop_component = registry.emplace<game::component::CropComponent>(crop);
    crop_component.type_ = game::defs::CropType::Strawberry;
    crop_component.current_stage_ = game::defs::GrowthStage::Mature;
    spatial.addTileEntityAtWorldPos(crop_world_pos, crop, game::defs::spatial_layer::CROP);

    DialogueEvents dialogue{};
    dispatcher.sink<game::defs::DialogueShowEvent>().connect<&DialogueEvents::onShow>(&dialogue);

    dispatcher.trigger(game::defs::UseToolEvent{game::defs::Tool::Sickle, crop_world_pos});
    dispatcher.update();

    EXPECT_EQ(spatial.getTileEntityAtWorldPos(crop_world_pos, game::defs::spatial_layer::CROP), crop);
    EXPECT_FALSE(registry.all_of<engine::component::NeedRemoveTag>(crop));

    ASSERT_FALSE(dialogue.shows.empty());
    EXPECT_EQ(dialogue.shows.back().target, player);
    EXPECT_NE(dialogue.shows.back().text.find("背包"), std::string::npos);
}

} // namespace game::system
