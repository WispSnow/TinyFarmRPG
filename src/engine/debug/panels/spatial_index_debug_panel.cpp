#include "spatial_index_debug_panel.h"
#include "engine/spatial/spatial_index_manager.h"
#include "engine/spatial/static_tile_grid.h"
#include "engine/spatial/dynamic_entity_grid.h"
#include "engine/component/transform_component.h"
#include "engine/component/collider_component.h"
#include "engine/component/tags.h"
#include "engine/component/tilelayer_component.h"
#include "engine/scene/scene_manager.h"
#include "engine/scene/scene.h"
#include <imgui.h>
#include <glm/vec2.hpp>
#include <entt/entity/registry.hpp>

namespace {

/// Return a human-readable collider type label for the given entity.
const char* colliderTypeLabel(entt::registry& registry, entt::entity entity) {
    if (registry.all_of<engine::component::AABBCollider>(entity)) {
        return "AABB";
    }
    if (registry.all_of<engine::component::CircleCollider>(entity)) {
        return "Circle";
    }
    return "Unknown";
}

/// Draw an ImGui table listing entities with their collider type (and
/// optionally their position).  Covers the repeated pattern found in
/// the dynamic-grid query tables and the collision query tables.
///
/// @param table_id        Unique ImGui ID for the table widget.
/// @param entities        Entity list to display.
/// @param registry        ECS registry used to look up components.
/// @param include_position If true an extra "位置" column is added.
/// @param max_display     Maximum rows before a "... (还有 N 个)" overflow
///                        message is shown.
void drawEntityTable(const char* table_id,
                     const std::vector<entt::entity>& entities,
                     entt::registry& registry,
                     bool include_position = false,
                     int max_display = 20) {
    const int column_count = include_position ? 3 : 2;
    if (!ImGui::BeginTable(table_id, column_count,
                           ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        return;
    }

    ImGui::TableSetupColumn("实体ID");
    ImGui::TableSetupColumn("类型");
    if (include_position) {
        ImGui::TableSetupColumn("位置");
    }
    ImGui::TableHeadersRow();

    for (size_t i = 0; i < entities.size() && i < static_cast<size_t>(max_display); ++i) {
        auto entity = entities[i];
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("%u", static_cast<unsigned>(entity));

        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%s", colliderTypeLabel(registry, entity));

        if (include_position) {
            ImGui::TableSetColumnIndex(2);
            if (registry.all_of<engine::component::TransformComponent>(entity)) {
                const auto& transform = registry.get<engine::component::TransformComponent>(entity);
                ImGui::Text("(%.1f, %.1f)", transform.position_.x, transform.position_.y);
            } else {
                ImGui::Text("N/A");
            }
        }
    }

    if (entities.size() > static_cast<size_t>(max_display)) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("... (还有 %zu 个)", entities.size() - static_cast<size_t>(max_display));
    }

    ImGui::EndTable();
}

} // anonymous namespace

