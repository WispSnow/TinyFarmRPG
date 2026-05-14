#include "game/ui/map_tab_content.h"

#include "engine/component/transform_component.h"
#include "game/ui/map_coordinate_mapper.h"
#include "game/world/world_state.h"

#include <RmlUi/Core/DataTypeRegister.h>
#include <entt/entity/registry.hpp>
#include <glm/common.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>

namespace game::ui {
namespace {

[[nodiscard]] std::string formatDp(float value) {
    std::array<char, 32> buffer{};
    const int count = std::snprintf(buffer.data(), buffer.size(), "%.2fdp", value);
    if (count <= 0) {
        return "0dp";
    }
    return std::string{buffer.data(), static_cast<std::size_t>(std::min(count, static_cast<int>(buffer.size() - 1U)))};
}

[[nodiscard]] std::string humanizeMapName(std::string_view map_name) {
    std::string label{map_name};
    std::replace(label.begin(), label.end(), '_', ' ');
    std::replace(label.begin(), label.end(), '-', ' ');

    bool capitalize = true;
    for (char& ch : label) {
        const unsigned char uch = static_cast<unsigned char>(ch);
        if (std::isspace(uch)) {
            capitalize = true;
            continue;
        }
        if (capitalize && std::isalpha(uch)) {
            ch = static_cast<char>(std::toupper(uch));
        }
        capitalize = false;
    }
    return label.empty() ? "Map" : label;
}

[[nodiscard]] const game::world::MapInfo* findMapInfo(const game::world::WorldState* world_state,
                                                      entt::id_type map_id) {
    if (!world_state || map_id == entt::null) {
        return nullptr;
    }
    const game::world::MapState* map_state = world_state->getMapState(map_id);
    return map_state ? &map_state->info : nullptr;
}

} // namespace

// Phase 1 has no custom Rml struct/array types. Later marker phases can register their view models here.
bool registerMapTabDataTypes(Rml::DataModelConstructor& constructor) {
    (void)constructor;
    return true;
}

MapTabViewState buildMapTabViewState(const entt::registry& registry,
                                     const entt::entity player,
                                     const game::world::WorldState* world_state,
                                     const entt::id_type map_id,
                                     const MapTabPreviewInput& preview) {
    MapTabViewState state{};
    const game::world::MapInfo* map_info = findMapInfo(world_state, map_id);
    if (!map_info) {
        state.map_status_text = "No map data";
        return state;
    }

    state.map_title = humanizeMapName(map_info->name);

    const bool preview_valid = !preview.source_uri.empty() && preview.width > 0 && preview.height > 0;
    if (!preview_valid) {
        state.map_status_text = "No map data";
        return state;
    }

    const MapPreviewLayout layout = computeMapPreviewLayout(
        glm::vec2{static_cast<float>(preview.width), static_cast<float>(preview.height)},
        glm::vec2{MAP_TAB_PREVIEW_FRAME_WIDTH, MAP_TAB_PREVIEW_FRAME_HEIGHT});

    state.map_preview_src = preview.source_uri;
    state.map_preview_left = formatDp(layout.content_position.x);
    state.map_preview_top = formatDp(layout.content_position.y);
    state.map_preview_width = formatDp(layout.content_size.x);
    state.map_preview_height = formatDp(layout.content_size.y);
    state.has_map_preview = true;
    state.map_status_text = "Current Position";

    if (player == entt::null || !registry.valid(player)) {
        return state;
    }

    const auto* transform = registry.try_get<engine::component::TransformComponent>(player);
    if (!transform) {
        return state;
    }

    const glm::vec2 map_local_position = glm::clamp(
        transform->position_,
        glm::vec2{0.0F, 0.0F},
        glm::vec2{static_cast<float>(preview.width), static_cast<float>(preview.height)});
    const glm::vec2 marker_top_left = mapMarkerTopLeft(
        map_local_position,
        layout,
        glm::vec2{MAP_TAB_PLAYER_MARKER_SIZE, MAP_TAB_PLAYER_MARKER_SIZE});
    state.player_marker_left = formatDp(marker_top_left.x);
    state.player_marker_top = formatDp(marker_top_left.y);
    state.has_player_marker = true;
    return state;
}

MapTabContent::MapTabContent(engine::ui::rmlui::RmlDocumentController& document_controller,
                             entt::registry& game_registry,
                             const entt::entity player,
                             const game::world::WorldState* world_state,
                             engine::ui::rmlui::RmlGeneratedImageRegistry* generated_images)
    : document_controller_(document_controller),
      game_registry_(game_registry),
      player_(player),
      world_state_(world_state),
      generated_images_(generated_images) {
}

bool MapTabContent::bindModel(Rml::DataModelConstructor& constructor) {
    if (!constructor.Bind("map_title", &map_title_) ||
        !constructor.Bind("map_preview_src", &map_preview_src_) ||
        !constructor.Bind("map_preview_left", &map_preview_left_) ||
        !constructor.Bind("map_preview_top", &map_preview_top_) ||
        !constructor.Bind("map_preview_width", &map_preview_width_) ||
        !constructor.Bind("map_preview_height", &map_preview_height_) ||
        !constructor.Bind("player_marker_left", &player_marker_left_) ||
        !constructor.Bind("player_marker_top", &player_marker_top_) ||
        !constructor.Bind("map_status_text", &map_status_text_) ||
        !constructor.Bind("has_map_preview", &has_map_preview_) ||
        !constructor.Bind("has_player_marker", &has_player_marker_)) {
        spdlog::error("MapTabContent: failed to bind map tab data model.");
        return false;
    }

    return true;
}

void MapTabContent::onActivated() {
    syncViewState();
}

void MapTabContent::onDeactivated() {
}

void MapTabContent::update(const float /*delta_time*/) {
}

bool MapTabContent::onCancel() {
    return false;
}

MapTabPreviewInput MapTabContent::buildPreviewInput(const entt::id_type map_id) {
    MapTabPreviewInput input{};
    const game::world::MapInfo* map_info = findMapInfo(world_state_, map_id);
    if (!map_info || map_info->file_path.empty() || !generated_images_) {
        return input;
    }
    if (preview_map_id_ == map_id && !current_preview_input_.source_uri.empty() &&
        current_preview_input_.width > 0 && current_preview_input_.height > 0) {
        return current_preview_input_;
    }

    MapPreviewBuildResult result = preview_builder_.buildPreview(map_info->name, map_info->file_path);
    if (!result.valid()) {
        return input;
    }

    input.source_uri = "generated://map-preview/" + map_info->name;
    input.width = result.map_pixel_size.x;
    input.height = result.map_pixel_size.y;
    preview_registration_ = generated_images_->registerImage(input.source_uri, std::move(result.image));
    if (!preview_registration_.valid()) {
        input = {};
        preview_map_id_ = entt::null;
        current_preview_input_ = {};
        return input;
    }
    preview_map_id_ = map_id;
    current_preview_input_ = input;
    return input;
}

void MapTabContent::syncViewState() {
    const entt::id_type map_id = world_state_ ? world_state_->getCurrentMap() : entt::null;
    MapTabViewState state = buildMapTabViewState(game_registry_, player_, world_state_, map_id, buildPreviewInput(map_id));

    map_title_ = std::move(state.map_title);
    map_preview_src_ = std::move(state.map_preview_src);
    map_preview_left_ = std::move(state.map_preview_left);
    map_preview_top_ = std::move(state.map_preview_top);
    map_preview_width_ = std::move(state.map_preview_width);
    map_preview_height_ = std::move(state.map_preview_height);
    player_marker_left_ = std::move(state.player_marker_left);
    player_marker_top_ = std::move(state.player_marker_top);
    map_status_text_ = std::move(state.map_status_text);
    has_map_preview_ = state.has_map_preview;
    has_player_marker_ = state.has_player_marker;

    document_controller_.markDirty("map_title");
    document_controller_.markDirty("map_preview_src");
    document_controller_.markDirty("map_preview_left");
    document_controller_.markDirty("map_preview_top");
    document_controller_.markDirty("map_preview_width");
    document_controller_.markDirty("map_preview_height");
    document_controller_.markDirty("player_marker_left");
    document_controller_.markDirty("player_marker_top");
    document_controller_.markDirty("map_status_text");
    document_controller_.markDirty("has_map_preview");
    document_controller_.markDirty("has_player_marker");
}

} // namespace game::ui
