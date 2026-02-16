#include "static_tile_grid.h"
#include <algorithm>
#include <cmath>
#include <spdlog/spdlog.h>

namespace engine::spatial {

// ==================== TileCellData 实现 ====================

bool TileCellData::hasEntity(entt::entity entity) const {
    return std::find(entities_.begin(), entities_.end(), entity) != entities_.end();
}

bool TileCellData::hasLayer(entt::id_type layer_id) const {
    return layer_to_index_.find(layer_id) != layer_to_index_.end();
}

entt::entity TileCellData::getEntity(entt::id_type layer_id) const {
    auto it = layer_to_index_.find(layer_id);
    if (it == layer_to_index_.end()) {
        return entt::null;
    }
    return entities_.at(it->second);
}

void TileCellData::addEntity(entt::entity entity, entt::id_type layer_id) {
    if (auto it = layer_to_index_.find(layer_id); it != layer_to_index_.end()) {
        entities_[it->second] = entity;
        return;
    }
    const std::size_t index = entities_.size();
    layer_to_index_[layer_id] = index;
    layers_.push_back(layer_id);
    entities_.push_back(entity);
}

void TileCellData::removeAtIndex(std::size_t index) {
    if (index >= entities_.size()) {
        return;
    }
    auto layer = layers_[index];
    layer_to_index_.erase(layer);
    entities_.erase(entities_.begin() + static_cast<long>(index));
    layers_.erase(layers_.begin() + static_cast<long>(index));
    for (std::size_t i = index; i < layers_.size(); ++i) {
        layer_to_index_[layers_[i]] = i;
    }
}

void TileCellData::removeEntity(entt::entity entity) {
    auto it = std::find(entities_.begin(), entities_.end(), entity);
    if (it != entities_.end()) {
        const auto index = static_cast<std::size_t>(std::distance(entities_.begin(), it));
        removeAtIndex(index);
    }
}

void TileCellData::removeEntityByLayer(entt::id_type layer_id) {
    auto it = layer_to_index_.find(layer_id);
    if (it == layer_to_index_.end()) {
        return;
    }
    removeAtIndex(it->second);
}

void TileCellData::clearEntities() {
    entities_.clear();
    layers_.clear();
    layer_to_index_.clear();
}

// ==================== StaticTileGrid 实现 ====================

void StaticTileGrid::initialize(glm::ivec2 map_size, glm::ivec2 tile_size) {
    map_size_ = glm::ivec2(std::max(0, map_size.x), std::max(0, map_size.y));
    tile_size_ = glm::ivec2(std::max(1, tile_size.x), std::max(1, tile_size.y));
    cells_.clear();
    cells_.resize(map_size_.x * map_size_.y);
    spdlog::info("初始化静态瓦片网格: {}x{} 瓦片, 单元格大小: {}x{}", 
                 map_size_.x, map_size_.y, tile_size_.x, tile_size_.y);
}

int StaticTileGrid::coordToIndex(glm::ivec2 tile_coord) const {
    return tile_coord.y * map_size_.x + tile_coord.x;
}

glm::ivec2 StaticTileGrid::indexToCoord(int tile_index) const {
    return glm::ivec2(tile_index % map_size_.x, tile_index / map_size_.x);
}

glm::ivec2 StaticTileGrid::worldToTileCoord(glm::vec2 world_pos) const {
    return glm::ivec2(
        static_cast<int>(std::floor(world_pos.x / static_cast<float>(tile_size_.x))),
        static_cast<int>(std::floor(world_pos.y / static_cast<float>(tile_size_.y)))
    );
}

bool StaticTileGrid::isValidCoord(glm::ivec2 tile_coord) const {
    return tile_coord.x >= 0 && tile_coord.x < map_size_.x &&
           tile_coord.y >= 0 && tile_coord.y < map_size_.y;
}

std::pair<glm::ivec2, glm::ivec2> StaticTileGrid::tileRangeForRect(const engine::utils::Rect& rect) const {
    glm::ivec2 min_coord = worldToTileCoord(rect.pos);
    glm::vec2 max_for_coord = glm::max(rect.pos + rect.size - glm::vec2(EPSILON), rect.pos);
    glm::ivec2 max_coord = worldToTileCoord(max_for_coord);

    min_coord = glm::clamp(min_coord, glm::ivec2(0), map_size_ - glm::ivec2(1));
    max_coord = glm::clamp(max_coord, glm::ivec2(0), map_size_ - glm::ivec2(1));

    return {min_coord, max_coord};
}

void StaticTileGrid::setTileType(glm::ivec2 tile_coord, engine::component::TileType type) {
    if (!isValidCoord(tile_coord)) return;
    cells_[coordToIndex(tile_coord)].tile_type_ |= type;
}

void StaticTileGrid::clearTileType(glm::ivec2 tile_coord, engine::component::TileType mask) {
    if (!isValidCoord(tile_coord)) return;
    cells_[coordToIndex(tile_coord)].tile_type_ &= ~mask;
}

void StaticTileGrid::addEntity(glm::ivec2 tile_coord, entt::entity entity, entt::id_type layer_id) {
    if (!isValidCoord(tile_coord)) return;
    cells_[coordToIndex(tile_coord)].addEntity(entity, layer_id);
}

void StaticTileGrid::addEntityAtWorldPos(glm::vec2 world_pos, entt::entity entity, entt::id_type layer_id) {
    addEntity(worldToTileCoord(world_pos), entity, layer_id);
}

void StaticTileGrid::removeEntity(glm::ivec2 tile_coord, entt::entity entity) {
    if (!isValidCoord(tile_coord)) return;
    cells_[coordToIndex(tile_coord)].removeEntity(entity);
}

void StaticTileGrid::removeEntityAtWorldPos(glm::vec2 world_pos, entt::entity entity) {
    removeEntity(worldToTileCoord(world_pos), entity);
}

void StaticTileGrid::removeEntityFromAll(entt::entity entity) {
    for (auto& cell : cells_) {
        cell.removeEntity(entity);
    }
}

entt::entity StaticTileGrid::getEntity(glm::ivec2 tile_coord, entt::id_type layer_id) const {
    if (!isValidCoord(tile_coord)) return entt::null;
    return cells_[coordToIndex(tile_coord)].getEntity(layer_id);
}

entt::entity StaticTileGrid::getEntityAtWorldPos(glm::vec2 world_pos, entt::id_type layer_id) const {
    return getEntity(worldToTileCoord(world_pos), layer_id);
}

bool StaticTileGrid::isSolid(glm::ivec2 tile_coord) const {
    if (!isValidCoord(tile_coord)) return false;
    auto tile_type = cells_[coordToIndex(tile_coord)].tile_type_;
    return engine::component::hasAllTileFlags(tile_type, engine::component::TileType::SOLID); // 只有四向都阻挡才视为SOLID
}

bool StaticTileGrid::isSolid(int tile_index) const {
    if (tile_index < 0 || tile_index >= static_cast<int>(cells_.size())) return false;
    auto tile_type = cells_[tile_index].tile_type_;
    return engine::component::hasAllTileFlags(tile_type, engine::component::TileType::SOLID); // 只有四向都阻挡才视为SOLID
}

bool StaticTileGrid::isSolidAtWorldPos(glm::vec2 world_pos) const {
    return isSolid(worldToTileCoord(world_pos));
}

bool StaticTileGrid::isBlockedNorth(glm::ivec2 tile_coord) const {
    if (!isValidCoord(tile_coord)) return false;
    auto tile_type = cells_[coordToIndex(tile_coord)].tile_type_;
    return engine::component::hasTileFlag(tile_type, engine::component::TileType::BLOCK_N);
}

bool StaticTileGrid::isBlockedSouth(glm::ivec2 tile_coord) const {
    if (!isValidCoord(tile_coord)) return false;
    auto tile_type = cells_[coordToIndex(tile_coord)].tile_type_;
    return engine::component::hasTileFlag(tile_type, engine::component::TileType::BLOCK_S);
}

bool StaticTileGrid::isBlockedWest(glm::ivec2 tile_coord) const {
    if (!isValidCoord(tile_coord)) return false;
    auto tile_type = cells_[coordToIndex(tile_coord)].tile_type_;
    return engine::component::hasTileFlag(tile_type, engine::component::TileType::BLOCK_W);
}

bool StaticTileGrid::isBlockedEast(glm::ivec2 tile_coord) const {
    if (!isValidCoord(tile_coord)) return false;
    auto tile_type = cells_[coordToIndex(tile_coord)].tile_type_;
    return engine::component::hasTileFlag(tile_type, engine::component::TileType::BLOCK_E);
}

bool StaticTileGrid::hasHazard(glm::ivec2 tile_coord) const {
    if (!isValidCoord(tile_coord)) return false;
    auto tile_type = cells_[coordToIndex(tile_coord)].tile_type_;
    return engine::component::hasTileFlag(tile_type, engine::component::TileType::HAZARD);
}

bool StaticTileGrid::hasWater(glm::ivec2 tile_coord) const {
    if (!isValidCoord(tile_coord)) return false;
    auto tile_type = cells_[coordToIndex(tile_coord)].tile_type_;
    return engine::component::hasTileFlag(tile_type, engine::component::TileType::WATER);
}

bool StaticTileGrid::isInteractable(glm::ivec2 tile_coord) const {
    if (!isValidCoord(tile_coord)) return false;
    auto tile_type = cells_[coordToIndex(tile_coord)].tile_type_;
    return engine::component::hasTileFlag(tile_type, engine::component::TileType::INTERACT);
}

bool StaticTileGrid::isArable(glm::ivec2 tile_coord) const {
    if (!isValidCoord(tile_coord)) return false;
    auto tile_type = cells_[coordToIndex(tile_coord)].tile_type_;
    return engine::component::hasTileFlag(tile_type, engine::component::TileType::ARABLE);
}

bool StaticTileGrid::isOccupied(glm::ivec2 tile_coord) const {
    if (!isValidCoord(tile_coord)) return false;
    auto tile_type = cells_[coordToIndex(tile_coord)].tile_type_;
    return engine::component::hasTileFlag(tile_type, engine::component::TileType::OCCUPIED);
}

bool StaticTileGrid::isThinWallBlockedBetween(glm::ivec2 from_tile, glm::ivec2 to_tile) const {
    if (!isValidCoord(from_tile) || !isValidCoord(to_tile)) {
        return false;
    }

    const glm::ivec2 delta = to_tile - from_tile;
    const auto from_type = getTileType(from_tile);
    const auto to_type = getTileType(to_tile);
    const auto has_flag = [](engine::component::TileType type, engine::component::TileType flag) {
        return engine::component::hasTileFlag(type, flag);
    };

    if (delta.x == 1 && delta.y == 0) { // east
        return has_flag(from_type, engine::component::TileType::BLOCK_E) ||
               has_flag(to_type, engine::component::TileType::BLOCK_W);
    }
    if (delta.x == -1 && delta.y == 0) { // west
        return has_flag(from_type, engine::component::TileType::BLOCK_W) ||
               has_flag(to_type, engine::component::TileType::BLOCK_E);
    }
    if (delta.x == 0 && delta.y == 1) { // south (screen down)
        return has_flag(from_type, engine::component::TileType::BLOCK_S) ||
               has_flag(to_type, engine::component::TileType::BLOCK_N);
    }
    if (delta.x == 0 && delta.y == -1) { // north (screen up)
        return has_flag(from_type, engine::component::TileType::BLOCK_N) ||
               has_flag(to_type, engine::component::TileType::BLOCK_S);
    }

    return false;
}

engine::component::TileType StaticTileGrid::getTileType(glm::ivec2 tile_coord) const {
    if (!isValidCoord(tile_coord)) return engine::component::TileType::NORMAL;
    return cells_[coordToIndex(tile_coord)].tile_type_;
}

const std::vector<entt::entity>& StaticTileGrid::getEntities(glm::ivec2 tile_coord) const {
    static const std::vector<entt::entity> empty;
    if (!isValidCoord(tile_coord)) return empty;
    return cells_[coordToIndex(tile_coord)].entities_;
}

const std::vector<entt::entity>& StaticTileGrid::getEntitiesAtWorldPos(glm::vec2 world_pos) const {
    return getEntities(worldToTileCoord(world_pos));
}

const TileCellData& StaticTileGrid::getCellData(glm::ivec2 tile_coord) const {
    static const TileCellData empty;
    if (!isValidCoord(tile_coord)) return empty;
    return cells_[coordToIndex(tile_coord)];
}

bool StaticTileGrid::hasSolidInRect(const engine::utils::Rect& rect) const {
    if (!isInitialized()) {
        return false;
    }
    const auto [min_coord, max_coord] = tileRangeForRect(rect);

    for (int y = min_coord.y; y <= max_coord.y; ++y) {
        for (int x = min_coord.x; x <= max_coord.x; ++x) {
            if (isSolid(glm::ivec2(x, y))) {
                return true;
            }
        }
    }
    return false;
}

std::vector<glm::ivec2> StaticTileGrid::getSolidTilesInRect(const engine::utils::Rect& rect) const {
    std::vector<glm::ivec2> result;
    if (!isInitialized()) {
        return result;
    }
    const auto [min_coord, max_coord] = tileRangeForRect(rect);

    for (int y = min_coord.y; y <= max_coord.y; ++y) {
        for (int x = min_coord.x; x <= max_coord.x; ++x) {
            glm::ivec2 coord(x, y);
            if (isSolid(coord)) {
                result.push_back(coord);
            }
        }
    }
    return result;
}

bool StaticTileGrid::hasBlockedDirectionInRect(const engine::utils::Rect& rect, engine::component::TileType direction_flag) const {
    if (!isInitialized()) {
        return false;
    }
    const auto [min_coord, max_coord] = tileRangeForRect(rect);

    for (int y = min_coord.y; y <= max_coord.y; ++y) {
        for (int x = min_coord.x; x <= max_coord.x; ++x) {
            if (engine::component::hasTileFlag(getTileType({x, y}), direction_flag)) {
                return true;
            }
        }
    }
    return false;
}

std::vector<glm::ivec2> StaticTileGrid::getTilesWithFlagInRect(const engine::utils::Rect& rect, engine::component::TileType flag) const {
    std::vector<glm::ivec2> result;
    if (!isInitialized()) {
        return result;
    }
    const auto [min_coord, max_coord] = tileRangeForRect(rect);

    for (int y = min_coord.y; y <= max_coord.y; ++y) {
        for (int x = min_coord.x; x <= max_coord.x; ++x) {
            glm::ivec2 coord(x, y);
            if (engine::component::hasTileFlag(getTileType(coord), flag)) {
                result.push_back(coord);
            }
        }
    }
    return result;
}

glm::ivec2 StaticTileGrid::getTileCoordAtWorldPos(glm::vec2 world_pos) const {
    return worldToTileCoord(world_pos);
}

engine::utils::Rect StaticTileGrid::getRectAtWorldPos(glm::vec2 world_pos) const {
    glm::ivec2 tile_coord = getTileCoordAtWorldPos(world_pos);
    return engine::utils::Rect(glm::vec2(tile_coord.x * tile_size_.x, tile_coord.y * tile_size_.y), tile_size_);
}

StaticTileGrid::EdgeBlockInfo StaticTileGrid::verticalEdgeBlockInfo(int boundary_col, int row_start, int row_end) const {
    const int col_left = boundary_col - 1;
    const int col_right = boundary_col;

    EdgeBlockInfo info{};

    for (int row = row_start; row <= row_end; ++row) {
        if (row < 0 || row >= map_size_.y) {
            continue;
        }

        bool blocked = false;
        if (col_left >= 0 && col_left < map_size_.x) {
            const auto tile_type = getTileType(glm::ivec2(col_left, row));
            blocked |= engine::component::hasTileFlag(tile_type, engine::component::TileType::BLOCK_E);
            info.has_solid |= engine::component::hasAllTileFlags(tile_type, engine::component::TileType::SOLID);
        }

        if (col_right >= 0 && col_right < map_size_.x) {
            const auto tile_type = getTileType(glm::ivec2(col_right, row));
            blocked |= engine::component::hasTileFlag(tile_type, engine::component::TileType::BLOCK_W);
            info.has_solid |= engine::component::hasAllTileFlags(tile_type, engine::component::TileType::SOLID);
        }

        if (blocked) {
            info.blocked = true;
            return info;
        }
    }

    return info;
}

StaticTileGrid::EdgeBlockInfo StaticTileGrid::horizontalEdgeBlockInfo(int boundary_row, int col_start, int col_end) const {
    const int row_above = boundary_row - 1;
    const int row_below = boundary_row;

    EdgeBlockInfo info{};

    for (int col = col_start; col <= col_end; ++col) {
        if (col < 0 || col >= map_size_.x) {
            continue;
        }

        bool blocked = false;
        if (row_below >= 0 && row_below < map_size_.y) {
            const auto tile_type = getTileType(glm::ivec2(col, row_below));
            blocked |= engine::component::hasTileFlag(tile_type, engine::component::TileType::BLOCK_N);
            info.has_solid |= engine::component::hasAllTileFlags(tile_type, engine::component::TileType::SOLID);
        }

        if (row_above >= 0 && row_above < map_size_.y) {
            const auto tile_type = getTileType(glm::ivec2(col, row_above));
            blocked |= engine::component::hasTileFlag(tile_type, engine::component::TileType::BLOCK_S);
            info.has_solid |= engine::component::hasAllTileFlags(tile_type, engine::component::TileType::SOLID);
        }

        if (blocked) {
            info.blocked = true;
            return info;
        }
    }

    return info;
}

std::pair<bool, float> StaticTileGrid::sweepVertical(const engine::utils::Rect& start_rect,
                                                     const engine::utils::Rect& end_rect,
                                                     bool moving_south,
                                                     SweepHitInfo* hit_info) const {
    const float start_edge = moving_south ? start_rect.pos.y + start_rect.size.y : start_rect.pos.y;
    const float end_edge = moving_south ? end_rect.pos.y + end_rect.size.y : end_rect.pos.y;

    if (std::abs(end_edge - start_edge) < EPSILON) {
        if (hit_info) {
            hit_info->boundary_row = -1;
            hit_info->boundary_col = -1;
            hit_info->row_start = -1;
            hit_info->row_end = -1;
            hit_info->col_start = -1;
            hit_info->col_end = -1;
            hit_info->hit_solid = false;
        }
        return {false, end_rect.pos.y};
    }

    const float min_x = std::min(start_rect.pos.x, end_rect.pos.x);
    const float max_x = std::max(start_rect.pos.x + start_rect.size.x, end_rect.pos.x + end_rect.size.x) - EPSILON;
    const int col_start = static_cast<int>(std::floor(min_x / static_cast<float>(tile_size_.x)));
    const int col_end = static_cast<int>(std::floor(max_x / static_cast<float>(tile_size_.x)));

    if (hit_info) {
        hit_info->boundary_row = -1;
        hit_info->boundary_col = -1;
        hit_info->row_start = -1;
        hit_info->row_end = -1;
        hit_info->col_start = col_start;
        hit_info->col_end = col_end;
        hit_info->hit_solid = false;
    }

    if (moving_south) {
        const float tile_height = static_cast<float>(tile_size_.y);
        const int first_boundary_row = static_cast<int>(std::ceil(start_edge / tile_height));
        const int last_boundary_row = static_cast<int>(std::floor(end_edge / tile_height));
        for (int boundary_row = first_boundary_row; boundary_row <= last_boundary_row; ++boundary_row) {
            const auto info = horizontalEdgeBlockInfo(boundary_row, col_start, col_end);
            if (!info.blocked) {
                continue;
            }
            const float boundary_y = static_cast<float>(boundary_row) * tile_height;
            const float resolved_bottom = boundary_y - EPSILON;
            const float resolved_pos_y = resolved_bottom - start_rect.size.y;
            if (hit_info) {
                hit_info->boundary_row = boundary_row;
                hit_info->hit_solid = info.has_solid;
            }
            return {true, resolved_pos_y};
        }
    } else {
        const float tile_height = static_cast<float>(tile_size_.y);
        const int first_boundary_row = static_cast<int>(std::floor(start_edge / tile_height));
        const int last_boundary_row = static_cast<int>(std::ceil(end_edge / tile_height));
        for (int boundary_row = first_boundary_row; boundary_row >= last_boundary_row; --boundary_row) {
            const auto info = horizontalEdgeBlockInfo(boundary_row, col_start, col_end);
            if (!info.blocked) {
                continue;
            }
            const float boundary_y = static_cast<float>(boundary_row) * tile_height;
            const float resolved_top = boundary_y + EPSILON;
            const float resolved_pos_y = resolved_top;
            if (hit_info) {
                hit_info->boundary_row = boundary_row;
                hit_info->hit_solid = info.has_solid;
            }
            return {true, resolved_pos_y};
        }
    }

    return {false, end_rect.pos.y};
}

std::pair<bool, float> StaticTileGrid::sweepHorizontal(const engine::utils::Rect& start_rect,
                                                       const engine::utils::Rect& end_rect,
                                                       bool moving_east,
                                                       SweepHitInfo* hit_info) const {
    const float start_edge = moving_east ? start_rect.pos.x + start_rect.size.x : start_rect.pos.x;
    const float end_edge = moving_east ? end_rect.pos.x + end_rect.size.x : end_rect.pos.x;

    if (std::abs(end_edge - start_edge) < EPSILON) {
        if (hit_info) {
            hit_info->boundary_row = -1;
            hit_info->boundary_col = -1;
            hit_info->row_start = -1;
            hit_info->row_end = -1;
            hit_info->col_start = -1;
            hit_info->col_end = -1;
            hit_info->hit_solid = false;
        }
        return {false, end_rect.pos.x};
    }

    const float min_y = std::min(start_rect.pos.y, end_rect.pos.y);
    const float max_y = std::max(start_rect.pos.y + start_rect.size.y, end_rect.pos.y + end_rect.size.y) - EPSILON;
    const int row_start = static_cast<int>(std::floor(min_y / static_cast<float>(tile_size_.y)));
    const int row_end = static_cast<int>(std::floor(max_y / static_cast<float>(tile_size_.y)));

    if (hit_info) {
        hit_info->boundary_row = -1;
        hit_info->boundary_col = -1;
        hit_info->row_start = row_start;
        hit_info->row_end = row_end;
        hit_info->col_start = -1;
        hit_info->col_end = -1;
        hit_info->hit_solid = false;
    }

    if (moving_east) {
        const float tile_width = static_cast<float>(tile_size_.x);
        const int first_boundary_col = static_cast<int>(std::ceil(start_edge / tile_width));
        const int last_boundary_col = static_cast<int>(std::floor(end_edge / tile_width));
        for (int boundary_col = first_boundary_col; boundary_col <= last_boundary_col; ++boundary_col) {
            const auto info = verticalEdgeBlockInfo(boundary_col, row_start, row_end);
            if (!info.blocked) {
                continue;
            }
            const float boundary_x = static_cast<float>(boundary_col) * tile_width;
            const float resolved_right = boundary_x - EPSILON;
            const float resolved_pos_x = resolved_right - start_rect.size.x;
            if (hit_info) {
                hit_info->boundary_col = boundary_col;
                hit_info->hit_solid = info.has_solid;
            }
            return {true, resolved_pos_x};
        }
    } else {
        const float tile_width = static_cast<float>(tile_size_.x);
        const int first_boundary_col = static_cast<int>(std::floor(start_edge / tile_width));
        const int last_boundary_col = static_cast<int>(std::ceil(end_edge / tile_width));
        for (int boundary_col = first_boundary_col; boundary_col >= last_boundary_col; --boundary_col) {
            const auto info = verticalEdgeBlockInfo(boundary_col, row_start, row_end);
            if (!info.blocked) {
                continue;
            }
            const float boundary_x = static_cast<float>(boundary_col) * tile_width;
            const float resolved_left = boundary_x + EPSILON;
            const float resolved_pos_x = resolved_left;
            if (hit_info) {
                hit_info->boundary_col = boundary_col;
                hit_info->hit_solid = info.has_solid;
            }
            return {true, resolved_pos_x};
        }
    }

    return {false, end_rect.pos.x};
}

} // namespace engine::spatial