namespace engine::debug {

SpatialIndexDebugPanel::SpatialIndexDebugPanel(engine::scene::SceneManager& scene_manager, 
                                                 engine::spatial::SpatialIndexManager& spatial_index_manager)
    : scene_manager_(scene_manager), spatial_index_manager_(spatial_index_manager) {
}

std::string_view SpatialIndexDebugPanel::name() const {
    return "Spatial Index";
}

void SpatialIndexDebugPanel::draw(bool& is_open) {
    if (!is_open) {
        return;
    }

    if (!ImGui::Begin("空间分区调试", &is_open, ImGuiWindowFlags_None)) {
        ImGui::End();
        return;
    }

    // Tab切换
    if (ImGui::BeginTabBar("SpatialIndexTabs")) {
        if (ImGui::BeginTabItem("静态网格")) {
            selected_tab_ = 0;
            drawStaticGridTab();
            ImGui::EndTabItem();
        }
        
        if (ImGui::BeginTabItem("动态网格")) {
            selected_tab_ = 1;
            drawDynamicGridTab();
            ImGui::EndTabItem();
        }
        
        if (ImGui::BeginTabItem("综合查询")) {
            selected_tab_ = 2;
            drawCollisionQueryTab();
            ImGui::EndTabItem();
        }
        
        ImGui::EndTabBar();
    }

    ImGui::End();
}

void SpatialIndexDebugPanel::drawStaticGridTab() {
    // 更新统计信息（每N帧更新一次）
    if (stats_update_counter_ % STATS_UPDATE_INTERVAL == 0) {
        updateStaticGridStats();
    }
    stats_update_counter_++;

    // 基础信息
    if (ImGui::CollapsingHeader("基础信息", ImGuiTreeNodeFlags_DefaultOpen)) {
        drawStaticGridInfo();
    }

    // 统计信息
    if (ImGui::CollapsingHeader("统计信息", ImGuiTreeNodeFlags_DefaultOpen)) {
        drawStaticGridStats();
    }

    // 可视化选项
    if (ImGui::CollapsingHeader("可视化选项")) {
        drawStaticGridVisualization();
    }

    // 单元格查询
    if (ImGui::CollapsingHeader("单元格查询")) {
        drawStaticGridQuery();
    }
}

void SpatialIndexDebugPanel::drawDynamicGridTab() {
    // 更新统计信息
    if (stats_update_counter_ % STATS_UPDATE_INTERVAL == 0) {
        updateDynamicGridStats();
    }
    stats_update_counter_++;

    // 基础信息
    if (ImGui::CollapsingHeader("基础信息", ImGuiTreeNodeFlags_DefaultOpen)) {
        drawDynamicGridInfo();
    }

    // 统计信息
    if (ImGui::CollapsingHeader("统计信息", ImGuiTreeNodeFlags_DefaultOpen)) {
        drawDynamicGridStats();
    }

    // 可视化选项
    if (ImGui::CollapsingHeader("可视化选项")) {
        drawDynamicGridVisualization();
    }

    // 实时查询测试
    if (ImGui::CollapsingHeader("实时查询测试")) {
        drawDynamicGridQueries();
    }
}

void SpatialIndexDebugPanel::drawCollisionQueryTab() {
    drawCollisionQueries();
}

bool SpatialIndexDebugPanel::hasStaticVisualizationEnabled() const {
    return show_static_grid_lines_ || show_thin_walls_ || highlight_solid_tiles_ || highlight_entities_cells_;
}

bool SpatialIndexDebugPanel::hasDynamicVisualizationEnabled() const {
    return show_dynamic_grid_lines_ || highlight_dynamic_cells_ || show_entity_bounds_;
}

bool SpatialIndexDebugPanel::hasAnyVisualizationEnabled() const {
    return hasStaticVisualizationEnabled() || hasDynamicVisualizationEnabled();
}

void SpatialIndexDebugPanel::updateStaticGridStats() {
    const auto& static_grid = spatial_index_manager_.getStaticGrid();
    const auto& map_size = static_grid.getMapSize();
    
    static_stats_.total_cells_ = static_cast<size_t>(map_size.x * map_size.y);
    static_stats_.solid_tiles_ = 0;
    static_stats_.blocked_n_tiles_ = 0;
    static_stats_.blocked_s_tiles_ = 0;
    static_stats_.blocked_w_tiles_ = 0;
    static_stats_.blocked_e_tiles_ = 0;
    static_stats_.cells_with_entities_ = 0;
    static_stats_.total_entities_ = 0;
    static_stats_.empty_tiles_ = 0;
    static_stats_.normal_tiles_ = 0;
    static_stats_.hazard_tiles_ = 0;
    static_stats_.water_tiles_ = 0;
    static_stats_.interact_tiles_ = 0;
    static_stats_.arable_tiles_ = 0;
    static_stats_.occupied_tiles_ = 0;

    for (int y = 0; y < map_size.y; ++y) {
        for (int x = 0; x < map_size.x; ++x) {
            glm::ivec2 coord(x, y);
            const auto& cell_data = static_grid.getCellData(coord);
            const auto tile_type = cell_data.tile_type_;

            const auto hasFlag = [&](engine::component::TileType flag) -> bool {
                return engine::component::hasTileFlag(tile_type, flag);
            };

            if (engine::component::hasAllTileFlags(tile_type, engine::component::TileType::SOLID)) {
                static_stats_.solid_tiles_++;
            }
            if (hasFlag(engine::component::TileType::BLOCK_N)) {
                static_stats_.blocked_n_tiles_++;
            }
            if (hasFlag(engine::component::TileType::BLOCK_S)) {
                static_stats_.blocked_s_tiles_++;
            }
            if (hasFlag(engine::component::TileType::BLOCK_W)) {
                static_stats_.blocked_w_tiles_++;
            }
            if (hasFlag(engine::component::TileType::BLOCK_E)) {
                static_stats_.blocked_e_tiles_++;
            }

            // 检查各种瓦片类型（支持位掩码）
            if (tile_type == engine::component::TileType::NORMAL) {
                static_stats_.normal_tiles_++;
            }
            if (hasFlag(engine::component::TileType::HAZARD)) {
                static_stats_.hazard_tiles_++;
            }
            if (hasFlag(engine::component::TileType::WATER)) {
                static_stats_.water_tiles_++;
            }
            if (hasFlag(engine::component::TileType::INTERACT)) {
                static_stats_.interact_tiles_++;
            }
            if (hasFlag(engine::component::TileType::ARABLE)) {
                static_stats_.arable_tiles_++;
            }
            if (hasFlag(engine::component::TileType::OCCUPIED)) {
                static_stats_.occupied_tiles_++;
            }
            
            const auto& entities = cell_data.entities_;
            if (!entities.empty()) {
                static_stats_.cells_with_entities_++;
                static_stats_.total_entities_ += entities.size();
            } else if (tile_type == engine::component::TileType::NORMAL) {
                static_stats_.empty_tiles_++;
            }
        }
    }
}

void SpatialIndexDebugPanel::updateDynamicGridStats() {
    const auto& dynamic_grid = spatial_index_manager_.getDynamicGrid();
    const auto& grid_size = dynamic_grid.getGridSize();
    
    dynamic_stats_.total_cells_ = static_cast<size_t>(grid_size.x * grid_size.y);
    dynamic_stats_.total_entities_ = 0;
    dynamic_stats_.aabb_colliders_ = 0;
    dynamic_stats_.circle_colliders_ = 0;
    dynamic_stats_.cells_with_entities_ = 0;

    // 获取当前场景
    engine::scene::Scene* current_scene = scene_manager_.getCurrentScene();
    if (!current_scene) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "当前场景为空");
        return;
    }

    auto& registry = current_scene->getRegistry();
    // 统计实体类型
    auto view = registry.view<engine::component::SpatialIndexTag>();
    for (auto entity : view) {
        dynamic_stats_.total_entities_++;
        if (registry.all_of<engine::component::AABBCollider>(entity)) {
            dynamic_stats_.aabb_colliders_++;
        }
        if (registry.all_of<engine::component::CircleCollider>(entity)) {
            dynamic_stats_.circle_colliders_++;
        }
    }

    // 直接访问网格内部数据统计有实体的单元格
    dynamic_stats_.cells_with_entities_ = dynamic_grid.getUsedCellCount();
}

