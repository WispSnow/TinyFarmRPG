#include "spatial_index_manager.h"
#include "collider_shape.h"
#include "engine/component/transform_component.h"
#include "engine/component/collider_component.h"
#include "engine/component/tags.h"
#include "engine/utils/math.h"
#include <glm/common.hpp>
#include <optional>
#include <spdlog/spdlog.h>

namespace {

void upsertDynamicEntity(engine::spatial::DynamicEntityGrid& grid,
                         entt::entity entity,
                         const engine::spatial::ColliderShape& shape) {
    if (shape.type == engine::spatial::ColliderShapeType::Rect) {
        grid.addEntity(entity, shape.rect);
        return;
    }
    grid.addEntity(entity, shape.center, shape.radius);
}

template <typename IntersectFn>
[[nodiscard]] std::vector<entt::entity> filterOverlaps(const entt::registry* registry,
                                                       std::vector<entt::entity> candidates,
                                                       IntersectFn&& intersect_fn,
                                                       bool fallback_to_candidates,
                                                       const char* warn_context) {
    if (!registry) {
        if (warn_context) {
            spdlog::warn("{} called without registry; returning empty", warn_context);
        }
        return fallback_to_candidates ? std::move(candidates) : std::vector<entt::entity>{};
    }

    std::vector<entt::entity> overlaps;
    overlaps.reserve(candidates.size());
    for (auto candidate : candidates) {
        auto shape = engine::spatial::buildColliderShape(*registry, candidate);
        if (shape && intersect_fn(*shape)) {
            overlaps.push_back(candidate);
        }
    }
    return overlaps;
}

} // namespace

