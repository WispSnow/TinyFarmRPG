#pragma once
#include "engine/utils/math.h"
#include <glm/vec2.hpp>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <entt/entity/entity.hpp>

namespace engine::spatial {

/**
 * @brief 动态实体网格索引，用于快速查询碰撞器实体
 */
class DynamicEntityGrid {
private:
    glm::ivec2 grid_size_{0, 0};          ///< @brief 网格尺寸（单元格数量）
    glm::vec2 cell_size_{1.0f, 1.0f};     ///< @brief 单元格大小（像素）
    glm::vec2 world_bounds_min_{0.0f, 0.0f};     ///< @brief 世界坐标边界（最小值）
    glm::vec2 world_bounds_max_{0.0f, 0.0f};     ///< @brief 世界坐标边界（最大值）
    
    // 每个单元格存储实体ID列表
    // grid_[y][x] 存储该单元格的实体列表
    std::vector<std::vector<std::vector<entt::entity>>> grid_;
    
    // 实体到网格单元格的映射（用于快速更新）
    // 一个实体可能跨越多个单元格
    std::unordered_map<entt::entity, std::unordered_set<glm::ivec2>> entity_to_cells_;
    
    // 辅助函数：坐标转换
    glm::ivec2 worldToCell(glm::vec2 world_pos) const;
    bool isValidCell(glm::ivec2 cell_coord) const;
    
    // 辅助函数：计算实体覆盖的网格单元格
    std::unordered_set<glm::ivec2> getCellsForRect(const engine::utils::Rect& rect) const;
    std::unordered_set<glm::ivec2> getCellsForCircle(const glm::vec2& center, float radius) const;
    
    // 内部更新函数
    void addEntityToCells(entt::entity entity, const std::unordered_set<glm::ivec2>& cells);
    void removeEntityFromCells(entt::entity entity);
    
public:
    DynamicEntityGrid() = default;
    ~DynamicEntityGrid() = default;

    [[nodiscard]] bool isInitialized() const noexcept {
        return grid_size_.x > 0 && grid_size_.y > 0 && cell_size_.x > 0.0f && cell_size_.y > 0.0f;
    }

    void reset() {
        grid_size_ = {0, 0};
        cell_size_ = {1.0f, 1.0f};
        world_bounds_min_ = {0.0f, 0.0f};
        world_bounds_max_ = {0.0f, 0.0f};
        grid_.clear();
        entity_to_cells_.clear();
    }
    
    // 初始化
    void initialize(glm::vec2 world_bounds_min, glm::vec2 world_bounds_max, glm::vec2 cell_size);
    
    // 添加/移除实体
    void addEntity(entt::entity entity, const engine::utils::Rect& bounds);
    void addEntity(entt::entity entity, const glm::vec2& center, float radius);  // 圆形碰撞器
    void removeEntity(entt::entity entity);
    void updateEntity(entt::entity entity, const engine::utils::Rect& bounds);
    void updateEntity(entt::entity entity, const glm::vec2& center, float radius);
    
    // 查询接口
    [[nodiscard]] std::vector<entt::entity> queryEntities(const engine::utils::Rect& rect) const;
    [[nodiscard]] std::vector<entt::entity> queryEntities(const glm::vec2& center, float radius) const;
    [[nodiscard]] std::vector<entt::entity> queryEntitiesAt(glm::vec2 world_pos) const;

    // Getters
    [[nodiscard]] const glm::ivec2& getGridSize() const { return grid_size_; }
    [[nodiscard]] const glm::vec2& getCellSize() const { return cell_size_; }
    [[nodiscard]] const glm::vec2& getWorldBoundsMin() const { return world_bounds_min_; }
    [[nodiscard]] const glm::vec2& getWorldBoundsMax() const { return world_bounds_max_; }
    
    // 调试接口：直接访问单元格数据
    /**
     * @brief 获取指定单元格的实体列表（直接访问内部数据）
     * @param cell_coord 单元格坐标
     * @return 该单元格的实体列表的常量引用
     */
    [[nodiscard]] const std::vector<entt::entity>& getCellEntities(glm::ivec2 cell_coord) const;

    /**
     * @brief 统计有实体的单元格数量（用于调试）
     * @return 有实体的单元格数量
     */
    [[nodiscard]] size_t getUsedCellCount() const;

    /**
     * @brief 获取所有注册的实体总数（用于调试）
     * @return 实体总数
     */
    [[nodiscard]] size_t getTotalEntityCount() const;
};

} // namespace engine::spatial