void SpatialIndexDebugPanel::drawStaticGridInfo() {
    const auto& static_grid = spatial_index_manager_.getStaticGrid();
    const auto& map_size = static_grid.getMapSize();
    const auto& tile_size = static_grid.getTileSize();
    
    ImGui::Text("地图尺寸: %d × %d 瓦片", map_size.x, map_size.y);
    ImGui::Text("瓦片尺寸: %d × %d 像素", tile_size.x, tile_size.y);
    ImGui::Text("总单元格数: %zu", static_cast<size_t>(map_size.x * map_size.y));
    ImGui::Text("网格状态: 已初始化");
}

void SpatialIndexDebugPanel::drawStaticGridStats() {
    float solid_percentage = static_stats_.total_cells_ > 0 
        ? (static_cast<float>(static_stats_.solid_tiles_) / static_cast<float>(static_stats_.total_cells_)) * 100.0f
        : 0.0f;
    
    ImGui::Text("SOLID瓦片统计:");
    ImGui::Indent();
    ImGui::Text("SOLID瓦片总数: %zu", static_stats_.solid_tiles_);
    ImGui::Text("SOLID瓦片占比: %.2f%%", solid_percentage);
    ImGui::Unindent();
    
    ImGui::Separator();
    ImGui::Text("瓦片类型统计:");
    ImGui::Indent();
    ImGui::Text("EMPTY (NORMAL+无实体): %zu", static_stats_.empty_tiles_);
    ImGui::Text("NORMAL: %zu", static_stats_.normal_tiles_);
    ImGui::Text("SOLID: %zu", static_stats_.solid_tiles_);
    ImGui::Text("BLOCK_N: %zu", static_stats_.blocked_n_tiles_);
    ImGui::Text("BLOCK_S: %zu", static_stats_.blocked_s_tiles_);
    ImGui::Text("BLOCK_W: %zu", static_stats_.blocked_w_tiles_);
    ImGui::Text("BLOCK_E: %zu", static_stats_.blocked_e_tiles_);
    ImGui::Text("HAZARD: %zu", static_stats_.hazard_tiles_);
    ImGui::Text("WATER: %zu", static_stats_.water_tiles_);
    ImGui::Text("INTERACT: %zu", static_stats_.interact_tiles_);
    ImGui::Text("ARABLE: %zu", static_stats_.arable_tiles_);
    ImGui::Text("OCCUPIED: %zu", static_stats_.occupied_tiles_);
    ImGui::Unindent();
    
    ImGui::Separator();
    ImGui::Text("实体分布统计:");
    ImGui::Indent();
    ImGui::Text("有实体的单元格数: %zu", static_stats_.cells_with_entities_);
    if (static_stats_.cells_with_entities_ > 0) {
        float avg_entities = static_cast<float>(static_stats_.total_entities_) 
                           / static_cast<float>(static_stats_.cells_with_entities_);
        ImGui::Text("平均每个单元格实体数: %.2f", avg_entities);
    }
    ImGui::Text("总实体数: %zu", static_stats_.total_entities_);
    ImGui::Unindent();
}

