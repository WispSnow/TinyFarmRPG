#include "dynamic_entity_grid.h"
#include <algorithm>
#include <cmath>
#include <spdlog/spdlog.h>

namespace engine::spatial {

void DynamicEntityGrid::initialize(glm::vec2 world_bounds_min, glm::vec2 world_bounds_max, glm::vec2 cell_size) {
    world_bounds_min_ = world_bounds_min;
    world_bounds_max_ = world_bounds_max;
    cell_size_ = glm::vec2(std::max(1.0f, cell_size.x), std::max(1.0f, cell_size.y));
    
    // 计算网格尺寸
    const float world_width = std::max(0.0f, world_bounds_max_.x - world_bounds_min_.x);
    const float world_height = std::max(0.0f, world_bounds_max_.y - world_bounds_min_.y);
    grid_size_ = glm::ivec2(
        static_cast<int>(std::ceil(world_width / cell_size_.x)),
        static_cast<int>(std::ceil(world_height / cell_size_.y))
    );
    grid_size_ = glm::ivec2(std::max(0, grid_size_.x), std::max(0, grid_size_.y));
    
    // 初始化网格
    grid_.clear();
    grid_.resize(grid_size_.y);
    for (int y = 0; y < grid_size_.y; ++y) {
        grid_[y].resize(grid_size_.x);
    }
    
    entity_to_cells_.clear();
    
    spdlog::info("初始化动态实体网格: {}x{} 单元格, 单元格大小: {}x{}, 世界范围: ({}, {}) 到 ({}, {})",
                 grid_size_.x, grid_size_.y, cell_size_.x, cell_size_.y,
                 world_bounds_min_.x, world_bounds_min_.y, world_bounds_max_.x, world_bounds_max_.y);
}

glm::ivec2 DynamicEntityGrid::worldToCell(glm::vec2 world_pos) const {
    glm::vec2 relative_pos = world_pos - world_bounds_min_;
    return glm::ivec2(
        static_cast<int>(std::floor(relative_pos.x / cell_size_.x)),
        static_cast<int>(std::floor(relative_pos.y / cell_size_.y))
    );
}

bool DynamicEntityGrid::isValidCell(glm::ivec2 cell_coord) const {
    return cell_coord.x >= 0 && cell_coord.x < grid_size_.x &&
           cell_coord.y >= 0 && cell_coord.y < grid_size_.y;
}

std::unordered_set<glm::ivec2> DynamicEntityGrid::getCellsForRect(const engine::utils::Rect& rect) const {
    std::unordered_set<glm::ivec2> cells;
    if (!isInitialized()) {
        return cells;
    }
    
    // 计算矩形覆盖的单元格范围
    glm::ivec2 min_cell = worldToCell(rect.pos);
    const glm::vec2 epsilon{0.001f, 0.001f};
    glm::vec2 max_for_coord = rect.pos + rect.size - epsilon;
    max_for_coord = glm::max(max_for_coord, rect.pos);
    glm::ivec2 max_cell = worldToCell(max_for_coord);
    
    // 确保在有效范围内
    min_cell = glm::clamp(min_cell, glm::ivec2(0), grid_size_ - glm::ivec2(1));
    max_cell = glm::clamp(max_cell, glm::ivec2(0), grid_size_ - glm::ivec2(1));
    
    // 添加所有覆盖的单元格
    for (int y = min_cell.y; y <= max_cell.y; ++y) {
        for (int x = min_cell.x; x <= max_cell.x; ++x) {
            cells.emplace(glm::ivec2(x, y));
        }
    }
    
    return cells;
}

std::unordered_set<glm::ivec2> DynamicEntityGrid::getCellsForCircle(const glm::vec2& center, float radius) const {
    std::unordered_set<glm::ivec2> cells;
    
    // 计算圆形覆盖的单元格范围（简化为AABB的情况即可）
    engine::utils::Rect rect(center - glm::vec2(radius), glm::vec2(radius * 2.0f));
    return getCellsForRect(rect);
}

void DynamicEntityGrid::addEntityToCells(entt::entity entity, const std::unordered_set<glm::ivec2>& cells) {
    for (const auto& cell_coord : cells) {
        if (!isValidCell(cell_coord)) continue;
        
        auto& cell_entities = grid_[cell_coord.y][cell_coord.x];
        // 检查是否已存在（避免重复添加）
        if (std::find(cell_entities.begin(), cell_entities.end(), entity) == cell_entities.end()) {
            cell_entities.push_back(entity);
        }
    }
}

void DynamicEntityGrid::removeEntityFromCells(entt::entity entity) {
    auto it = entity_to_cells_.find(entity);
    if (it != entity_to_cells_.end()) {
        for (const auto& cell_coord : it->second) {
            if (!isValidCell(cell_coord)) continue;
            
            auto& cell_entities = grid_[cell_coord.y][cell_coord.x];
            cell_entities.erase(
                std::remove(cell_entities.begin(), cell_entities.end(), entity),
                cell_entities.end()
            );
        }
        entity_to_cells_.erase(it);
    }
}

void DynamicEntityGrid::addEntity(entt::entity entity, const engine::utils::Rect& bounds) {
    // 先移除（如果已存在）
    removeEntity(entity);
    
    // 计算覆盖的单元格
    auto cells = getCellsForRect(bounds);
    
    // 添加到单元格
    addEntityToCells(entity, cells);
    
    // 记录实体到单元格的映射
    entity_to_cells_.emplace(entity, cells);
}

void DynamicEntityGrid::addEntity(entt::entity entity, const glm::vec2& center, float radius) {
    // 先移除（如果已存在）
    removeEntity(entity);
    
    // 计算覆盖的单元格
    auto cells = getCellsForCircle(center, radius);
    
    // 添加到单元格
    addEntityToCells(entity, cells);
    
    // 记录实体到单元格的映射
    entity_to_cells_.emplace(entity, cells);
}

void DynamicEntityGrid::removeEntity(entt::entity entity) {
    removeEntityFromCells(entity);
}

void DynamicEntityGrid::updateEntity(entt::entity entity, const engine::utils::Rect& bounds) {
    // 更新就是先移除再添加
    addEntity(entity, bounds);
}

void DynamicEntityGrid::updateEntity(entt::entity entity, const glm::vec2& center, float radius) {
    // 更新就是先移除再添加
    addEntity(entity, center, radius);
}

std::vector<entt::entity> DynamicEntityGrid::queryEntities(const engine::utils::Rect& rect) const {
    // 计算查询区域覆盖的单元格
    auto cells = getCellsForRect(rect);

    std::vector<entt::entity> results;
    if (cells.empty()) {
        return results;
    }

    // 收集所有单元格中的实体（可能重复）
    for (const auto& cell_coord : cells) {
        if (!isValidCell(cell_coord)) continue;
        const auto& cell_entities = grid_[cell_coord.y][cell_coord.x];
        results.insert(results.end(), cell_entities.begin(), cell_entities.end());
    }

    // 去重（broadphase 只需要候选集合，顺序无关）
    std::sort(results.begin(), results.end());
    results.erase(std::unique(results.begin(), results.end()), results.end());
    return results;
}

std::vector<entt::entity> DynamicEntityGrid::queryEntities(const glm::vec2& center, float radius) const {
    // 计算查询区域覆盖的单元格
    auto cells = getCellsForCircle(center, radius);

    std::vector<entt::entity> results;
    if (cells.empty()) {
        return results;
    }

    // 收集所有单元格中的实体（可能重复）
    for (const auto& cell_coord : cells) {
        if (!isValidCell(cell_coord)) continue;
        const auto& cell_entities = grid_[cell_coord.y][cell_coord.x];
        results.insert(results.end(), cell_entities.begin(), cell_entities.end());
    }

    // 去重（broadphase 只需要候选集合，顺序无关）
    std::sort(results.begin(), results.end());
    results.erase(std::unique(results.begin(), results.end()), results.end());
    return results;
}

std::vector<entt::entity> DynamicEntityGrid::queryEntitiesAt(glm::vec2 world_pos) const {
    glm::ivec2 cell_coord = worldToCell(world_pos);
    if (!isValidCell(cell_coord)) {
        return std::vector<entt::entity>();
    }
    
    const auto& cell_entities = grid_[cell_coord.y][cell_coord.x];
    return std::vector<entt::entity>(cell_entities.begin(), cell_entities.end());
}

const std::vector<entt::entity>& DynamicEntityGrid::getCellEntities(glm::ivec2 cell_coord) const {
    static const std::vector<entt::entity> empty;
    if (!isValidCell(cell_coord)) {
        return empty;
    }
    return grid_[cell_coord.y][cell_coord.x];
}

size_t DynamicEntityGrid::getUsedCellCount() const {
    size_t count = 0;
    for (int y = 0; y < grid_size_.y; ++y) {
        for (int x = 0; x < grid_size_.x; ++x) {
            if (!grid_[y][x].empty()) {
                count++;
            }
        }
    }
    return count;
}

size_t DynamicEntityGrid::getTotalEntityCount() const {
    return entity_to_cells_.size();
}

} // namespace engine::spatial
