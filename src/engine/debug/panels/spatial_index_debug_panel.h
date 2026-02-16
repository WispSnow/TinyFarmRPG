#pragma once

#include "engine/debug/debug_panel.h"
#include <entt/entity/fwd.hpp>
#include <cstddef>
#include <string>
#include <vector>

namespace engine::spatial {
class SpatialIndexManager;
}

namespace engine::scene {
class SceneManager;
}

namespace engine::debug {

/**
 * @brief 空间索引调试面板，用于可视化、监控和调试空间分区系统
 */
class SpatialIndexDebugPanel final : public DebugPanel {
    engine::scene::SceneManager& scene_manager_;
    engine::spatial::SpatialIndexManager& spatial_index_manager_;
    
    // UI状态
    int selected_tab_ = 0;  // 0=静态网格, 1=动态网格, 2=综合查询
    
    // 静态网格查询输入
    int static_query_tile_x_ = 0;
    int static_query_tile_y_ = 0;
    
    // 动态网格查询输入
    float dynamic_query_pos_x_ = 0.0f;
    float dynamic_query_pos_y_ = 0.0f;
    float dynamic_query_rect_x_ = 0.0f;
    float dynamic_query_rect_y_ = 0.0f;
    float dynamic_query_rect_w_ = 100.0f;
    float dynamic_query_rect_h_ = 100.0f;
    float dynamic_query_circle_x_ = 0.0f;
    float dynamic_query_circle_y_ = 0.0f;
    float dynamic_query_circle_r_ = 50.0f;
    bool dynamic_query_show_candidates_ = false;

    struct EntityQueryCache {
        bool has_result{false};
        float x{0.0f};
        float y{0.0f};
        float w{0.0f};
        float h{0.0f};
        float r{0.0f};
        std::vector<entt::entity> candidates{};
        std::vector<entt::entity> overlaps{};
    };

    EntityQueryCache dynamic_point_query_cache_{};
    EntityQueryCache dynamic_rect_query_cache_{};
    EntityQueryCache dynamic_circle_query_cache_{};

    struct CollisionQueryCache {
        bool has_result{false};
        bool has_static_collision{false};
        float x{0.0f};
        float y{0.0f};
        float w{0.0f};
        float h{0.0f};
        float r{0.0f};
        std::vector<entt::entity> dynamic_colliders{};
    };

    CollisionQueryCache collision_rect_query_cache_{};
    CollisionQueryCache collision_circle_query_cache_{};
    
    // 综合查询输入
    float collision_query_rect_x_ = 0.0f;
    float collision_query_rect_y_ = 0.0f;
    float collision_query_rect_w_ = 100.0f;
    float collision_query_rect_h_ = 100.0f;
    float collision_query_circle_x_ = 0.0f;
    float collision_query_circle_y_ = 0.0f;
    float collision_query_circle_r_ = 50.0f;
    
    // 可视化选项
    bool show_static_grid_lines_ = false;
    bool show_thin_walls_ = false;
    bool highlight_solid_tiles_ = false;
    bool highlight_entities_cells_ = false;
    bool show_dynamic_grid_lines_ = false;
    bool highlight_dynamic_cells_ = false;
    bool show_entity_bounds_ = false;
    
    // 统计信息缓存（避免每帧计算）
    int stats_update_counter_ = 0;
    static constexpr int STATS_UPDATE_INTERVAL = 30;  // 每30帧更新一次统计
    
    struct StaticGridStats {
        size_t total_cells_ = 0;
        size_t solid_tiles_ = 0;
        size_t blocked_n_tiles_ = 0;
        size_t blocked_s_tiles_ = 0;
        size_t blocked_w_tiles_ = 0;
        size_t blocked_e_tiles_ = 0;
        size_t cells_with_entities_ = 0;
        size_t total_entities_ = 0;
        size_t empty_tiles_ = 0;
        size_t normal_tiles_ = 0;
        size_t hazard_tiles_ = 0;
        size_t water_tiles_ = 0;
        size_t interact_tiles_ = 0;
        size_t arable_tiles_ = 0;
        size_t occupied_tiles_ = 0;
    } static_stats_;
    
    struct DynamicGridStats {
        size_t total_entities_ = 0;
        size_t aabb_colliders_ = 0;
        size_t circle_colliders_ = 0;
        size_t cells_with_entities_ = 0;
        size_t total_cells_ = 0;
    } dynamic_stats_;

    // 静态瓦片查询缓存（用于持久显示）
    bool static_query_has_result_ = false;
    bool static_query_coord_valid_ = false;
    bool static_query_is_solid_ = false;
    int static_query_result_tile_x_ = 0;
    int static_query_result_tile_y_ = 0;
    std::string static_query_flags_{};
    std::vector<entt::entity> static_query_entities_{};
    
    // 辅助函数
    void drawStaticGridTab();
    void drawDynamicGridTab();
    void drawCollisionQueryTab();
    void updateStaticGridStats();
    void updateDynamicGridStats();
    void drawStaticGridInfo();
    void drawStaticGridStats();
    void drawStaticGridVisualization();
    void drawStaticGridQuery();
    void drawDynamicGridInfo();
    void drawDynamicGridStats();
    void drawDynamicGridVisualization();
    void drawDynamicGridQueries();
    void drawCollisionQueries();

public:
    SpatialIndexDebugPanel(engine::scene::SceneManager& scene_manager,
                           engine::spatial::SpatialIndexManager& spatial_index_manager);

    std::string_view name() const override;
    void draw(bool& is_open) override;

    [[nodiscard]] bool hasStaticVisualizationEnabled() const;
    [[nodiscard]] bool hasDynamicVisualizationEnabled() const;
    [[nodiscard]] bool hasAnyVisualizationEnabled() const;
    [[nodiscard]] bool isStaticGridLinesEnabled() const { return show_static_grid_lines_; }
    [[nodiscard]] bool isThinWallVisualizationEnabled() const { return show_thin_walls_; }
    [[nodiscard]] bool isHighlightSolidTilesEnabled() const { return highlight_solid_tiles_; }
    [[nodiscard]] bool isHighlightStaticEntityCellsEnabled() const { return highlight_entities_cells_; }
    [[nodiscard]] bool isDynamicGridLinesEnabled() const { return show_dynamic_grid_lines_; }
    [[nodiscard]] bool isHighlightDynamicCellsEnabled() const { return highlight_dynamic_cells_; }
    [[nodiscard]] bool isEntityBoundsEnabled() const { return show_entity_bounds_; }

private:
};

} // namespace engine::debug