void SpatialIndexDebugPanel::drawStaticGridVisualization() {
    ImGui::Checkbox("显示网格线", &show_static_grid_lines_);
    ImGui::Checkbox("显示薄墙（BLOCK_* 边界）", &show_thin_walls_);
    ImGui::Checkbox("高亮SOLID瓦片", &highlight_solid_tiles_);
    ImGui::Checkbox("高亮有实体的单元格", &highlight_entities_cells_);
    
    if (show_static_grid_lines_ || show_thin_walls_ || highlight_solid_tiles_ || highlight_entities_cells_) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), 
                          "注意: 可视化功能需要在游戏视口中实现渲染");
    }
}

void SpatialIndexDebugPanel::drawStaticGridQuery() {
    ImGui::Text("输入瓦片坐标查询:");
    ImGui::InputInt("瓦片 X", &static_query_tile_x_);
    ImGui::InputInt("瓦片 Y", &static_query_tile_y_);
    
    if (ImGui::Button("查询")) {
        const auto& static_grid = spatial_index_manager_.getStaticGrid();
        const auto& map_size = static_grid.getMapSize();
        static_query_has_result_ = true;
        static_query_result_tile_x_ = static_query_tile_x_;
        static_query_result_tile_y_ = static_query_tile_y_;

        const glm::ivec2 coord(static_query_result_tile_x_, static_query_result_tile_y_);
        static_query_coord_valid_ = coord.x >= 0 && coord.x < map_size.x && coord.y >= 0 && coord.y < map_size.y;
        static_query_is_solid_ = false;
        static_query_flags_.clear();
        static_query_entities_.clear();

        if (static_query_coord_valid_) {
            static_query_is_solid_ = static_grid.isSolid(coord);
            const auto tile_type = static_grid.getTileType(coord);
            const auto hasFlag = [&](engine::component::TileType flag) -> bool {
                return engine::component::hasTileFlag(tile_type, flag);
            };

            if (tile_type == engine::component::TileType::NORMAL) {
                static_query_flags_ = "NORMAL";
            } else {
                std::vector<const char*> flags;
                if (hasFlag(engine::component::TileType::BLOCK_N)) flags.push_back("BLOCK_N");
                if (hasFlag(engine::component::TileType::BLOCK_S)) flags.push_back("BLOCK_S");
                if (hasFlag(engine::component::TileType::BLOCK_W)) flags.push_back("BLOCK_W");
                if (hasFlag(engine::component::TileType::BLOCK_E)) flags.push_back("BLOCK_E");
                if (hasFlag(engine::component::TileType::HAZARD)) flags.push_back("HAZARD");
                if (hasFlag(engine::component::TileType::WATER)) flags.push_back("WATER");
                if (hasFlag(engine::component::TileType::INTERACT)) flags.push_back("INTERACT");
                if (hasFlag(engine::component::TileType::ARABLE)) flags.push_back("ARABLE");
                if (hasFlag(engine::component::TileType::OCCUPIED)) flags.push_back("OCCUPIED");

                if (flags.empty()) {
                    static_query_flags_ = "UNKNOWN";
                } else {
                    for (size_t i = 0; i < flags.size(); ++i) {
                        if (i > 0) {
                            static_query_flags_ += ", ";
                        }
                        static_query_flags_ += flags[i];
                    }
                }
            }

            const auto& entities = static_grid.getEntities(coord);
            static_query_entities_.assign(entities.begin(), entities.end());
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("清除")) {
        static_query_has_result_ = false;
        static_query_entities_.clear();
        static_query_flags_.clear();
    }

    if (!static_query_has_result_) {
        return;
    }

    ImGui::Separator();
    ImGui::Text("查询结果: (%d, %d)", static_query_result_tile_x_, static_query_result_tile_y_);

    if (!static_query_coord_valid_) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "无效坐标");
        return;
    }
        
    ImGui::Text("是否为SOLID: %s", static_query_is_solid_ ? "是" : "否");
    ImGui::Text("瓦片标志: %s", static_query_flags_.c_str());
    ImGui::Text("实体数量: %zu", static_query_entities_.size());
    
    if (!static_query_entities_.empty()) {
        engine::scene::Scene* current_scene = scene_manager_.getCurrentScene();
        if (current_scene) {
            auto& registry = current_scene->getRegistry();
            drawEntityTable("StaticEntities", static_query_entities_, registry, false, 10);
        }
    }
}

