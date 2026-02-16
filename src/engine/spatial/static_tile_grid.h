#pragma once
#include "engine/component/tilelayer_component.h"
#include "engine/utils/math.h"
#include <glm/vec2.hpp>
#include <vector>
#include <utility>
#include <unordered_map>
#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>

namespace engine::spatial {

/**
 * @brief sweep 查询的命中信息（用于调试/可视化）
 * @note 仅在命中时有意义；范围字段用于描述本次 sweep 涉及的 tile span。
 */
struct SweepHitInfo {
    bool hit_solid = false; ///< @brief 本次命中是否由 SOLID 瓦片参与导致

    // 命中的边界（vertical sweep 命中 boundary_row；horizontal sweep 命中 boundary_col）
    int boundary_row = -1;
    int boundary_col = -1;

    // sweep 覆盖的 tile span
    int row_start = -1;
    int row_end = -1;
    int col_start = -1;
    int col_end = -1;
};

/**
 * @brief 单元格数据，存储每个瓦片单元格的完整信息
 * @note 用于支持自动图块和地块状态管理
 */
struct TileCellData {
    engine::component::TileType tile_type_ = engine::component::TileType::NORMAL;  ///< @brief 瓦片类型（位掩码）
    // 图层 -> 实体映射，每个图层最多一个实体；同时保留顺序列表以兼容现有遍历/栈顶逻辑
    std::vector<entt::entity> entities_;                     ///< @brief 实体列表（顺序插入，用于遍历/取顶层）
    std::vector<entt::id_type> layers_;                      ///< @brief 与 entities_ 对应的图层名称
    std::unordered_map<entt::id_type, std::size_t> layer_to_index_; ///< @brief 图层到索引的映射

    [[nodiscard]] bool hasEntity(entt::entity entity) const;
    [[nodiscard]] bool hasLayer(entt::id_type layer_id) const;
    [[nodiscard]] entt::entity getEntity(entt::id_type layer_id) const;
    void addEntity(entt::entity entity, entt::id_type layer_id);
    void removeEntity(entt::entity entity);
    void removeEntityByLayer(entt::id_type layer_id);
    void clearEntities();

private:
    void removeAtIndex(std::size_t index);
};

/**
 * @brief 静态瓦片网格索引，用于快速查询SOLID瓦片和单元格实体
 * @note 扩展设计以支持自动图块功能（需要查询周围8个地块）
 */
class StaticTileGrid {
private:
    glm::ivec2 map_size_{0, 0};           ///< @brief 地图尺寸（瓦片数量）
    glm::ivec2 tile_size_{1, 1};          ///< @brief 瓦片尺寸（像素）
    std::vector<TileCellData> cells_;  ///< @brief 一维数组，存储每个单元格的完整数据
    static constexpr float EPSILON = 0.001f; ///< @brief 浮点容差，避免卡边
    
public:
    StaticTileGrid() = default;
    ~StaticTileGrid() = default;

    [[nodiscard]] bool isInitialized() const noexcept {
        if (map_size_.x <= 0 || map_size_.y <= 0 || tile_size_.x <= 0 || tile_size_.y <= 0) {
            return false;
        }
        const auto expected =
            static_cast<std::size_t>(map_size_.x) * static_cast<std::size_t>(map_size_.y);
        return cells_.size() == expected;
    }

    void reset() {
        map_size_ = {0, 0};
        tile_size_ = {1, 1};
        cells_.clear();
    }
    
    // 初始化：根据地图尺寸创建网格
    void initialize(glm::ivec2 map_size, glm::ivec2 tile_size);
    
    // 瓦片类型设置接口
    void setTileType(glm::ivec2 tile_coord, engine::component::TileType type);
    void clearTileType(glm::ivec2 tile_coord, engine::component::TileType mask);

    // 实体管理接口
    void addEntity(glm::ivec2 tile_coord, entt::entity entity, entt::id_type layer_id = entt::null);
    void addEntityAtWorldPos(glm::vec2 world_pos, entt::entity entity, entt::id_type layer_id = entt::null);
    void removeEntity(glm::ivec2 tile_coord, entt::entity entity);
    void removeEntityAtWorldPos(glm::vec2 world_pos, entt::entity entity);
    void removeEntityFromAll(entt::entity entity);  ///< @brief 从所有单元格中移除实体
    [[nodiscard]] entt::entity getEntity(glm::ivec2 tile_coord, entt::id_type layer_id) const;
    [[nodiscard]] entt::entity getEntityAtWorldPos(glm::vec2 world_pos, entt::id_type layer_id) const;

