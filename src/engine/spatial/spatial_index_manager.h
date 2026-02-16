#pragma once
#include "static_tile_grid.h"
#include "dynamic_entity_grid.h"
#include <entt/entity/registry.hpp>
#include <glm/vec2.hpp>

namespace engine::spatial {

enum class Direction {
    North,
    South,
    East,
    West
};

/**
 * @brief 碰撞检测结果
 */
struct CollisionResult {
    bool has_static_collision = false;           ///< @brief 是否有静态碰撞（传统SOLID）
    bool blocked_north = false;                  ///< @brief 北方向（上方）被阻挡
    bool blocked_south = false;                  ///< @brief 南方向（下方）被阻挡
    bool blocked_west = false;                   ///< @brief 西方向（左方）被阻挡
    bool blocked_east = false;                   ///< @brief 东方向（右方）被阻挡
    bool has_hazard = false;                     ///< @brief 有危险区域
    bool has_water = false;                      ///< @brief 有水域
    std::vector<entt::entity> dynamic_colliders; ///< @brief 动态碰撞器列表
};

/**
 * @brief 薄墙裁剪结果
 */
struct SweepResult {
    engine::utils::Rect rect{}; ///< @brief 裁剪后的静态允许区域
    bool hit_thin_wall = false; ///< @brief 是否命中薄墙（方向阻挡边界）
    bool hit_solid = false;     ///< @brief 是否命中 SOLID 瓦片边界
    SweepHitInfo hit_info{};    ///< @brief sweep 命中边界信息（用于调试/可视化）
};

/**
 * @brief 空间索引管理器，统一管理静态和动态网格
 */
class SpatialIndexManager {
private:
    StaticTileGrid static_grid_;
    DynamicEntityGrid dynamic_grid_;
    entt::registry* registry_{nullptr};  ///< @brief 非拥有指针
    
public:
    SpatialIndexManager() = default;
    ~SpatialIndexManager() = default;

    SpatialIndexManager(entt::registry& registry,
                        glm::ivec2 map_size,
                        glm::ivec2 tile_size,
                        glm::vec2 world_bounds_min,
                        glm::vec2 world_bounds_max,
                        glm::vec2 dynamic_cell_size = glm::vec2(64.0f, 64.0f)) {
        initialize(registry, map_size, tile_size, world_bounds_min, world_bounds_max, dynamic_cell_size);
    }

    [[nodiscard]] bool isInitialized() const noexcept {
        return registry_ != nullptr && static_grid_.isInitialized() && dynamic_grid_.isInitialized();
    }

    void reset() {
        registry_ = nullptr;
        static_grid_.reset();
        dynamic_grid_.reset();
    }

    void resetIfUsingRegistry(const entt::registry* registry) {
        if (registry_ == registry) {
            reset();
        }
    }
    
    // 初始化
    void initialize(entt::registry& registry, 
                    glm::ivec2 map_size, 
                    glm::ivec2 tile_size,
                    glm::vec2 world_bounds_min,
                    glm::vec2 world_bounds_max,
                    glm::vec2 dynamic_cell_size = glm::vec2(64.0f, 64.0f));
    