void SpatialIndexDebugPanel::drawDynamicGridInfo() {
    const auto& dynamic_grid = spatial_index_manager_.getDynamicGrid();
    const auto& grid_size = dynamic_grid.getGridSize();
    const auto& cell_size = dynamic_grid.getCellSize();
    const auto& world_bounds_min = dynamic_grid.getWorldBoundsMin();
    const auto& world_bounds_max = dynamic_grid.getWorldBoundsMax();
    
    ImGui::Text("网格尺寸: %d × %d 单元格", grid_size.x, grid_size.y);
    ImGui::Text("单元格大小: %.1f × %.1f 像素", cell_size.x, cell_size.y);
    ImGui::Text("总单元格数: %zu", static_cast<size_t>(grid_size.x * grid_size.y));
    ImGui::Text("世界边界: (%.1f, %.1f) 到 (%.1f, %.1f)", 
                world_bounds_min.x, world_bounds_min.y, 
                world_bounds_max.x, world_bounds_max.y);
    ImGui::Text("网格状态: 已初始化");
}

void SpatialIndexDebugPanel::drawDynamicGridStats() {
    ImGui::Text("实体统计:");
    ImGui::Indent();
    ImGui::Text("碰撞器实体总数: %zu", dynamic_stats_.total_entities_);
    ImGui::Text("AABB碰撞器: %zu", dynamic_stats_.aabb_colliders_);
    ImGui::Text("Circle碰撞器: %zu", dynamic_stats_.circle_colliders_);
    ImGui::Unindent();
    
    ImGui::Separator();
    ImGui::Text("单元格使用统计:");
    ImGui::Indent();
    ImGui::Text("有实体的单元格数: %zu", dynamic_stats_.cells_with_entities_);
    
    if (dynamic_stats_.total_cells_ > 0) {
        float usage_percentage = (static_cast<float>(dynamic_stats_.cells_with_entities_) 
                                 / static_cast<float>(dynamic_stats_.total_cells_)) * 100.0f;
        ImGui::Text("单元格使用率: %.2f%%", usage_percentage);
    }
    
    if (dynamic_stats_.cells_with_entities_ > 0 && dynamic_stats_.total_entities_ > 0) {
        float avg_entities = static_cast<float>(dynamic_stats_.total_entities_) 
                           / static_cast<float>(dynamic_stats_.cells_with_entities_);
        ImGui::Text("平均每个单元格实体数: %.2f", avg_entities);
    }
    ImGui::Unindent();
}

