#include "auto_tile_system.h"
#include "engine/component/auto_tile_component.h"
#include "engine/component/sprite_component.h"
#include "engine/component/tags.h"
#include "engine/component/transform_component.h"
#include "engine/resource/auto_tile_library.h"
#include "engine/spatial/spatial_index_manager.h"
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <array>
#include <cmath>
#include <spdlog/spdlog.h>

namespace engine::system {

namespace {
constexpr std::array<glm::ivec2, 8> NEIGHBOR_OFFSETS = {
    glm::ivec2(0, -1),  // 上
    glm::ivec2(1, -1),  // 右上
    glm::ivec2(1, 0),   // 右
    glm::ivec2(1, 1),   // 右下
    glm::ivec2(0, 1),   // 下
    glm::ivec2(-1, 1),  // 左下
    glm::ivec2(-1, 0),  // 左
    glm::ivec2(-1, -1)  // 左上
};
} // namespace

AutoTileSystem::AutoTileSystem(entt::registry& registry,
                               entt::dispatcher& dispatcher,
                               engine::resource::AutoTileLibrary& auto_tile_library,
                               engine::spatial::SpatialIndexManager& spatial_index)
    : registry_(registry),
      dispatcher_(dispatcher),
      auto_tile_library_(auto_tile_library),
      spatial_index_(spatial_index) {
    dispatcher_.sink<engine::utils::AddAutoTileEntityEvent>().connect<&AutoTileSystem::onAddAutoTileEntity>(this);
    dispatcher_.sink<engine::utils::RemoveAutoTileEntityEvent>().connect<&AutoTileSystem::onRemoveAutoTileEntity>(this);
}

AutoTileSystem::~AutoTileSystem() {
    dispatcher_.disconnect(this);
}

void AutoTileSystem::update() {
    auto view = registry_.view<
        engine::component::AutoTileComponent,
        engine::component::TransformComponent,
        engine::component::AutoTileDirtyTag
    >();

    for (auto entity : view) {
        auto& auto_tile = view.get<engine::component::AutoTileComponent>(entity);
        const auto& transform = view.get<engine::component::TransformComponent>(entity);
        const glm::ivec2 tile_coord = worldToTileCoord(transform.position_);
        refreshEntity(entity, tile_coord, auto_tile);
        registry_.remove<engine::component::AutoTileDirtyTag>(entity);
    }
}

void AutoTileSystem::refreshEntity(entt::entity entity,
                                   glm::ivec2 tile_coord,
                                   engine::component::AutoTileComponent& auto_tile) {
    if (!isValidCoord(tile_coord)) {
        return;
    }

    const uint8_t mask = computeMask(tile_coord, auto_tile.rule_id_);
    if (mask == auto_tile.current_mask_) {
        return;
    }

    auto_tile.current_mask_ = mask;
    auto* sprite = registry_.try_get<engine::component::SpriteComponent>(entity);
    if (!sprite) {
        spdlog::warn("AutoTileSystem: entity {} 缺少 SpriteComponent，无法更新图块。", static_cast<uint32_t>(entity));
        return;
    }

    if (const auto* rect = auto_tile_library_.getSrcRect(auto_tile.rule_id_, mask)) {
        sprite->sprite_.src_rect_ = *rect;
    } else {
        spdlog::warn("AutoTileSystem: 规则 {} 缺少 mask {} 的纹理定义。", auto_tile.rule_id_, mask);
    }
}

uint8_t AutoTileSystem::computeMask(glm::ivec2 tile_coord, entt::id_type rule_id) const {
    uint8_t mask = 0;
    for (std::size_t i = 0; i < NEIGHBOR_OFFSETS.size(); ++i) {
        const glm::ivec2 neighbor_coord = tile_coord + NEIGHBOR_OFFSETS[i];
        if (!isValidCoord(neighbor_coord)) {
            continue;
        }
        if (hasMatchingRule(neighbor_coord, rule_id)) {
            mask |= static_cast<uint8_t>(1u << i);
        }
    }
    return mask;
}

bool AutoTileSystem::hasMatchingRule(glm::ivec2 tile_coord, entt::id_type rule_id) const {
    const auto& entities = spatial_index_.getTileEntities(tile_coord);
    for (auto entity : entities) {
        if (const auto* auto_tile = registry_.try_get<engine::component::AutoTileComponent>(entity);
            auto_tile && (rule_id == entt::null || auto_tile->rule_id_ == rule_id)) {
            return true;
        }
    }
    return false;
}

glm::ivec2 AutoTileSystem::worldToTileCoord(glm::vec2 position) const {
    const auto& tile_size = spatial_index_.getStaticGrid().getTileSize();
    if (tile_size.x <= 0 || tile_size.y <= 0) {
        return {-1, -1};
    }
    return glm::ivec2(
        static_cast<int>(std::floor(position.x / static_cast<float>(tile_size.x))),
        static_cast<int>(std::floor(position.y / static_cast<float>(tile_size.y)))
    );
}

bool AutoTileSystem::isValidCoord(glm::ivec2 coord) const {
    const auto& map_size = spatial_index_.getStaticGrid().getMapSize();
    return coord.x >= 0 && coord.x < map_size.x &&
           coord.y >= 0 && coord.y < map_size.y;
}

void AutoTileSystem::markClusterDirty(glm::ivec2 tile_coord, entt::id_type rule_id, bool include_center) {
    if (include_center) {
        markTileDirty(tile_coord, rule_id);
    }
    for (const auto& offset : NEIGHBOR_OFFSETS) {
        markTileDirty(tile_coord + offset, rule_id);
    }
}

void AutoTileSystem::markTileDirty(glm::ivec2 tile_coord, entt::id_type rule_id) {
    if (!isValidCoord(tile_coord)) {
        return;
    }
    const auto& entities = spatial_index_.getTileEntities(tile_coord);
    for (auto entity : entities) {
        if (auto* auto_tile = registry_.try_get<engine::component::AutoTileComponent>(entity);
            auto_tile && (rule_id == entt::null || auto_tile->rule_id_ == rule_id)) {
            registry_.emplace_or_replace<engine::component::AutoTileDirtyTag>(entity);
        }
    }
}

void AutoTileSystem::onAddAutoTileEntity(const engine::utils::AddAutoTileEntityEvent& event) {
    spdlog::info("AutoTileSystem::onAddAutoTileEntity: 添加自动图块，世界位置: {}, {}", event.world_pos_.x, event.world_pos_.y);
    const glm::ivec2 coord = worldToTileCoord(event.world_pos_);
    markClusterDirty(coord, event.rule_id_, true);
}

void AutoTileSystem::onRemoveAutoTileEntity(const engine::utils::RemoveAutoTileEntityEvent& event) {
    spdlog::info("AutoTileSystem::onRemoveAutoTileEntity: 移除自动图块，世界位置: {}, {}", event.world_pos_.x, event.world_pos_.y);
    const glm::ivec2 coord = worldToTileCoord(event.world_pos_);
    markClusterDirty(coord, event.rule_id_, false);
}

} // namespace engine::system
