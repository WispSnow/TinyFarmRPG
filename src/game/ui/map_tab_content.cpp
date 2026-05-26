#include "game/ui/map_tab_content.h"

#include "engine/component/transform_component.h"
#include "game/component/quest_log_component.h"
#include "game/data/quest_catalog.h"
#include "game/data/rpg_catalog.h"
#include "game/data/rpg_data.h"
#include "game/data/shop_catalog.h"
#include "game/data/shop_data.h"
#include "game/runtime/service_lookup.h"
#include "game/ui/localized_text.h"
#include "game/ui/map_coordinate_mapper.h"
#include "game/ui/slot_grid_support.h"
#include "game/ui/text_utils.h"
#include "game/world/world_state.h"

#include <RmlUi/Core/DataTypeRegister.h>
#include <RmlUi/Core/Event.h>
#include <entt/entity/registry.hpp>
#include <glm/common.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <string_view>
#include <utility>

namespace game::ui {
namespace {

using MapMarkerViewModels = std::vector<MapMarkerViewModel>;

[[nodiscard]] std::string formatDp(float value) {
    std::array<char, 32> buffer{};
    const int count = std::snprintf(buffer.data(), buffer.size(), "%.2fdp", value);
    if (count <= 0) {
        return "0dp";
    }
    return std::string{buffer.data(), static_cast<std::size_t>(std::min(count, static_cast<int>(buffer.size() - 1U)))};
}

[[nodiscard]] std::string formatWholeDp(float value) {
    return std::to_string(static_cast<int>(std::lround(value))) + "dp";
}

[[nodiscard]] const game::world::MapInfo* findMapInfo(const game::world::WorldState* world_state,
                                                      entt::id_type map_id) {
    if (!world_state || map_id == entt::null) {
        return nullptr;
    }
    const game::world::MapState* map_state = world_state->getMapState(map_id);
    return map_state ? &map_state->info : nullptr;
}

[[nodiscard]] bool registerMapMarkerViewModelType(Rml::DataModelConstructor& constructor) {
    if (auto handle = constructor.RegisterStruct<MapMarkerViewModel>()) {
        handle.RegisterMember("marker_index", &MapMarkerViewModel::marker_index);
        handle.RegisterMember("kind", &MapMarkerViewModel::kind);
        handle.RegisterMember("icon_decorator", &MapMarkerViewModel::icon_decorator);
        handle.RegisterMember("left", &MapMarkerViewModel::left);
        handle.RegisterMember("top", &MapMarkerViewModel::top);
        handle.RegisterMember("width", &MapMarkerViewModel::width);
        handle.RegisterMember("height", &MapMarkerViewModel::height);
        handle.RegisterMember("z_index", &MapMarkerViewModel::z_index);
        handle.RegisterMember("title", &MapMarkerViewModel::title);
        handle.RegisterMember("type_label", &MapMarkerViewModel::type_label);
        handle.RegisterMember("description", &MapMarkerViewModel::description);
        handle.RegisterMember("is_selected", &MapMarkerViewModel::is_selected);
        return true;
    }
    return false;
}

[[nodiscard]] std::string localizedMapName(const game::runtime::LocalizationService* localization,
                                           const std::string_view map_name) {
    const std::string key = "map." + std::string{map_name} + ".name";
    if (localization && localization->hasText(key)) {
        return localization->tr(key);
    }
    return humanizeMapName(map_name);
}

[[nodiscard]] const char* markerKindString(const MapObjectMarkerKind kind) {
    switch (kind) {
        case MapObjectMarkerKind::Shop:
            return "shop";
        case MapObjectMarkerKind::Rest:
            return "rest";
        case MapObjectMarkerKind::Npc:
            return "npc";
    }
    return "npc";
}

[[nodiscard]] std::string markerKindTypeLabel(const MapObjectMarkerKind kind,
                                              const game::runtime::LocalizationService* localization) {
    switch (kind) {
        case MapObjectMarkerKind::Shop:
            return game::ui::localizeTextOrFallback(localization, "map.type.shop", "Shop");
        case MapObjectMarkerKind::Rest:
            return game::ui::localizeTextOrFallback(localization, "map.type.rest", "Rest Point");
        case MapObjectMarkerKind::Npc:
            return game::ui::localizeTextOrFallback(localization, "map.type.npc", "NPC");
    }
    return game::ui::localizeTextOrFallback(localization, "map.type.place", "Place");
}

[[nodiscard]] const char* markerDecorator(const MapObjectMarkerKind kind) {
    switch (kind) {
        case MapObjectMarkerKind::Shop:
            return "image(map-marker-shop)";
        case MapObjectMarkerKind::Rest:
            return "image(map-marker-rest)";
        case MapObjectMarkerKind::Npc:
            return "image(map-marker-npc)";
    }
    return "image(map-marker-npc)";
}

[[nodiscard]] int markerBaseZIndex(const MapObjectMarkerKind kind) {
    switch (kind) {
        case MapObjectMarkerKind::Shop:
            return 30;
        case MapObjectMarkerKind::Rest:
            return 20;
        case MapObjectMarkerKind::Npc:
            return 10;
    }
    return 10;
}

void populateShopDetail(MapMarkerViewModel& marker,
                        const MapObjectMarker& source,
                        const game::data::ShopCatalog* shop_catalog,
                        const game::runtime::LocalizationService* localization) {
    const auto* shop = shop_catalog && !source.shop_id.empty() ? shop_catalog->findShop(source.shop_id) : nullptr;
    if (shop) {
        marker.title = shop->title_.empty() ? source.shop_id : game::ui::tryLocalize(localization, shop->title_);
        marker.description = shop->greeting_.empty()
                                 ? game::ui::localizeTextOrFallback(localization, "map.shop.default_description", "Buy and sell supplies.")
                                 : game::ui::tryLocalize(localization, shop->greeting_);
        return;
    }
    marker.title = !source.object_name.empty() ? humanizeId(source.object_name) : humanizeId(source.shop_id);
    marker.description = game::ui::localizeTextOrFallback(localization, "map.shop.default_description", "Buy and sell supplies.");
    if (!source.shop_id.empty()) {
        spdlog::warn("MapTabContent: shop marker references missing shop_id='{}'.", source.shop_id);
    }
}

void populateNpcDetail(MapMarkerViewModel& marker,
                       const MapObjectMarker& source,
                       const game::data::RpgCatalog* rpg_catalog,
                       const game::runtime::LocalizationService* localization) {
    const auto* actor = rpg_catalog && !source.recruit_actor_id.empty()
                            ? rpg_catalog->findActor(source.recruit_actor_id)
                            : nullptr;
    if (actor) {
        marker.title = actor->display_name_.empty()
                           ? source.recruit_actor_id
                           : game::ui::tryLocalize(localization, actor->display_name_);
    } else {
        marker.title = !source.object_name.empty() ? humanizeId(source.object_name) : humanizeId(source.recruit_actor_id);
        if (!source.recruit_actor_id.empty()) {
            spdlog::warn("MapTabContent: npc marker references missing recruit_actor_id='{}'.", source.recruit_actor_id);
        }
    }
    marker.description = game::ui::localizeTextOrFallback(localization, "map.npc.default_description", "Talk to this person.");
}

void populateObjectMarkerDetail(MapMarkerViewModel& marker,
                                const MapObjectMarker& source,
                                const game::data::ShopCatalog* shop_catalog,
                                const game::data::RpgCatalog* rpg_catalog,
                                const game::runtime::LocalizationService* localization) {
    marker.type_label = markerKindTypeLabel(source.kind, localization);
    switch (source.kind) {
        case MapObjectMarkerKind::Shop:
            populateShopDetail(marker, source, shop_catalog, localization);
            break;
        case MapObjectMarkerKind::Rest:
            marker.title = source.object_name.empty()
                               ? game::ui::localizeTextOrFallback(localization, "map.rest.title", "Rest Point")
                               : humanizeId(source.object_name);
            marker.description = game::ui::localizeTextOrFallback(localization, "map.rest.description", "Recover and pass time.");
            break;
        case MapObjectMarkerKind::Npc:
            populateNpcDetail(marker, source, rpg_catalog, localization);
            break;
    }
}

[[nodiscard]] MapMarkerViewModel makePlayerMarkerViewModel(const int marker_index,
                                                           const glm::vec2 map_local_position,
                                                           const MapPreviewLayout& layout,
                                                           const std::string_view map_title,
                                                           const bool selected,
                                                           const game::runtime::LocalizationService* localization) {
    const float marker_size = selected ? MAP_TAB_MARKER_SELECTED_SIZE : MAP_TAB_MARKER_SIZE;
    const glm::vec2 top_left = mapMarkerBottomCenterTopLeft(
        map_local_position,
        layout,
        glm::vec2{MAP_TAB_PREVIEW_FRAME_WIDTH, MAP_TAB_PREVIEW_FRAME_HEIGHT},
        glm::vec2{marker_size, marker_size});

    MapMarkerViewModel marker{};
    marker.marker_index = marker_index;
    marker.kind = "player";
    marker.icon_decorator = "image(map-marker-player)";
    marker.left = formatWholeDp(top_left.x);
    marker.top = formatWholeDp(top_left.y);
    marker.width = formatWholeDp(marker_size);
    marker.height = formatWholeDp(marker_size);
    marker.z_index = selected ? 100 : 50;
    marker.normal_top_left = mapMarkerBottomCenterTopLeft(
        map_local_position,
        layout,
        glm::vec2{MAP_TAB_PREVIEW_FRAME_WIDTH, MAP_TAB_PREVIEW_FRAME_HEIGHT},
        glm::vec2{MAP_TAB_MARKER_SIZE, MAP_TAB_MARKER_SIZE});
    marker.selected_top_left = mapMarkerBottomCenterTopLeft(
        map_local_position,
        layout,
        glm::vec2{MAP_TAB_PREVIEW_FRAME_WIDTH, MAP_TAB_PREVIEW_FRAME_HEIGHT},
        glm::vec2{MAP_TAB_MARKER_SELECTED_SIZE, MAP_TAB_MARKER_SELECTED_SIZE});
    marker.normal_z_index = 50;
    marker.title = game::ui::localizeTextOrFallback(localization, "map.current_position", "Current Position");
    marker.type_label = game::ui::localizeTextOrFallback(localization, "map.type.player", "Player");
    marker.description = Rml::String{map_title.data(), map_title.size()};
    marker.is_selected = selected;
    return marker;
}

[[nodiscard]] MapMarkerViewModel makeObjectMarkerViewModel(const int marker_index,
                                                           const MapObjectMarker& source,
                                                           const MapPreviewLayout& layout,
                                                           const bool selected,
                                                           const game::data::ShopCatalog* shop_catalog,
                                                           const game::data::RpgCatalog* rpg_catalog,
                                                           const game::runtime::LocalizationService* localization) {
    const float marker_size = selected ? MAP_TAB_MARKER_SELECTED_SIZE : MAP_TAB_MARKER_SIZE;
    const glm::vec2 top_left = mapMarkerBottomCenterTopLeft(
        source.map_position,
        layout,
        glm::vec2{MAP_TAB_PREVIEW_FRAME_WIDTH, MAP_TAB_PREVIEW_FRAME_HEIGHT},
        glm::vec2{marker_size, marker_size});

    MapMarkerViewModel marker{};
    marker.marker_index = marker_index;
    marker.kind = markerKindString(source.kind);
    marker.icon_decorator = markerDecorator(source.kind);
    marker.left = formatWholeDp(top_left.x);
    marker.top = formatWholeDp(top_left.y);
    marker.width = formatWholeDp(marker_size);
    marker.height = formatWholeDp(marker_size);
    marker.z_index = selected ? 100 : markerBaseZIndex(source.kind);
    marker.normal_top_left = mapMarkerBottomCenterTopLeft(
        source.map_position,
        layout,
        glm::vec2{MAP_TAB_PREVIEW_FRAME_WIDTH, MAP_TAB_PREVIEW_FRAME_HEIGHT},
        glm::vec2{MAP_TAB_MARKER_SIZE, MAP_TAB_MARKER_SIZE});
    marker.selected_top_left = mapMarkerBottomCenterTopLeft(
        source.map_position,
        layout,
        glm::vec2{MAP_TAB_PREVIEW_FRAME_WIDTH, MAP_TAB_PREVIEW_FRAME_HEIGHT},
        glm::vec2{MAP_TAB_MARKER_SELECTED_SIZE, MAP_TAB_MARKER_SELECTED_SIZE});
    marker.normal_z_index = markerBaseZIndex(source.kind);
    marker.is_selected = selected;
    populateObjectMarkerDetail(marker, source, shop_catalog, rpg_catalog, localization);
    return marker;
}

[[nodiscard]] const char* questMarkerKindString(const QuestRuntimeMarkerKind kind) {
    switch (kind) {
        case QuestRuntimeMarkerKind::Offer:
            return "quest_offer";
        case QuestRuntimeMarkerKind::Objective:
            return "quest_objective";
        case QuestRuntimeMarkerKind::TurnIn:
            return "quest_turn_in";
    }
    return "quest_offer";
}

[[nodiscard]] int questMarkerBaseZIndex(const QuestRuntimeMarkerKind kind) {
    switch (kind) {
        case QuestRuntimeMarkerKind::TurnIn:
            return 45;
        case QuestRuntimeMarkerKind::Objective:
            return 42;
        case QuestRuntimeMarkerKind::Offer:
            return 40;
    }
    return 40;
}

[[nodiscard]] glm::vec2 offsetMarkerTopLeft(const glm::vec2 top_left,
                                            const glm::vec2 marker_size,
                                            const int same_position_offset_index) {
    const float offset = static_cast<float>(same_position_offset_index) * 2.0F;
    return glm::vec2{
        std::clamp(top_left.x + offset, 0.0F, std::max(0.0F, MAP_TAB_PREVIEW_FRAME_WIDTH - marker_size.x)),
        top_left.y,
    };
}

struct MarkerPositionKey {
    float x{0.0F};
    float y{0.0F};
};

[[nodiscard]] bool operator<(const MarkerPositionKey lhs, const MarkerPositionKey rhs) {
    if (lhs.x != rhs.x) {
        return lhs.x < rhs.x;
    }
    return lhs.y < rhs.y;
}

[[nodiscard]] MapMarkerViewModel makeQuestMarkerViewModel(const int marker_index,
                                                          const QuestRuntimeMarker& source,
                                                          const MapPreviewLayout& layout,
                                                          const bool selected,
                                                          const int same_position_offset_index) {
    const float marker_size = selected ? MAP_TAB_MARKER_SELECTED_SIZE : MAP_TAB_MARKER_SIZE;
    const glm::vec2 selected_size{MAP_TAB_MARKER_SELECTED_SIZE, MAP_TAB_MARKER_SELECTED_SIZE};
    const glm::vec2 normal_size{MAP_TAB_MARKER_SIZE, MAP_TAB_MARKER_SIZE};
    const glm::vec2 normal_top_left = offsetMarkerTopLeft(
        mapMarkerBottomCenterTopLeft(
            source.map_position,
            layout,
            glm::vec2{MAP_TAB_PREVIEW_FRAME_WIDTH, MAP_TAB_PREVIEW_FRAME_HEIGHT},
            normal_size),
        normal_size,
        same_position_offset_index);
    const glm::vec2 selected_top_left = offsetMarkerTopLeft(
        mapMarkerBottomCenterTopLeft(
            source.map_position,
            layout,
            glm::vec2{MAP_TAB_PREVIEW_FRAME_WIDTH, MAP_TAB_PREVIEW_FRAME_HEIGHT},
            selected_size),
        selected_size,
        same_position_offset_index);
    const glm::vec2 top_left = selected ? selected_top_left : normal_top_left;

    MapMarkerViewModel marker{};
    marker.marker_index = marker_index;
    marker.kind = questMarkerKindString(source.kind);
    marker.icon_decorator = "image(map-marker-quest)";
    marker.left = formatWholeDp(top_left.x);
    marker.top = formatWholeDp(top_left.y);
    marker.width = formatWholeDp(marker_size);
    marker.height = formatWholeDp(marker_size);
    marker.z_index = selected ? 100 : questMarkerBaseZIndex(source.kind);
    marker.normal_top_left = normal_top_left;
    marker.selected_top_left = selected_top_left;
    marker.normal_z_index = questMarkerBaseZIndex(source.kind);
    marker.title = source.title;
    marker.type_label = source.type_label;
    marker.description = source.description;
    marker.is_selected = selected;
    return marker;
}

void syncDetailFromSelection(MapTabViewState& state,
                             int selected_marker_index,
                             const game::runtime::LocalizationService* localization) {
    const auto selected_it = std::find_if(
        state.map_markers.begin(),
        state.map_markers.end(),
        [selected_marker_index](const MapMarkerViewModel& marker) {
            return marker.marker_index == selected_marker_index;
        });
    if (selected_it != state.map_markers.end()) {
        state.map_detail_title = selected_it->title;
        state.map_detail_type = selected_it->type_label;
        state.map_detail_description = selected_it->description;
        state.has_map_detail = true;
        state.map_status_text = selected_it->title;
        return;
    }

    if (state.has_map_preview && !state.has_place_markers) {
        state.map_detail_title = game::ui::localizeTextOrFallback(localization, "map.no_places_title", "No places marked");
        state.map_detail_type = game::ui::localizeTextOrFallback(localization, "map.type.map", "Map");
        state.map_detail_description = game::ui::localizeTextOrFallback(
            localization,
            "map.no_places_description",
            "Current area has no marked places.");
        state.has_map_detail = true;
        state.map_status_text = state.map_detail_title;
    }
}

} // namespace

bool registerMapTabDataTypes(Rml::DataModelConstructor& constructor) {
    return registerMapMarkerViewModelType(constructor) &&
           constructor.RegisterArray<MapMarkerViewModels>();
}

MapTabViewState buildMapTabViewState(const entt::registry& /*registry*/,
                                     const entt::entity /*player*/,
                                     const game::world::WorldState* world_state,
                                     const entt::id_type map_id,
                                     const MapTabPreviewInput& preview,
                                     const game::runtime::LocalizationService* localization) {
    MapTabViewState state{};
    const game::world::MapInfo* map_info = findMapInfo(world_state, map_id);
    if (!map_info) {
        state.map_status_text = game::ui::localizeTextOrFallback(localization, "inventory.map.empty", "No map data");
        return state;
    }

    state.map_title = localizedMapName(localization, map_info->name);

    const bool preview_valid = !preview.source_uri.empty() && preview.width > 0 && preview.height > 0;
    if (!preview_valid) {
        state.map_status_text = game::ui::localizeTextOrFallback(localization, "inventory.map.empty", "No map data");
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
    state.map_status_text = game::ui::localizeTextOrFallback(localization, "map.current_position", "Current Position");
    return state;
}

MapTabViewState buildMapTabViewState(const entt::registry& registry,
                                     const entt::entity player,
                                     const game::world::WorldState* world_state,
                                     const entt::id_type map_id,
                                     const MapTabPreviewInput& preview,
                                     const std::vector<MapObjectMarker>& static_place_markers,
                                     const std::vector<QuestRuntimeMarker>& quest_markers,
                                     const game::data::ShopCatalog* shop_catalog,
                                     const game::data::RpgCatalog* rpg_catalog,
                                     const int selected_marker_index,
                                     const game::runtime::LocalizationService* localization) {
    MapTabViewState state = buildMapTabViewState(registry, player, world_state, map_id, preview, localization);
    if (!state.has_map_preview) {
        return state;
    }

    const MapPreviewLayout layout = computeMapPreviewLayout(
        glm::vec2{static_cast<float>(preview.width), static_cast<float>(preview.height)},
        glm::vec2{MAP_TAB_PREVIEW_FRAME_WIDTH, MAP_TAB_PREVIEW_FRAME_HEIGHT});

    int marker_index = 0;
    if (player != entt::null && registry.valid(player)) {
        if (const auto* transform = registry.try_get<engine::component::TransformComponent>(player)) {
            const glm::vec2 player_position = glm::clamp(
                transform->position_,
                glm::vec2{0.0F, 0.0F},
                glm::vec2{static_cast<float>(preview.width), static_cast<float>(preview.height)});
            state.map_markers.push_back(makePlayerMarkerViewModel(
                marker_index,
                player_position,
                layout,
                std::string_view{state.map_title.data(), state.map_title.size()},
                marker_index == selected_marker_index,
                localization));
            ++marker_index;
        }
    }

    std::map<MarkerPositionKey, int> quest_position_counts{};
    for (const QuestRuntimeMarker& quest_marker : quest_markers) {
        QuestRuntimeMarker clamped_marker = quest_marker;
        clamped_marker.map_position = glm::clamp(
            clamped_marker.map_position,
            glm::vec2{0.0F, 0.0F},
            glm::vec2{static_cast<float>(preview.width), static_cast<float>(preview.height)});
        const MarkerPositionKey position_key{clamped_marker.map_position.x, clamped_marker.map_position.y};
        const int same_position_offset_index = quest_position_counts[position_key]++;

        state.map_markers.push_back(makeQuestMarkerViewModel(
            marker_index,
            clamped_marker,
            layout,
            marker_index == selected_marker_index,
            same_position_offset_index));
        ++marker_index;
    }

    for (const MapObjectMarker& object_marker : static_place_markers) {
        MapObjectMarker clamped_marker = object_marker;
        clamped_marker.map_position = glm::clamp(
            object_marker.map_position,
            glm::vec2{0.0F, 0.0F},
            glm::vec2{static_cast<float>(preview.width), static_cast<float>(preview.height)});
        state.map_markers.push_back(makeObjectMarkerViewModel(
            marker_index,
            clamped_marker,
            layout,
            marker_index == selected_marker_index,
            shop_catalog,
            rpg_catalog,
            localization));
        ++marker_index;
    }

    state.has_map_markers = !state.map_markers.empty();
    state.has_place_markers = !quest_markers.empty() || !static_place_markers.empty();
    syncDetailFromSelection(state, selected_marker_index, localization);
    return state;
}

int defaultMapMarkerSelection(const bool has_player_marker, const std::size_t object_marker_count) {
    if (object_marker_count > 0U) {
        return has_player_marker ? 1 : 0;
    }
    if (has_player_marker) {
        return 0;
    }
    return -1;
}

MapTabContent::MapTabContent(engine::ui::rmlui::RmlDocumentController& document_controller,
                             entt::registry& game_registry,
                             const entt::entity player,
                             const game::world::WorldState* world_state,
                             const game::data::QuestCatalog* quest_catalog,
                             const game::data::ShopCatalog* shop_catalog,
                             const game::data::RpgCatalog* rpg_catalog,
                             engine::ui::rmlui::RmlGeneratedImageRegistry* generated_images)
    : document_controller_(document_controller),
      game_registry_(game_registry),
      player_(player),
      world_state_(world_state),
      quest_catalog_(quest_catalog),
      shop_catalog_(shop_catalog),
      rpg_catalog_(rpg_catalog),
      localization_(game::runtime::findLocalizationService(game_registry)),
      generated_images_(generated_images) {
}

bool MapTabContent::bindModel(Rml::DataModelConstructor& constructor) {
    if (!constructor.Bind("map_title", &map_title_) ||
        !constructor.Bind("map_preview_src", &map_preview_src_) ||
        !constructor.Bind("map_preview_left", &map_preview_left_) ||
        !constructor.Bind("map_preview_top", &map_preview_top_) ||
        !constructor.Bind("map_preview_width", &map_preview_width_) ||
        !constructor.Bind("map_preview_height", &map_preview_height_) ||
        !constructor.Bind("map_status_text", &map_status_text_) ||
        !constructor.Bind("map_markers", &map_markers_) ||
        !constructor.Bind("map_detail_title", &map_detail_title_) ||
        !constructor.Bind("map_detail_type", &map_detail_type_) ||
        !constructor.Bind("map_detail_description", &map_detail_description_) ||
        !constructor.Bind("has_map_preview", &has_map_preview_) ||
        !constructor.Bind("has_map_markers", &has_map_markers_) ||
        !constructor.Bind("has_place_markers", &has_place_markers_) ||
        !constructor.Bind("has_map_detail", &has_map_detail_)) {
        spdlog::error("MapTabContent: failed to bind map tab data model.");
        return false;
    }

    if (!bindIndexedEventCallback(constructor, "map_marker_focus", this, &MapTabContent::onMarkerFocus) ||
        !bindIndexedEventCallback(constructor, "map_marker_hover", this, &MapTabContent::onMarkerHover) ||
        !bindIndexedEventCallback(constructor, "map_marker_click", this, &MapTabContent::onMarkerClick)) {
        spdlog::error("MapTabContent: failed to bind map marker event callbacks.");
        return false;
    }

    return true;
}

void MapTabContent::onActivated() {
    selected_marker_index_ = -1;
    syncViewState();
}

void MapTabContent::onDeactivated() {
}

void MapTabContent::update(const float /*delta_time*/) {
}

bool MapTabContent::onCancel() {
    return false;
}

void MapTabContent::onLanguageChanged() {
    localization_ = game::runtime::findLocalizationService(game_registry_);
    syncViewState();
}

void MapTabContent::invalidateQuestMarkers() {
    selected_marker_index_ = -1;
    syncViewState();
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
    preview_registration_ = generated_images_->registerImage(
        input.source_uri,
        std::move(result.image),
        engine::ui::rmlui::RmlUiTextureFilterMode::Linear);
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
    const game::world::MapInfo* map_info = findMapInfo(world_state_, map_id);
    const MapTabPreviewInput preview = buildPreviewInput(map_id);
    MapMarkerSnapshot marker_snapshot{};
    std::vector<QuestRuntimeMarker> quest_markers{};
    if (map_info && !map_info->file_path.empty() && preview.width > 0 && preview.height > 0) {
        marker_snapshot = marker_provider_.snapshotForMap(map_info->file_path);
        const auto* quest_log = player_ != entt::null && game_registry_.valid(player_)
                                    ? game_registry_.try_get<game::component::QuestLogComponent>(player_)
                                    : nullptr;
        if (quest_log && quest_catalog_) {
            quest_markers = resolveQuestMapMarkers(
                map_id,
                *quest_log,
                *quest_catalog_,
                marker_snapshot.quest_giver_markers,
                localization_);
        }
    }

    const bool rebuilt_for_new_map = current_marker_map_id_ != map_id;
    if (rebuilt_for_new_map) {
        selected_marker_index_ = -1;
        current_marker_map_id_ = map_id;
    }

    const bool has_preview = !preview.source_uri.empty() && preview.width > 0 && preview.height > 0;
    const bool has_player_marker = has_preview && player_ != entt::null && game_registry_.valid(player_) &&
                                   game_registry_.try_get<engine::component::TransformComponent>(player_) != nullptr;
    const std::size_t place_marker_count = quest_markers.size() + marker_snapshot.static_place_markers.size();
    const int marker_count = static_cast<int>(place_marker_count) + (has_player_marker ? 1 : 0);
    if (selected_marker_index_ < 0 || selected_marker_index_ >= marker_count) {
        selected_marker_index_ = defaultMapMarkerSelection(has_player_marker, place_marker_count);
    }

    MapTabViewState state = buildMapTabViewState(
        game_registry_,
        player_,
        world_state_,
        map_id,
        preview,
        marker_snapshot.static_place_markers,
        quest_markers,
        shop_catalog_,
        rpg_catalog_,
        selected_marker_index_,
        localization_);

    map_title_ = std::move(state.map_title);
    map_preview_src_ = std::move(state.map_preview_src);
    map_preview_left_ = std::move(state.map_preview_left);
    map_preview_top_ = std::move(state.map_preview_top);
    map_preview_width_ = std::move(state.map_preview_width);
    map_preview_height_ = std::move(state.map_preview_height);
    map_status_text_ = std::move(state.map_status_text);
    map_markers_ = std::move(state.map_markers);
    map_detail_title_ = std::move(state.map_detail_title);
    map_detail_type_ = std::move(state.map_detail_type);
    map_detail_description_ = std::move(state.map_detail_description);
    has_map_preview_ = state.has_map_preview;
    has_map_markers_ = state.has_map_markers;
    has_place_markers_ = state.has_place_markers;
    has_map_detail_ = state.has_map_detail;

    document_controller_.markDirty("map_title");
    document_controller_.markDirty("map_preview_src");
    document_controller_.markDirty("map_preview_left");
    document_controller_.markDirty("map_preview_top");
    document_controller_.markDirty("map_preview_width");
    document_controller_.markDirty("map_preview_height");
    document_controller_.markDirty("map_status_text");
    document_controller_.markDirty("map_markers");
    document_controller_.markDirty("map_detail_title");
    document_controller_.markDirty("map_detail_type");
    document_controller_.markDirty("map_detail_description");
    document_controller_.markDirty("has_map_preview");
    document_controller_.markDirty("has_map_markers");
    document_controller_.markDirty("has_place_markers");
    document_controller_.markDirty("has_map_detail");
}

void MapTabContent::syncDetailFromSelection() {
    map_detail_title_.clear();
    map_detail_type_.clear();
    map_detail_description_.clear();
    has_map_detail_ = false;

    const auto selected_it = std::find_if(
        map_markers_.begin(),
        map_markers_.end(),
        [this](const MapMarkerViewModel& marker) {
            return marker.marker_index == selected_marker_index_;
        });
    if (selected_it != map_markers_.end()) {
        map_detail_title_ = selected_it->title;
        map_detail_type_ = selected_it->type_label;
        map_detail_description_ = selected_it->description;
        map_status_text_ = selected_it->title;
        has_map_detail_ = true;
        return;
    }

    if (has_map_preview_ && !has_place_markers_) {
        map_detail_title_ = game::ui::localizeTextOrFallback(localization_, "map.no_places_title", "No places marked");
        map_detail_type_ = game::ui::localizeTextOrFallback(localization_, "map.type.map", "Map");
        map_detail_description_ = game::ui::localizeTextOrFallback(
            localization_,
            "map.no_places_description",
            "Current area has no marked places.");
        map_status_text_ = map_detail_title_;
        has_map_detail_ = true;
    }
}

void MapTabContent::markDetailDirty() {
    document_controller_.markDirty("map_status_text");
    document_controller_.markDirty("map_detail_title");
    document_controller_.markDirty("map_detail_type");
    document_controller_.markDirty("map_detail_description");
    document_controller_.markDirty("has_map_detail");
}

void MapTabContent::selectMarker(const int marker_index) {
    if (marker_index < 0) {
        return;
    }

    const auto selected_it = std::find_if(
        map_markers_.begin(),
        map_markers_.end(),
        [marker_index](const MapMarkerViewModel& marker) {
            return marker.marker_index == marker_index;
        });
    if (selected_it == map_markers_.end() || selected_marker_index_ == marker_index) {
        return;
    }

    bool changed = false;
    for (MapMarkerViewModel& marker : map_markers_) {
        const bool should_select = marker.marker_index == marker_index;
        if (marker.is_selected == should_select) {
            continue;
        }

        marker.is_selected = should_select;
        const glm::vec2 top_left = should_select ? marker.selected_top_left : marker.normal_top_left;
        const float marker_size = should_select ? MAP_TAB_MARKER_SELECTED_SIZE : MAP_TAB_MARKER_SIZE;
        marker.left = formatWholeDp(top_left.x);
        marker.top = formatWholeDp(top_left.y);
        marker.width = formatWholeDp(marker_size);
        marker.height = formatWholeDp(marker_size);
        marker.z_index = should_select ? 100 : marker.normal_z_index;
        changed = true;
    }

    selected_marker_index_ = marker_index;
    syncDetailFromSelection();

    if (changed) {
        document_controller_.markDirty("map_markers");
    }
    markDetailDirty();
}

void MapTabContent::onMarkerFocus(const int marker_index, Rml::Event& /*event*/) {
    selectMarker(marker_index);
}

void MapTabContent::onMarkerHover(const int marker_index, Rml::Event& /*event*/) {
    selectMarker(marker_index);
}

void MapTabContent::onMarkerClick(const int marker_index, Rml::Event& event) {
    event.StopPropagation();
    selectMarker(marker_index);
}

} // namespace game::ui
