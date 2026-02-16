#include "farm_system.h"
#include "inventory_helpers.h"
#include "system_helpers.h"
#include "game/component/crop_component.h"
#include "game/component/inventory_component.h"
#include "game/component/resource_node_component.h"
#include "game/defs/crop_defs.h"
#include "game/defs/events.h"
#include "game/defs/constants.h"
#include "game/defs/spatial_layers.h"
#include "game/defs/render_layers.h"
#include "game/data/item_catalog.h"
#include "game/data/game_time.h"
#include "game/factory/entity_factory.h"
#include "game/factory/blueprint_manager.h"
#include "engine/utils/events.h"
#include "engine/utils/math.h"
#include "engine/spatial/spatial_index_manager.h"
#include "engine/component/auto_tile_component.h"
#include "engine/component/collider_component.h"
#include "engine/component/transform_component.h"
#include "engine/component/sprite_component.h"
#include "engine/component/render_component.h"
#include "engine/component/tilelayer_component.h"
#include "engine/component/tags.h"
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <entt/core/hashed_string.hpp>
#include <spdlog/spdlog.h>
#include <glm/vec2.hpp>
#include <algorithm>
#include <random>
#include <cmath>
#include <glm/gtc/constants.hpp>
#include <cstdint>

using namespace entt::literals;

namespace {
    constexpr entt::id_type RULE_SOIL_TILLED = game::defs::auto_tile_rule::SOIL_TILLED;
    constexpr entt::id_type RULE_SOIL_WET    = game::defs::auto_tile_rule::SOIL_WET;
    constexpr entt::id_type ANIM_AXE = "axe"_hs;
    constexpr entt::id_type ANIM_PICKAXE = "pickaxe"_hs;
    constexpr std::uint8_t NOTIFICATION_CHANNEL = 1;

    using game::system::detail::stackLimitOrDefault;

