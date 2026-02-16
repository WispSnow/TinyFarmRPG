#include "map_inspector_debug_panel.h"

#include "game/world/map_manager.h"
#include "game/world/world_state.h"
#include "game/component/map_component.h"

#include "engine/component/light_component.h"
#include "engine/component/name_component.h"
#include "engine/component/tilelayer_component.h"
#include "engine/loader/tiled_diagnostics.h"
#include "engine/loader/tiled_json_cache.h"
#include "engine/spatial/spatial_index_manager.h"

#include <imgui.h>
#include <ranges>

namespace game::debug {

namespace {
struct TileFlagStats {
    int total_cells{0};
    int non_normal{0};
    int solid{0};
    int block_n{0};
    int block_s{0};
    int block_w{0};
    int block_e{0};
    int hazard{0};
    int water{0};
    int interact{0};
    int arable{0};
    int occupied{0};
};

[[nodiscard]] TileFlagStats computeTileFlagStats(const engine::spatial::StaticTileGrid& grid) {
    TileFlagStats stats{};
    const auto map_size = grid.getMapSize();
    stats.total_cells = map_size.x * map_size.y;

    for (int y = 0; y < map_size.y; ++y) {
        for (int x = 0; x < map_size.x; ++x) {
            const auto type = grid.getTileType({x, y});
            if (type != engine::component::TileType::NORMAL) {
                stats.non_normal += 1;
            }
            if (engine::component::hasAllTileFlags(type, engine::component::TileType::SOLID)) {
                stats.solid += 1;
            }
            if (engine::component::hasTileFlag(type, engine::component::TileType::BLOCK_N)) {
                stats.block_n += 1;
            }
            if (engine::component::hasTileFlag(type, engine::component::TileType::BLOCK_S)) {
                stats.block_s += 1;
            }
            if (engine::component::hasTileFlag(type, engine::component::TileType::BLOCK_W)) {
                stats.block_w += 1;
            }
            if (engine::component::hasTileFlag(type, engine::component::TileType::BLOCK_E)) {
                stats.block_e += 1;
            }
            if (engine::component::hasTileFlag(type, engine::component::TileType::HAZARD)) {
                stats.hazard += 1;
            }
            if (engine::component::hasTileFlag(type, engine::component::TileType::WATER)) {
                stats.water += 1;
            }
            if (engine::component::hasTileFlag(type, engine::component::TileType::INTERACT)) {
                stats.interact += 1;
            }
            if (engine::component::hasTileFlag(type, engine::component::TileType::ARABLE)) {
                stats.arable += 1;
            }
            if (engine::component::hasTileFlag(type, engine::component::TileType::OCCUPIED)) {
                stats.occupied += 1;
            }
        }
    }

    return stats;
}
} // namespace

MapInspectorDebugPanel::MapInspectorDebugPanel(const game::world::MapManager& map_manager,
                                               const game::world::WorldState& world_state,
                                               entt::registry& registry,
                                               engine::spatial::SpatialIndexManager& spatial_index_manager)
    : map_manager_(map_manager),
      world_state_(world_state),
      registry_(registry),
      spatial_index_manager_(spatial_index_manager) {
}

std::string_view MapInspectorDebugPanel::name() const {
    return "Map Inspector";
}

void MapInspectorDebugPanel::draw(bool& is_open) {
    if (!is_open) {
        return;
    }

    if (!ImGui::Begin("Map Inspector", &is_open, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    const auto current_map_id = map_manager_.currentMapId();
    const auto* map_state = world_state_.getMapState(current_map_id);

    ImGui::Text("current_map_id: %u", static_cast<unsigned>(current_map_id));
    if (map_state) {
        ImGui::Text("name: %s", map_state->info.name.empty() ? "(unknown)" : map_state->info.name.c_str());
        ImGui::Text("file: %s", map_state->info.file_path.empty() ? "(unknown)" : map_state->info.file_path.c_str());
        ImGui::Text("in_world: %s", map_state->info.in_world ? "true" : "false");
        ImGui::Text("size_px: %d x %d", map_state->info.size_px.x, map_state->info.size_px.y);
        if (map_state->info.ambient_override) {
            const auto& ambient = *map_state->info.ambient_override;
            ImGui::Text("ambient_override: %.2f, %.2f, %.2f", ambient.r, ambient.g, ambient.b);
        }
    } else {
        ImGui::Text("map_state: (missing)");
    }

    ImGui::Separator();

    if (ImGui::CollapsingHeader("Loading Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        const auto& settings = map_manager_.loadingSettings();
        ImGui::Text("config: %s", settings.source_path.empty() ? "(unknown)" : settings.source_path.c_str());
        ImGui::Text("preload_mode: %s", game::world::MapLoadingSettings::toString(settings.preload_mode).data());
        ImGui::Text("log_timings: %s", settings.log_timings ? "true" : "false");
        ImGui::Text("preloaded_maps: %zu", map_manager_.preloadedMapCount());
        ImGui::Text("current_map_preloaded: %s", map_manager_.isMapPreloaded(current_map_id) ? "true" : "false");
    }

    if (ImGui::CollapsingHeader("Tile Layers", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto view = registry_.view<engine::component::TileLayerComponent, engine::component::NameComponent>();
        std::size_t count = 0;
        for (auto entity : view) {
            (void)entity;
            count += 1;
        }
        ImGui::Text("tile_layer_entities: %zu", count);

        for (auto entity : view) {
            const auto& name = view.get<engine::component::NameComponent>(entity);
            const auto& layer = view.get<engine::component::TileLayerComponent>(entity);
            ImGui::BulletText("%s: tiles=%zu (map=%dx%d, tile=%dx%d)",
                              name.name_.empty() ? "(unnamed)" : name.name_.c_str(),
                              layer.tiles_.size(),
                              layer.map_size_.x,
                              layer.map_size_.y,
                              layer.tile_size_.x,
                              layer.tile_size_.y);
        }
    }

    if (ImGui::CollapsingHeader("Object Counts", ImGuiTreeNodeFlags_DefaultOpen)) {
        // 对任意 ECS view 求实体数量，返回 size_t；%zu 表示按无符号十进制打印 size_t
        const auto countView = [](const auto& view) -> std::size_t {
            return static_cast<std::size_t>(std::ranges::distance(view));
        };

        ImGui::Text("map_triggers: %zu", countView(registry_.view<game::component::MapTrigger>()));
        ImGui::Text("rest_areas: %zu", countView(registry_.view<game::component::RestArea>()));

        ImGui::Text("point_lights: %zu", countView(registry_.view<engine::component::PointLightComponent>()));
        ImGui::Text("spot_lights: %zu", countView(registry_.view<engine::component::SpotLightComponent>()));
        ImGui::Text("emissive_rects: %zu", countView(registry_.view<engine::component::EmissiveRectComponent>()));
    }

    if (ImGui::CollapsingHeader("Tile Flags (Static Grid)", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (!spatial_index_manager_.isInitialized()) {
            ImGui::Text("SpatialIndexManager: (not initialized)");
        } else {
            const auto& grid = spatial_index_manager_.getStaticGrid();
            const auto map_size = grid.getMapSize();
            const auto tile_size = grid.getTileSize();
            ImGui::Text("map: %d x %d (tile: %d x %d)", map_size.x, map_size.y, tile_size.x, tile_size.y);

            const TileFlagStats stats = computeTileFlagStats(grid);
            ImGui::Text("cells: %d", stats.total_cells);
            ImGui::Text("non_normal: %d", stats.non_normal);
            ImGui::Separator();
            ImGui::Text("SOLID: %d", stats.solid);
            ImGui::Text("BLOCK_N/S/W/E: %d / %d / %d / %d", stats.block_n, stats.block_s, stats.block_w, stats.block_e);
            ImGui::Text("HAZARD: %d", stats.hazard);
            ImGui::Text("WATER: %d", stats.water);
            ImGui::Text("INTERACT: %d", stats.interact);
            ImGui::Text("ARABLE: %d", stats.arable);
            ImGui::Text("OCCUPIED: %d", stats.occupied);
        }
    }

    if (ImGui::CollapsingHeader("Tiled JSON Cache", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("level_json_cache: %zu / %zu",
                    engine::loader::tiled::levelJsonCacheSize(),
                    engine::loader::tiled::LEVEL_JSON_CACHE_MAX_ENTRIES);
        ImGui::Text("tileset_json_cache: %zu / %zu",
                    engine::loader::tiled::tilesetJsonCacheSize(),
                    engine::loader::tiled::TILESET_JSON_CACHE_MAX_ENTRIES);
        if (ImGui::Button("Clear JSON cache")) {
            engine::loader::tiled::clearJsonCache();
        }
    }

    if (ImGui::CollapsingHeader("Unknown Tokens", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button("Clear unknown token report")) {
            engine::loader::tiled::diagnostics::clearAll();
        }

        const auto& tile_flags = engine::loader::tiled::diagnostics::unknownTileFlagTokens();
        const auto& object_types = engine::loader::tiled::diagnostics::unknownObjectTypes();
        const auto& light_names = engine::loader::tiled::diagnostics::unknownLightNames();

        ImGui::Separator();
        ImGui::TextUnformatted("tile_flag");
        if (tile_flags.empty()) {
            ImGui::Text("(none)");
        } else {
            for (const auto& entry : tile_flags) {
                ImGui::BulletText("[%dx] %s tile=%d token=%s",
                                  entry.count,
                                  entry.tileset_path.c_str(),
                                  entry.tile_local_id,
                                  entry.token.c_str());
            }
        }

        ImGui::Separator();
        ImGui::TextUnformatted("object.type");
        if (object_types.empty()) {
            ImGui::Text("(none)");
        } else {
            for (const auto& entry : object_types) {
                ImGui::BulletText("[%dx] type=%s name=%s point=%s",
                                  entry.count,
                                  entry.type.c_str(),
                                  entry.name.c_str(),
                                  entry.point ? "true" : "false");
            }
        }

        ImGui::Separator();
        ImGui::TextUnformatted("light.name");
        if (light_names.empty()) {
            ImGui::Text("(none)");
        } else {
            for (const auto& entry : light_names) {
                ImGui::BulletText("[%dx] name=%s point=%s",
                                  entry.count,
                                  entry.name.c_str(),
                                  entry.point ? "true" : "false");
            }
        }
    }

    ImGui::End();
}

} // namespace game::debug