    // 碰撞检测查询接口
    [[nodiscard]] bool isSolid(glm::ivec2 tile_coord) const;     ///< @brief 是否为完全阻挡（SOLID）
    [[nodiscard]] bool isSolid(int tile_index) const;
    [[nodiscard]] bool isSolidAtWorldPos(glm::vec2 world_pos) const;
    [[nodiscard]] bool isBlockedNorth(glm::ivec2 tile_coord) const;  ///< @brief 北方向是否被阻挡
    [[nodiscard]] bool isBlockedSouth(glm::ivec2 tile_coord) const;  ///< @brief 南方向是否被阻挡
    [[nodiscard]] bool isBlockedWest(glm::ivec2 tile_coord) const;   ///< @brief 西方向是否被阻挡
    [[nodiscard]] bool isBlockedEast(glm::ivec2 tile_coord) const;   ///< @brief 东方向是否被阻挡
    [[nodiscard]] bool hasHazard(glm::ivec2 tile_coord) const;       ///< @brief 是否有危险区域
    [[nodiscard]] bool hasWater(glm::ivec2 tile_coord) const;        ///< @brief 是否有水域
    [[nodiscard]] bool isInteractable(glm::ivec2 tile_coord) const;  ///< @brief 是否可交互
    [[nodiscard]] bool isArable(glm::ivec2 tile_coord) const;        ///< @brief 是否可耕作
    [[nodiscard]] bool isOccupied(glm::ivec2 tile_coord) const;      ///< @brief 是否被占用（不可耕作）
    [[nodiscard]] bool isThinWallBlockedBetween(glm::ivec2 from_tile, glm::ivec2 to_tile) const; ///< @brief 相邻瓦片之间是否存在薄墙阻挡

    // 类型查询接口
    [[nodiscard]] engine::component::TileType getTileType(glm::ivec2 tile_coord) const;

    // 实体查询
    [[nodiscard]] const std::vector<entt::entity>& getEntities(glm::ivec2 tile_coord) const;
    [[nodiscard]] const std::vector<entt::entity>& getEntitiesAtWorldPos(glm::vec2 world_pos) const;
    [[nodiscard]] const TileCellData& getCellData(glm::ivec2 tile_coord) const;

    // 区域查询
    [[nodiscard]] bool hasSolidInRect(const engine::utils::Rect& rect) const;
    [[nodiscard]] std::vector<glm::ivec2> getSolidTilesInRect(const engine::utils::Rect& rect) const;
    [[nodiscard]] bool hasBlockedDirectionInRect(const engine::utils::Rect& rect, engine::component::TileType direction_flag) const;
    [[nodiscard]] std::vector<glm::ivec2> getTilesWithFlagInRect(const engine::utils::Rect& rect, engine::component::TileType flag) const;

    // 坐标查询
    [[nodiscard]] glm::ivec2 getTileCoordAtWorldPos(glm::vec2 world_pos) const;
    [[nodiscard]] engine::utils::Rect getRectAtWorldPos(glm::vec2 world_pos) const;

    /**
     * @brief 沿纵向（Y轴）扫掠，检测薄墙阻挡并返回裁剪后的位置
     * @param start_rect 起始包围盒
     * @param end_rect 目标包围盒（仅Y轴发生变化）
     * @param moving_south 是否向南移动
     * @param hit_info 命中信息
     * @return pair<bool,float>：bool 是否命中薄墙，float 裁剪后的 rect.pos.y
     */
    [[nodiscard]] std::pair<bool, float> sweepVertical(const engine::utils::Rect& start_rect,
                                         const engine::utils::Rect& end_rect,
                                         bool moving_south,
                                         SweepHitInfo* hit_info = nullptr) const;

    /**
     * @brief 沿横向（X轴）扫掠，检测薄墙阻挡并返回裁剪后的位置
     * @param start_rect 起始包围盒
     * @param end_rect 目标包围盒（仅X轴发生变化）
     * @param moving_east 是否向东移动
     * @param hit_info 命中信息
     * @return pair<bool,float>：bool 是否命中薄墙，float 裁剪后的 rect.pos.x
     */
    [[nodiscard]] std::pair<bool, float> sweepHorizontal(const engine::utils::Rect& start_rect,
                                           const engine::utils::Rect& end_rect,
                                           bool moving_east,
                                           SweepHitInfo* hit_info = nullptr) const;

    // Getters
    [[nodiscard]] const glm::ivec2& getMapSize() const { return map_size_; }
    [[nodiscard]] const glm::ivec2& getTileSize() const { return tile_size_; }

private:
    // 辅助函数：Rect → 瓦片坐标范围（已 clamp 到合法区间）
    std::pair<glm::ivec2, glm::ivec2> tileRangeForRect(const engine::utils::Rect& rect) const;

    // 辅助函数：坐标转换
    int coordToIndex(glm::ivec2 tile_coord) const;
    glm::ivec2 indexToCoord(int tile_index) const;
    glm::ivec2 worldToTileCoord(glm::vec2 world_pos) const;
    
    // 边界检查
    bool isValidCoord(glm::ivec2 tile_coord) const;

    struct EdgeBlockInfo {
        bool blocked = false;
        bool has_solid = false;
    };

    EdgeBlockInfo verticalEdgeBlockInfo(int boundary_col, int row_start, int row_end) const;
    EdgeBlockInfo horizontalEdgeBlockInfo(int boundary_row, int col_start, int col_end) const;
};

} // namespace engine::spatial