    bool isHoeBlockedByDynamicColliders(entt::registry& registry,
                                        engine::spatial::SpatialIndexManager& spatial_index,
                                        const glm::vec2& tile_world_pos) {
        const auto tile_rect = spatial_index.getRectAtWorldPos(tile_world_pos);
        for (auto entity : spatial_index.queryColliders(tile_rect)) {
            if (!registry.valid(entity)) {
                continue;
            }
            if (registry.any_of<engine::component::NeedRemoveTag>(entity)) {
                continue;
            }
            if (registry.any_of<engine::component::AABBCollider>(entity)) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] bool inventoryCanAdd(const game::component::InventoryComponent& inventory,
                                       entt::id_type item_id,
                                       int count,
                                       int stack_limit) {
        if (item_id == entt::null || count <= 0) return true;
        stack_limit = std::max(1, stack_limit);

        int remaining = count;
        for (int i = 0; i < inventory.slotCount() && remaining > 0; ++i) {
            const auto& slot = inventory.slot(i);
            if (slot.empty()) {
                remaining -= std::min(stack_limit, remaining);
                continue;
            }
            if (slot.item_id_ != item_id) continue;
            const int space = stack_limit - slot.count_;
            if (space > 0) {
                remaining -= std::min(space, remaining);
            }
        }
        return remaining <= 0;
    }

    [[nodiscard]] entt::entity findSoilOrWetAt(engine::spatial::SpatialIndexManager& spatial_index,
                                               const glm::vec2& world_pos) {
        entt::entity soil_or_wet = spatial_index.getTileEntityAtWorldPos(world_pos, game::defs::spatial_layer::WET);
        if (soil_or_wet == entt::null) {
            soil_or_wet = spatial_index.getTileEntityAtWorldPos(world_pos, game::defs::spatial_layer::SOIL);
        }
        return soil_or_wet;
    }

    [[nodiscard]] entt::entity findMatureCropAt(entt::registry& registry,
                                                engine::spatial::SpatialIndexManager& spatial_index,
                                                const glm::vec2& world_pos) {
        const entt::entity crop_entity =
            spatial_index.getTileEntityAtWorldPos(world_pos, game::defs::spatial_layer::CROP);
        if (crop_entity == entt::null) {
            return entt::null;
        }
        const auto* crop = registry.try_get<game::component::CropComponent>(crop_entity);
        if (!crop || crop->current_stage_ != game::defs::GrowthStage::Mature) {
            return entt::null;
        }
        return crop_entity;
    }
}

namespace game::system {

FarmSystem::FarmSystem(entt::registry& registry,
                       entt::dispatcher& dispatcher,
                       engine::spatial::SpatialIndexManager& spatial_index,
                       game::factory::EntityFactory& entity_factory,
                       const game::factory::BlueprintManager& blueprint_manager,
                       game::data::ItemCatalog* item_catalog)
    : registry_(registry),
      dispatcher_(dispatcher),
      spatial_index_(spatial_index),
      entity_factory_(entity_factory),
      blueprint_manager_(blueprint_manager),
      item_catalog_(item_catalog) {
    dispatcher_.sink<game::defs::UseToolEvent>().connect<&FarmSystem::onUseToolEvent>(this);
    dispatcher_.sink<game::defs::UseSeedEvent>().connect<&FarmSystem::onUseSeedEvent>(this);
}

FarmSystem::~FarmSystem() {
    dispatcher_.disconnect(this);
}

void FarmSystem::onUseToolEvent(const game::defs::UseToolEvent& event) {
    switch (event.tool_) {
        case game::defs::Tool::Hoe:
            // 添加耕地自动图块
            addTilledEntity(event.world_pos_);
            break;
        case game::defs::Tool::WateringCan:
            // 添加湿润水自动图块
            addWetEntity(event.world_pos_);
            break;
        case game::defs::Tool::Sickle:
            harvestCrop(event.world_pos_);
            break;
        case game::defs::Tool::Pickaxe:
            hitRock(event.world_pos_);
            break;
        case game::defs::Tool::Axe:
            hitTree(event.world_pos_);
            break;
        case game::defs::Tool::None:
            break;
    }
}

bool FarmSystem::addTilledEntity(const glm::vec2& world_pos) {
    spdlog::info("尝试添加耕地自动图块，世界位置: {}, {}", world_pos.x, world_pos.y);
    const auto tile_coord = spatial_index_.getTileCoordAtWorldPos(world_pos);
    if (!spatial_index_.isArableAt(tile_coord) || spatial_index_.isSolidAt(world_pos) || spatial_index_.isOccupiedAt(tile_coord)) {
        spdlog::info("区域不可耕作、被阻挡或被占用，跳过添加耕地");
        return false;
    }
    if (spatial_index_.getTileEntityAtWorldPos(world_pos, game::defs::spatial_layer::CROP) != entt::null) {
        spdlog::info("该位置已有作物，跳过添加耕地");
        return false;
    }
    if (spatial_index_.getTileEntityAtWorldPos(world_pos, game::defs::spatial_layer::SOIL) != entt::null) {
        spdlog::info("该位置已有耕地，跳过重复添加");
        return true;
    }
    if (isHoeBlockedByDynamicColliders(registry_, spatial_index_, world_pos)) {
        spdlog::info("该位置被动态碰撞体覆盖，无法锄地");
        return false;
    }

    const auto& entities = spatial_index_.getTileEntitiesAtWorldPos(world_pos);
    if (entities.empty()) {
        spdlog::info("区域地块没有实体，无法添加耕地自动图块");
        return false;
    }

    entt::entity base_entity = spatial_index_.getTileEntityAtWorldPos(world_pos, game::defs::spatial_layer::MAIN);
    if (base_entity == entt::null) {
        for (auto entity : entities) {
            if (registry_.all_of<engine::component::TransformComponent, engine::component::RenderComponent>(entity)) {
                base_entity = entity;
                break;
            }
        }
    }
    if (base_entity == entt::null) {
        spdlog::warn("区域地块实体缺少 Transform/Render，无法添加耕地");
        return false;
    }

    const auto* base_transform = registry_.try_get<engine::component::TransformComponent>(base_entity);
    const auto* base_render = registry_.try_get<engine::component::RenderComponent>(base_entity);
    if (!base_transform || !base_render) {
        spdlog::warn("基础实体缺少 Transform/Render，无法添加耕地");
        return false;
    }

    auto soil_entity =
        entity_factory_.createSoilTile(base_transform->position_, RULE_SOIL_TILLED, game::defs::spatial_layer::SOIL);
    if (soil_entity == entt::null) {
        spdlog::error("创建耕地实体失败");
        return false;
    }
    if (auto* render = registry_.try_get<engine::component::RenderComponent>(soil_entity)) {
        render->layer_ = game::defs::render_layer::SOIL;
        render->depth_ = base_render->depth_;
    }

    // 播放锄头音效
    dispatcher_.enqueue<engine::utils::PlaySoundEvent>(soil_entity, "hoe"_hs);
    // 标记自动图块刷新
    dispatcher_.enqueue<engine::utils::AddAutoTileEntityEvent>(RULE_SOIL_TILLED, world_pos);
    return true;
}

bool FarmSystem::addWetEntity(const glm::vec2& world_pos) {
    spdlog::info("尝试添加湿润耕地自动图块，世界位置: {}, {}", world_pos.x, world_pos.y);

    const auto soil_entity =
        spatial_index_.getTileEntityAtWorldPos(world_pos, game::defs::spatial_layer::SOIL);
    if (soil_entity == entt::null) {
        spdlog::info("无耕地层，无法添加湿润耕地");
        return false;
    }
    const auto* soil_transform = registry_.try_get<engine::component::TransformComponent>(soil_entity);
    const auto* soil_render = registry_.try_get<engine::component::RenderComponent>(soil_entity);
    if (!soil_transform || !soil_render) {
        spdlog::warn("耕地缺少 Transform/Render，无法添加湿润层");
        return false;
    }

    if (auto exist_wet = spatial_index_.getTileEntityAtWorldPos(world_pos, game::defs::spatial_layer::WET);
        exist_wet != entt::null) {
        spatial_index_.removeTileEntityAtWorldPos(world_pos, exist_wet);
        registry_.emplace<engine::component::NeedRemoveTag>(exist_wet);
        spdlog::info("移除旧的湿润耕地，准备添加新的");
    }

    auto wet_entity =
        entity_factory_.createSoilTile(soil_transform->position_, RULE_SOIL_WET, game::defs::spatial_layer::WET);
    if (wet_entity == entt::null) {
        spdlog::error("创建湿润耕地实体失败");
        return false;
    }
    if (auto* render = registry_.try_get<engine::component::RenderComponent>(wet_entity)) {
        render->layer_ = game::defs::render_layer::SOIL_WET;
        render->depth_ = soil_render->depth_;
    }

    dispatcher_.enqueue<engine::utils::PlaySoundEvent>(wet_entity, "watering"_hs);
    dispatcher_.enqueue<engine::utils::AddAutoTileEntityEvent>(RULE_SOIL_WET, world_pos);
    return true;
}

void FarmSystem::onUseSeedEvent(const game::defs::UseSeedEvent& event) {
    if (event.seed_type_ == game::defs::CropType::Unknown) {
        spdlog::warn("尝试种植未知类型的种子");
        return;
    }
    if (event.source == entt::null || !registry_.valid(event.source)) {
        spdlog::warn("UseSeedEvent 缺少有效的来源实体，种植被忽略");
        return;
    }
    if (!registry_.all_of<game::component::InventoryComponent>(event.source)) {
        spdlog::warn("来源实体缺少 InventoryComponent，无法扣减种子");
        return;
    }
    auto& inventory = registry_.get<game::component::InventoryComponent>(event.source);
    if (event.inventory_slot_index_ < 0 || event.inventory_slot_index_ >= inventory.slotCount()) {
        spdlog::warn("UseSeedEvent 的物品栏槽位无效: {}", event.inventory_slot_index_);
        return;
    }
    const auto& stack = inventory.slot(event.inventory_slot_index_);
    if (stack.empty() || stack.item_id_ != event.seed_item_id_) {
        spdlog::warn("激活槽没有匹配的种子，id: {}", static_cast<std::uint32_t>(event.seed_item_id_));
        return;
    }

    if (!plantSeed(event.world_pos_, event.seed_type_)) {
        return;
    }

    // 种植成功，扣减对应槽位的种子
    game::defs::RemoveItemRequest remove_evt{};
    remove_evt.target = event.source;
    remove_evt.item_id = event.seed_item_id_;
    remove_evt.count = 1;
    remove_evt.slot_index = event.inventory_slot_index_;
    dispatcher_.trigger(remove_evt);
}

bool FarmSystem::plantSeed(const glm::vec2& world_pos, game::defs::CropType seed_type) {
    spdlog::info("尝试种植种子，世界位置: {}, {}, 种子类型: {}", world_pos.x, world_pos.y, static_cast<int>(seed_type));
    
    if (spatial_index_.getTileEntityAtWorldPos(world_pos, game::defs::spatial_layer::CROP) != entt::null) {
        spdlog::info("该位置已有作物，无法种植");
        return false;
    }

    entt::entity soil_or_wet = findSoilOrWetAt(spatial_index_, world_pos);
    if (soil_or_wet == entt::null) {
        spdlog::info("区域没有耕地或湿润耕地，无法种植");
        return false;
    }

    // 获取当前游戏天数
    auto* game_time = registry_.ctx().find<game::data::GameTime>();
    if (!game_time) {
        spdlog::error("无法获取游戏时间，种植失败");
        return false;
    }
    std::uint32_t current_day = game_time->day_;
    
    // 计算作物实体的世界位置（格网的中心偏下一点）
    auto transform = registry_.get<engine::component::TransformComponent>(soil_or_wet);
    transform.position_ += game::defs::CROP_OFFSET;
    
    // 将作物类型转换为ID
    auto crop_type_id = game::defs::cropTypeToId(seed_type);
    if (crop_type_id == entt::null) {
        spdlog::error("未知的作物类型: {}", static_cast<int>(seed_type));
        return false;
    }
    
    // 使用EntityFactory创建作物实体
    auto crop_entity = entity_factory_.createCrop(crop_type_id, transform.position_, current_day);
    if (crop_entity == entt::null) {
        spdlog::error("创建作物实体失败");
        return false;
    }
    
    // 播放种植音效
    dispatcher_.enqueue<engine::utils::PlaySoundEvent>(crop_entity, "plant"_hs);
    
    spdlog::info("成功种植种子，实体ID: {}", static_cast<std::uint32_t>(crop_entity));
    return true;
}

bool FarmSystem::harvestCrop(const glm::vec2& world_pos) {
    spdlog::info("尝试收获作物，世界位置: {}, {}", world_pos.x, world_pos.y);
    
    const entt::entity crop_entity = findMatureCropAt(registry_, spatial_index_, world_pos);
    
    if (crop_entity == entt::null) {
        spdlog::info("该位置没有成熟作物，无法收获");
        return false;
    }
    
    // 获取作物组件和蓝图信息
    const auto& crop_component = registry_.get<game::component::CropComponent>(crop_entity);
    auto crop_type_id = game::defs::cropTypeToId(crop_component.type_);
    if (crop_type_id == entt::null) {
        spdlog::error("作物实体 {} 的作物类型无效: {}", 
                     static_cast<std::uint32_t>(crop_entity), 
                     static_cast<int>(crop_component.type_));
        return false;
    }
    
    if (!blueprint_manager_.hasCropBlueprint(crop_type_id)) {
        spdlog::error("未找到作物蓝图: {}", crop_type_id);
        return false;
    }
    
    const auto& crop_blueprint = blueprint_manager_.getCropBlueprint(crop_type_id);
    
    const entt::entity player = helpers::getPlayerEntity(registry_);
    if (player == entt::null || !registry_.all_of<game::component::InventoryComponent>(player)) {
        spdlog::warn("未找到玩家背包，收获被忽略");
        return false;
    }

    // 发放收获物到玩家背包；若背包已满则提示并保持作物不被移除
    if (const entt::id_type harvest_item_id = crop_blueprint.harvest_item_id_; harvest_item_id != entt::null) {
        const auto& inventory = registry_.get<game::component::InventoryComponent>(player);
        const int stack_limit = stackLimitOrDefault(item_catalog_, harvest_item_id);
        if (!inventoryCanAdd(inventory, harvest_item_id, 1, stack_limit)) {
            helpers::emitDialogueBubbleShow(dispatcher_,
                                           NOTIFICATION_CHANNEL,
                                           player,
                                           std::string{},
                                           "背包空间不足，无法收获",
                                           helpers::computeHeadPosition(registry_, player));
            return false;
        }

        dispatcher_.trigger(game::defs::AddItemRequest{player, harvest_item_id, 1});
    }
    
    // 播放收获音效
    dispatcher_.enqueue<engine::utils::PlaySoundEvent>(crop_entity, "harvest"_hs);
    
    // 从空间索引移除作物实体
    spatial_index_.removeTileEntityAtWorldPos(world_pos, crop_entity);
    
    // 标记实体删除
    registry_.emplace<engine::component::NeedRemoveTag>(crop_entity);

    // 移除湿润层（如存在）
    if (auto wet_entity = spatial_index_.getTileEntityAtWorldPos(world_pos, game::defs::spatial_layer::WET);
        wet_entity != entt::null) {
        spatial_index_.removeTileEntityAtWorldPos(world_pos, wet_entity);
        registry_.emplace<engine::component::NeedRemoveTag>(wet_entity);
    }
    
    spdlog::info("作物实体 {} 已移除，耕地状态已恢复", static_cast<std::uint32_t>(crop_entity));
    return true;
}

template <typename CleanupFn>
bool FarmSystem::hitResourceNode(entt::entity target,
                                 const glm::vec2& world_pos,
                                 entt::id_type sound_id,
                                 entt::id_type default_anim_id,
                                 std::string_view not_found_msg,
                                 std::string_view missing_component_msg,
                                 std::string_view destroyed_msg,
                                 CleanupFn cleanup) {
    if (target == entt::null) {
        spdlog::info("{}", not_found_msg);
        return false;
    }

    auto* node = registry_.try_get<game::component::ResourceNodeComponent>(target);
    if (!node) {
        spdlog::info("{}", missing_component_msg);
        return false;
    }

    dispatcher_.enqueue<engine::utils::PlaySoundEvent>(target, sound_id);
    const entt::id_type anim_id = node->hit_animation_id_ != entt::null ? node->hit_animation_id_ : default_anim_id;
    if (anim_id != entt::null) {
        dispatcher_.enqueue<engine::utils::PlayAnimationEvent>(target, anim_id, false);
    }

    node->hit_count_++;
    spdlog::info("命中计数: {}/{}", node->hit_count_, node->hits_to_break_);
    if (node->hit_count_ < node->hits_to_break_) {
        return true;
    }

    dropResource(*node, world_pos);

    cleanup(target, world_pos);

    registry_.emplace<engine::component::NeedRemoveTag>(target);
    spdlog::info("{}", destroyed_msg);
    return true;
}

bool FarmSystem::hitRock(const glm::vec2& world_pos) {
    // 查找石头实体：优先从 ROCK 空间层查询，回退到遍历瓦片实体
    entt::entity rock_entity =
        spatial_index_.getTileEntityAtWorldPos(world_pos, game::defs::spatial_layer::ROCK);
    if (rock_entity == entt::null) {
        const auto& entities = spatial_index_.getTileEntitiesAtWorldPos(world_pos);
        for (auto entity : entities) {
            const auto* node = registry_.try_get<game::component::ResourceNodeComponent>(entity);
            if (node && node->type_ == game::component::ResourceNodeType::Rock) {
                rock_entity = entity;
                break;
            }
        }
    }

    return hitResourceNode(
        rock_entity, world_pos,
        "pickaxe"_hs, ANIM_PICKAXE,
        "该位置没有石头可敲击",
        "石头实体缺少 ResourceNodeComponent，无法敲击",
        "石头已被敲碎并移除",
        [this](entt::entity entity, const glm::vec2& pos) {
            spatial_index_.removeTileEntityAtWorldPos(pos, entity);
            auto tile_coord = spatial_index_.getTileCoordAtWorldPos(pos);
            spatial_index_.clearTileType(tile_coord, engine::component::TileType::SOLID);
        });
}

bool FarmSystem::hitTree(const glm::vec2& world_pos) {
    // 查找树木实体：优先从动态碰撞网格查询，回退到静态网格瓦片实体
    entt::entity tree_entity = entt::null;

    const auto query_rect = spatial_index_.getRectAtWorldPos(world_pos);
    for (auto entity : spatial_index_.queryColliders(query_rect)) {
        const auto* node = registry_.try_get<game::component::ResourceNodeComponent>(entity);
        if (node && node->type_ == game::component::ResourceNodeType::Tree) {
            tree_entity = entity;
            break;
        }
    }

    if (tree_entity == entt::null) {
        const auto& entities = spatial_index_.getTileEntitiesAtWorldPos(world_pos);
        for (auto entity : entities) {
            const auto* node = registry_.try_get<game::component::ResourceNodeComponent>(entity);
            if (node && node->type_ == game::component::ResourceNodeType::Tree) {
                tree_entity = entity;
                break;
            }
        }
    }

    return hitResourceNode(
        tree_entity, world_pos,
        "axe"_hs, ANIM_AXE,
        "该位置没有树可砍伐",
        "树木实体缺少 ResourceNodeComponent，无法砍伐",
        "树木已被砍倒并移除",
        [this](entt::entity entity, const glm::vec2& pos) {
            spatial_index_.removeColliderEntity(entity);
            spatial_index_.removeTileEntityAtWorldPos(pos, entity);
            auto tile_coord = spatial_index_.getTileCoordAtWorldPos(pos);
            spatial_index_.clearTileType(tile_coord, engine::component::TileType::SOLID);
        });
}

void FarmSystem::dropResource(const game::component::ResourceNodeComponent& node, const glm::vec2& world_pos) {
    if (node.drop_item_id_ == entt::null) {
        return;
    }

    const int drop_count = engine::utils::randomInt(node.drop_min_, node.drop_max_);
    if (drop_count <= 0) {
        return;
    }

    const auto* item = item_catalog_ ? item_catalog_->findItem(node.drop_item_id_) : nullptr;
    if (!item) {
        return;
    }

    const auto& image = item_catalog_->getIconOrFallback(item->icon_id_);
    engine::component::Sprite sprite(image.getTexturePath(), image.getSourceRect());

    const glm::vec2 origin = world_pos + glm::vec2(game::defs::TILE_SIZE * 0.5f, game::defs::TILE_SIZE * 0.5f);
    constexpr float SCATTER_RADIUS = 16.0f;

    for (int i = 0; i < drop_count; ++i) {
        const float angle = engine::utils::randomFloat(0.0f, glm::two_pi<float>());
        const float radius = std::sqrt(engine::utils::randomFloat(0.0f, 1.0f)) * SCATTER_RADIUS;
        const glm::vec2 offset = glm::vec2(std::cos(angle), std::sin(angle)) * radius;

        const float flight_time = engine::utils::randomFloat(0.18f, 0.26f);
        const float arc_height = engine::utils::randomFloat(4.0f, 8.0f);

        entity_factory_.createResourcePickup(node.drop_item_id_, sprite, origin, origin + offset, flight_time, arc_height);
    }

    spdlog::info("生成资源掉落实体，物品: {}，数量: {}", static_cast<std::uint32_t>(node.drop_item_id_), drop_count);
}

} // namespace game::system