    // 静态网格接口（代理）
    void setTileType(glm::ivec2 tile_coord, engine::component::TileType type);
    void clearTileType(glm::ivec2 tile_coord, engine::component::TileType mask);
    void addTileEntity(glm::ivec2 tile_coord, entt::entity entity, entt::id_type layer_id = entt::null);
    void addTileEntityAtWorldPos(glm::vec2 world_pos, entt::entity entity, entt::id_type layer_id = entt::null);
    void removeTileEntity(glm::ivec2 tile_coord, entt::entity entity);
    void removeTileEntityAtWorldPos(glm::vec2 world_pos, entt::entity entity);
    [[nodiscard]] bool isSolidAt(glm::vec2 world_pos) const;
    [[nodiscard]] bool isBlockedNorthAt(glm::ivec2 tile_coord) const;
    [[nodiscard]] bool isBlockedSouthAt(glm::ivec2 tile_coord) const;
    [[nodiscard]] bool isBlockedWestAt(glm::ivec2 tile_coord) const;
    [[nodiscard]] bool isBlockedEastAt(glm::ivec2 tile_coord) const;
    [[nodiscard]] bool hasHazardAt(glm::ivec2 tile_coord) const;
    [[nodiscard]] bool hasWaterAt(glm::ivec2 tile_coord) const;
    [[nodiscard]] bool isInteractableAt(glm::ivec2 tile_coord) const;
    [[nodiscard]] bool isArableAt(glm::ivec2 tile_coord) const;
    [[nodiscard]] bool isOccupiedAt(glm::ivec2 tile_coord) const;
    [[nodiscard]] bool isThinWallBlockedBetween(glm::ivec2 from_tile, glm::ivec2 to_tile) const;
    [[nodiscard]] bool hasSolidInRect(const engine::utils::Rect& rect) const;
    [[nodiscard]] bool hasBlockedDirectionInRect(const engine::utils::Rect& rect, engine::component::TileType direction_flag) const;
    [[nodiscard]] CollisionResult checkDirectionalCollision(const engine::utils::Rect& rect) const;
    [[nodiscard]] const std::vector<entt::entity>& getTileEntities(glm::ivec2 tile_coord) const;
    [[nodiscard]] const std::vector<entt::entity>& getTileEntitiesAtWorldPos(glm::vec2 world_pos) const;
    [[nodiscard]] entt::entity getTileEntity(glm::ivec2 tile_coord, entt::id_type layer_id) const;
    [[nodiscard]] entt::entity getTileEntityAtWorldPos(glm::vec2 world_pos, entt::id_type layer_id) const;
    [[nodiscard]] glm::ivec2 getTileCoordAtWorldPos(glm::vec2 world_pos) const;
    [[nodiscard]] engine::utils::Rect getRectAtWorldPos(glm::vec2 world_pos) const;
    
    // 动态网格接口（代理）
    void addColliderEntity(entt::entity entity);
    void removeColliderEntity(entt::entity entity);
    void updateColliderEntity(entt::entity entity);

    // 动态网格查询：broadphase（仅返回候选实体，不保证真实相交）
    [[nodiscard]] std::vector<entt::entity> queryColliderCandidates(const engine::utils::Rect& rect) const;
    [[nodiscard]] std::vector<entt::entity> queryColliderCandidates(const glm::vec2& center, float radius) const;
    [[nodiscard]] std::vector<entt::entity> queryColliderCandidatesAt(glm::vec2 world_pos) const;

    // 动态网格查询：narrowphase（返回与查询形状真实相交的实体）
    [[nodiscard]] std::vector<entt::entity> queryColliders(const engine::utils::Rect& rect) const;
    [[nodiscard]] std::vector<entt::entity> queryColliders(const glm::vec2& center, float radius) const;
    [[nodiscard]] std::vector<entt::entity> queryCollidersAt(glm::vec2 world_pos) const;

    // 综合查询（同时查询静态和动态）
    [[nodiscard]] CollisionResult checkCollision(const engine::utils::Rect& rect) const;
    [[nodiscard]] CollisionResult checkCollision(const glm::vec2& center, float radius) const;

    // 薄墙裁剪（静态）
    [[nodiscard]] SweepResult resolveStaticSweep(const engine::utils::Rect& start_rect,
                                   const engine::utils::Rect& target_rect,
                                   Direction direction) const;

    // 批量更新（用于系统更新）
    void updateAllDynamicEntities();
    
    // Getters（用于直接访问网格，高级用法）
    StaticTileGrid& getStaticGrid() { return static_grid_; }
    const StaticTileGrid& getStaticGrid() const { return static_grid_; }
    DynamicEntityGrid& getDynamicGrid() { return dynamic_grid_; }
    const DynamicEntityGrid& getDynamicGrid() const { return dynamic_grid_; }
};

} // namespace engine::spatial