void SpatialIndexDebugPanel::drawDynamicGridVisualization() {
    ImGui::Checkbox("显示网格线", &show_dynamic_grid_lines_);
    ImGui::Checkbox("高亮有实体的单元格", &highlight_dynamic_cells_);
    ImGui::Checkbox("显示实体边界框", &show_entity_bounds_);
    
    if (show_dynamic_grid_lines_ || highlight_dynamic_cells_ || show_entity_bounds_) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), 
                          "注意: 可视化功能需要在游戏视口中实现渲染");
    }
}

void SpatialIndexDebugPanel::drawDynamicGridQueries() {
    // 获取当前场景
    engine::scene::Scene* current_scene = scene_manager_.getCurrentScene();
    if (!current_scene) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "当前场景为空");
        return;
    }
    auto& registry = current_scene->getRegistry();

    ImGui::Checkbox("显示 candidates（broadphase，仅网格候选）", &dynamic_query_show_candidates_);
    ImGui::SameLine();
    ImGui::TextDisabled("关闭时显示 overlaps（真实相交）");

    // 点查询
    if (ImGui::CollapsingHeader("点查询", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputFloat("世界坐标 X", &dynamic_query_pos_x_);
        ImGui::InputFloat("世界坐标 Y", &dynamic_query_pos_y_);
        
        if (ImGui::Button("查询点")) {
            glm::vec2 pos(dynamic_query_pos_x_, dynamic_query_pos_y_);
            dynamic_point_query_cache_.has_result = true;
            dynamic_point_query_cache_.x = dynamic_query_pos_x_;
            dynamic_point_query_cache_.y = dynamic_query_pos_y_;
            dynamic_point_query_cache_.r = 1.0f;
            dynamic_point_query_cache_.candidates = spatial_index_manager_.queryColliderCandidates(pos, 1.0f);  // 使用很小的半径
            dynamic_point_query_cache_.overlaps = spatial_index_manager_.queryColliders(pos, 1.0f);
        }

        ImGui::SameLine();
        if (ImGui::Button("清除##dynamic_point_query")) {
            dynamic_point_query_cache_.has_result = false;
            dynamic_point_query_cache_.candidates.clear();
            dynamic_point_query_cache_.overlaps.clear();
        }

        if (dynamic_point_query_cache_.has_result) {
            const auto& candidates = dynamic_point_query_cache_.candidates;
            const auto& overlaps = dynamic_point_query_cache_.overlaps;
            const auto& entities = dynamic_query_show_candidates_ ? candidates : overlaps;

            ImGui::Separator();
            ImGui::Text("最后一次查询点: (%.1f, %.1f)", dynamic_point_query_cache_.x, dynamic_point_query_cache_.y);
            ImGui::Text("查询结果: overlaps(narrowphase)=%zu, candidates(broadphase)=%zu", overlaps.size(), candidates.size());

            if (!entities.empty()) {
                drawEntityTable("PointQueryEntities", entities, registry, true);
            } else {
                ImGui::TextDisabled("无匹配实体");
            }
        }
    }
    
    // 矩形查询
    if (ImGui::CollapsingHeader("矩形查询")) {
        ImGui::InputFloat("矩形 X", &dynamic_query_rect_x_);
        ImGui::InputFloat("矩形 Y", &dynamic_query_rect_y_);
        ImGui::InputFloat("宽度 W", &dynamic_query_rect_w_);
        ImGui::InputFloat("高度 H", &dynamic_query_rect_h_);
        
        if (ImGui::Button("查询矩形")) {
            engine::utils::Rect rect(
                glm::vec2(dynamic_query_rect_x_, dynamic_query_rect_y_),
                glm::vec2(dynamic_query_rect_w_, dynamic_query_rect_h_)
            );
            dynamic_rect_query_cache_.has_result = true;
            dynamic_rect_query_cache_.x = dynamic_query_rect_x_;
            dynamic_rect_query_cache_.y = dynamic_query_rect_y_;
            dynamic_rect_query_cache_.w = dynamic_query_rect_w_;
            dynamic_rect_query_cache_.h = dynamic_query_rect_h_;
            dynamic_rect_query_cache_.candidates = spatial_index_manager_.queryColliderCandidates(rect);
            dynamic_rect_query_cache_.overlaps = spatial_index_manager_.queryColliders(rect);
        }

        ImGui::SameLine();
        if (ImGui::Button("清除##dynamic_rect_query")) {
            dynamic_rect_query_cache_.has_result = false;
            dynamic_rect_query_cache_.candidates.clear();
            dynamic_rect_query_cache_.overlaps.clear();
        }

        if (dynamic_rect_query_cache_.has_result) {
            const auto& candidates = dynamic_rect_query_cache_.candidates;
            const auto& overlaps = dynamic_rect_query_cache_.overlaps;
            const auto& entities = dynamic_query_show_candidates_ ? candidates : overlaps;

            ImGui::Separator();
            ImGui::Text("最后一次查询矩形: (%.1f, %.1f) %.1fx%.1f",
                        dynamic_rect_query_cache_.x,
                        dynamic_rect_query_cache_.y,
                        dynamic_rect_query_cache_.w,
                        dynamic_rect_query_cache_.h);
            ImGui::Text("查询结果: overlaps(narrowphase)=%zu, candidates(broadphase)=%zu", overlaps.size(), candidates.size());

            if (!entities.empty()) {
                drawEntityTable("RectQueryEntities", entities, registry);
            } else {
                ImGui::TextDisabled("无匹配实体");
            }
        }
    }
    
    // 圆形查询
    if (ImGui::CollapsingHeader("圆形查询")) {
        ImGui::InputFloat("圆心 X", &dynamic_query_circle_x_);
        ImGui::InputFloat("圆心 Y", &dynamic_query_circle_y_);
        ImGui::InputFloat("半径 R", &dynamic_query_circle_r_);
        
        if (ImGui::Button("查询圆形")) {
            glm::vec2 center(dynamic_query_circle_x_, dynamic_query_circle_y_);
            dynamic_circle_query_cache_.has_result = true;
            dynamic_circle_query_cache_.x = dynamic_query_circle_x_;
            dynamic_circle_query_cache_.y = dynamic_query_circle_y_;
            dynamic_circle_query_cache_.r = dynamic_query_circle_r_;
            dynamic_circle_query_cache_.candidates = spatial_index_manager_.queryColliderCandidates(center, dynamic_query_circle_r_);
            dynamic_circle_query_cache_.overlaps = spatial_index_manager_.queryColliders(center, dynamic_query_circle_r_);
        }

        ImGui::SameLine();
        if (ImGui::Button("清除##dynamic_circle_query")) {
            dynamic_circle_query_cache_.has_result = false;
            dynamic_circle_query_cache_.candidates.clear();
            dynamic_circle_query_cache_.overlaps.clear();
        }

        if (dynamic_circle_query_cache_.has_result) {
            const auto& candidates = dynamic_circle_query_cache_.candidates;
            const auto& overlaps = dynamic_circle_query_cache_.overlaps;
            const auto& entities = dynamic_query_show_candidates_ ? candidates : overlaps;

            ImGui::Separator();
            ImGui::Text("最后一次查询圆形: (%.1f, %.1f) r=%.1f",
                        dynamic_circle_query_cache_.x,
                        dynamic_circle_query_cache_.y,
                        dynamic_circle_query_cache_.r);
            ImGui::Text("查询结果: overlaps(narrowphase)=%zu, candidates(broadphase)=%zu", overlaps.size(), candidates.size());

            if (!entities.empty()) {
                drawEntityTable("CircleQueryEntities", entities, registry);
            } else {
                ImGui::TextDisabled("无匹配实体");
            }
        }
    }
}