namespace engine::spatial {

void SpatialIndexManager::initialize(entt::registry& registry, 
                                      glm::ivec2 map_size, 
                                      glm::ivec2 tile_size,
                                      glm::vec2 world_bounds_min,
                                      glm::vec2 world_bounds_max,
                                      glm::vec2 dynamic_cell_size) {
    registry_ = &registry;
    
    // 初始化静态网格
    static_grid_.initialize(map_size, tile_size);
    
    // 初始化动态网格
    dynamic_grid_.initialize(world_bounds_min, world_bounds_max, dynamic_cell_size);
}

void SpatialIndexManager::setTileType(glm::ivec2 tile_coord, engine::component::TileType type) {
    static_grid_.setTileType(tile_coord, type);
}

void SpatialIndexManager::clearTileType(glm::ivec2 tile_coord, engine::component::TileType mask) {
    static_grid_.clearTileType(tile_coord, mask);
}

bool SpatialIndexManager::isBlockedNorthAt(glm::ivec2 tile_coord) const {
    return static_grid_.isBlockedNorth(tile_coord);
}

bool SpatialIndexManager::isBlockedSouthAt(glm::ivec2 tile_coord) const {
    return static_grid_.isBlockedSouth(tile_coord);
}

bool SpatialIndexManager::isBlockedWestAt(glm::ivec2 tile_coord) const {
    return static_grid_.isBlockedWest(tile_coord);
}

bool SpatialIndexManager::isBlockedEastAt(glm::ivec2 tile_coord) const {
    return static_grid_.isBlockedEast(tile_coord);
}

bool SpatialIndexManager::hasHazardAt(glm::ivec2 tile_coord) const {
    return static_grid_.hasHazard(tile_coord);
}

bool SpatialIndexManager::hasWaterAt(glm::ivec2 tile_coord) const {
    return static_grid_.hasWater(tile_coord);
}

bool SpatialIndexManager::isInteractableAt(glm::ivec2 tile_coord) const {
    return static_grid_.isInteractable(tile_coord);
}

bool SpatialIndexManager::isArableAt(glm::ivec2 tile_coord) const {
    return static_grid_.isArable(tile_coord);
}

bool SpatialIndexManager::isOccupiedAt(glm::ivec2 tile_coord) const {
    return static_grid_.isOccupied(tile_coord);
}

bool SpatialIndexManager::isThinWallBlockedBetween(glm::ivec2 from_tile, glm::ivec2 to_tile) const {
    return static_grid_.isThinWallBlockedBetween(from_tile, to_tile);
}

void SpatialIndexManager::addTileEntity(glm::ivec2 tile_coord, entt::entity entity, entt::id_type layer_id) {
    static_grid_.addEntity(tile_coord, entity, layer_id);
}

void SpatialIndexManager::addTileEntityAtWorldPos(glm::vec2 world_pos, entt::entity entity, entt::id_type layer_id) {
    static_grid_.addEntityAtWorldPos(world_pos, entity, layer_id);
}

void SpatialIndexManager::removeTileEntity(glm::ivec2 tile_coord, entt::entity entity) {
    static_grid_.removeEntity(tile_coord, entity);
}

void SpatialIndexManager::removeTileEntityAtWorldPos(glm::vec2 world_pos, entt::entity entity) {
    static_grid_.removeEntityAtWorldPos(world_pos, entity);
}

bool SpatialIndexManager::isSolidAt(glm::vec2 world_pos) const {
    return static_grid_.isSolidAtWorldPos(world_pos);
}

bool SpatialIndexManager::hasSolidInRect(const engine::utils::Rect& rect) const {
    return static_grid_.hasSolidInRect(rect);
}

bool SpatialIndexManager::hasBlockedDirectionInRect(const engine::utils::Rect& rect, engine::component::TileType direction_flag) const {
    return static_grid_.hasBlockedDirectionInRect(rect, direction_flag);
}

const std::vector<entt::entity>& SpatialIndexManager::getTileEntities(glm::ivec2 tile_coord) const {
    return static_grid_.getEntities(tile_coord);
}

const std::vector<entt::entity>& SpatialIndexManager::getTileEntitiesAtWorldPos(glm::vec2 world_pos) const {
    return static_grid_.getEntitiesAtWorldPos(world_pos);
}

entt::entity SpatialIndexManager::getTileEntity(glm::ivec2 tile_coord, entt::id_type layer_id) const {
    return static_grid_.getEntity(tile_coord, layer_id);
}

entt::entity SpatialIndexManager::getTileEntityAtWorldPos(glm::vec2 world_pos, entt::id_type layer_id) const {
    return static_grid_.getEntityAtWorldPos(world_pos, layer_id);
}

glm::ivec2 SpatialIndexManager::getTileCoordAtWorldPos(glm::vec2 world_pos) const {
    return static_grid_.getTileCoordAtWorldPos(world_pos);
}

engine::utils::Rect SpatialIndexManager::getRectAtWorldPos(glm::vec2 world_pos) const {
    return static_grid_.getRectAtWorldPos(world_pos);
}

void SpatialIndexManager::addColliderEntity(entt::entity entity) {
    updateColliderEntity(entity);
}

void SpatialIndexManager::removeColliderEntity(entt::entity entity) {
    dynamic_grid_.removeEntity(entity);
}

void SpatialIndexManager::updateColliderEntity(entt::entity entity) {
    if (!registry_ || !registry_->valid(entity)) {
        return;
    }

    auto shape = buildColliderShape(*registry_, entity);
    if (!shape) {
        dynamic_grid_.removeEntity(entity);
        return;
    }

    upsertDynamicEntity(dynamic_grid_, entity, *shape);
}

std::vector<entt::entity> SpatialIndexManager::queryColliderCandidates(const engine::utils::Rect& rect) const {
    return dynamic_grid_.queryEntities(rect);
}

std::vector<entt::entity> SpatialIndexManager::queryColliderCandidates(const glm::vec2& center, float radius) const {
    return dynamic_grid_.queryEntities(center, radius);
}

std::vector<entt::entity> SpatialIndexManager::queryColliderCandidatesAt(glm::vec2 world_pos) const {
    return dynamic_grid_.queryEntitiesAt(world_pos);
}

std::vector<entt::entity> SpatialIndexManager::queryColliders(const engine::utils::Rect& rect) const {
    auto candidates = dynamic_grid_.queryEntities(rect);
    return filterOverlaps(registry_, std::move(candidates),
        [&rect](const auto& shape) { return intersectsRectQuery(rect, shape); },
        false, "SpatialIndexManager::queryColliders(rect)");
}

std::vector<entt::entity> SpatialIndexManager::queryColliders(const glm::vec2& center, float radius) const {
    auto candidates = dynamic_grid_.queryEntities(center, radius);
    return filterOverlaps(registry_, std::move(candidates),
        [&center, radius](const auto& shape) { return intersectsCircleQuery(center, radius, shape); },
        false, "SpatialIndexManager::queryColliders(circle)");
}

std::vector<entt::entity> SpatialIndexManager::queryCollidersAt(glm::vec2 world_pos) const {
    auto candidates = dynamic_grid_.queryEntitiesAt(world_pos);
    return filterOverlaps(registry_, std::move(candidates),
        [&world_pos](const auto& shape) { return intersectsPointQuery(world_pos, shape); },
        false, "SpatialIndexManager::queryCollidersAt(point)");
}

CollisionResult SpatialIndexManager::checkCollision(const engine::utils::Rect& rect) const {
    CollisionResult result;
    result.has_static_collision = static_grid_.hasSolidInRect(rect);

    auto candidates = dynamic_grid_.queryEntities(rect);
    result.dynamic_colliders = filterOverlaps(registry_, std::move(candidates),
        [&rect](const auto& shape) { return intersectsRectQuery(rect, shape); },
        true, nullptr);

    return result;
}

CollisionResult SpatialIndexManager::checkCollision(const glm::vec2& center, float radius) const {
    CollisionResult result;
    // 对于圆形，静态碰撞检测需要转换为矩形（简化处理）
    engine::utils::Rect rect(
        center - glm::vec2(radius),
        glm::vec2(radius * 2.0f)
    );
    result.has_static_collision = static_grid_.hasSolidInRect(rect);

    auto candidates = dynamic_grid_.queryEntities(center, radius);
    result.dynamic_colliders = filterOverlaps(registry_, std::move(candidates),
        [&center, radius](const auto& shape) { return intersectsCircleQuery(center, radius, shape); },
        true, nullptr);

    return result;
}

SweepResult SpatialIndexManager::resolveStaticSweep(const engine::utils::Rect& start_rect,
                                                    const engine::utils::Rect& target_rect,
                                                    Direction direction) const {
    SweepResult result;
    result.rect = target_rect;
    result.hit_info = {};

    const bool is_vertical = (direction == Direction::North || direction == Direction::South);
    const bool positive_dir = (direction == Direction::South || direction == Direction::East);

    SweepHitInfo info{};
    auto [hit, resolved_pos] = is_vertical
        ? static_grid_.sweepVertical(start_rect, target_rect, positive_dir, &info)
        : static_grid_.sweepHorizontal(start_rect, target_rect, positive_dir, &info);

    if (hit) {
        result.hit_solid = info.hit_solid;
        result.hit_thin_wall = !info.hit_solid;
        result.hit_info = info;
        (is_vertical ? result.rect.pos.y : result.rect.pos.x) = resolved_pos;
    }

    return result;
}

void SpatialIndexManager::updateAllDynamicEntities() {
    if (!registry_) return;
    
    // 遍历所有拥有SpatialIndexTag的实体
    auto view = registry_->view<engine::component::SpatialIndexTag>();
    for (auto entity : view) {
        updateColliderEntity(entity);
    }
}

CollisionResult SpatialIndexManager::checkDirectionalCollision(const engine::utils::Rect& rect) const {
    CollisionResult result;

    // 检查静态方向性碰撞
    result.has_static_collision = static_grid_.hasSolidInRect(rect);
    result.blocked_north = static_grid_.hasBlockedDirectionInRect(rect, engine::component::TileType::BLOCK_N);
    result.blocked_south = static_grid_.hasBlockedDirectionInRect(rect, engine::component::TileType::BLOCK_S);
    result.blocked_west = static_grid_.hasBlockedDirectionInRect(rect, engine::component::TileType::BLOCK_W);
    result.blocked_east = static_grid_.hasBlockedDirectionInRect(rect, engine::component::TileType::BLOCK_E);
    result.has_hazard = static_grid_.hasBlockedDirectionInRect(rect, engine::component::TileType::HAZARD);
    result.has_water = static_grid_.hasBlockedDirectionInRect(rect, engine::component::TileType::WATER);

    // 检查动态碰撞（复用原有逻辑）
    auto candidates = dynamic_grid_.queryEntities(rect);
    result.dynamic_colliders = filterOverlaps(registry_, std::move(candidates),
        [&rect](const auto& shape) { return intersectsRectQuery(rect, shape); },
        true, nullptr);

    return result;
}

} // namespace engine::spatial