void SpatialIndexDebugPanel::drawCollisionQueries() {
    // 获取当前场景
    engine::scene::Scene* current_scene = scene_manager_.getCurrentScene();
    if (!current_scene) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "当前场景为空");
        return;
    }
    auto& registry = current_scene->getRegistry();

    // 矩形碰撞检测
    if (ImGui::CollapsingHeader("矩形碰撞检测", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputFloat("矩形 X", &collision_query_rect_x_);
        ImGui::InputFloat("矩形 Y", &collision_query_rect_y_);
        ImGui::InputFloat("宽度 W", &collision_query_rect_w_);
        ImGui::InputFloat("高度 H", &collision_query_rect_h_);
        
        if (ImGui::Button("检测矩形碰撞")) {
            engine::utils::Rect rect(
                glm::vec2(collision_query_rect_x_, collision_query_rect_y_),
                glm::vec2(collision_query_rect_w_, collision_query_rect_h_)
            );
            auto result = spatial_index_manager_.checkCollision(rect);

            collision_rect_query_cache_.has_result = true;
            collision_rect_query_cache_.x = collision_query_rect_x_;
            collision_rect_query_cache_.y = collision_query_rect_y_;
            collision_rect_query_cache_.w = collision_query_rect_w_;
            collision_rect_query_cache_.h = collision_query_rect_h_;
            collision_rect_query_cache_.has_static_collision = result.has_static_collision;
            collision_rect_query_cache_.dynamic_colliders = std::move(result.dynamic_colliders);
        }

        ImGui::SameLine();
        if (ImGui::Button("清除##collision_rect_query")) {
            collision_rect_query_cache_.has_result = false;
            collision_rect_query_cache_.dynamic_colliders.clear();
        }

        if (collision_rect_query_cache_.has_result) {
            const auto& entities = collision_rect_query_cache_.dynamic_colliders;
            ImGui::Separator();
            ImGui::Text("最后一次检测矩形: (%.1f, %.1f) %.1fx%.1f",
                        collision_rect_query_cache_.x,
                        collision_rect_query_cache_.y,
                        collision_rect_query_cache_.w,
                        collision_rect_query_cache_.h);
            ImGui::Text("静态碰撞: %s", collision_rect_query_cache_.has_static_collision ? "是" : "否");
            ImGui::Text("动态碰撞器数量: %zu", entities.size());

            if (!entities.empty()) {
                drawEntityTable("CollisionEntities", entities, registry);
            } else {
                ImGui::TextDisabled("无动态碰撞器");
            }
        }
    }
    
    // 圆形碰撞检测
    if (ImGui::CollapsingHeader("圆形碰撞检测")) {
        ImGui::InputFloat("圆心 X", &collision_query_circle_x_);
        ImGui::InputFloat("圆心 Y", &collision_query_circle_y_);
        ImGui::InputFloat("半径 R", &collision_query_circle_r_);
        
        if (ImGui::Button("检测圆形碰撞")) {
            glm::vec2 center(collision_query_circle_x_, collision_query_circle_y_);
            auto result = spatial_index_manager_.checkCollision(center, collision_query_circle_r_);

            collision_circle_query_cache_.has_result = true;
            collision_circle_query_cache_.x = collision_query_circle_x_;
            collision_circle_query_cache_.y = collision_query_circle_y_;
            collision_circle_query_cache_.r = collision_query_circle_r_;
            collision_circle_query_cache_.has_static_collision = result.has_static_collision;
            collision_circle_query_cache_.dynamic_colliders = std::move(result.dynamic_colliders);
        }

        ImGui::SameLine();
        if (ImGui::Button("清除##collision_circle_query")) {
            collision_circle_query_cache_.has_result = false;
            collision_circle_query_cache_.dynamic_colliders.clear();
        }

        if (collision_circle_query_cache_.has_result) {
            const auto& entities = collision_circle_query_cache_.dynamic_colliders;
            ImGui::Separator();
            ImGui::Text("最后一次检测圆形: (%.1f, %.1f) r=%.1f",
                        collision_circle_query_cache_.x,
                        collision_circle_query_cache_.y,
                        collision_circle_query_cache_.r);
            ImGui::Text("静态碰撞: %s", collision_circle_query_cache_.has_static_collision ? "是" : "否");
            ImGui::Text("动态碰撞器数量: %zu", entities.size());

            if (!entities.empty()) {
                drawEntityTable("CircleCollisionEntities", entities, registry);
            } else {
                ImGui::TextDisabled("无动态碰撞器");
            }
        }
    }
}

} // namespace engine::debug
