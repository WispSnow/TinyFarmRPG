#include "game/ui/map_tab_content.h"

#include "engine/component/transform_component.h"
#include "game/data/quest_catalog.h"
#include "game/data/quest_data.h"
#include "game/data/rpg_catalog.h"
#include "game/data/rpg_data.h"
#include "game/data/shop_catalog.h"
#include "game/data/shop_data.h"
#include "game/ui/map_coordinate_mapper.h"
#include "game/ui/slot_grid_support.h"
#include "game/world/world_state.h"

#include <RmlUi/Core/DataTypeRegister.h>
#include <RmlUi/Core/Event.h>
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

using MapMarkerViewModels = std::vector<MapMarkerViewModel>;

[[nodiscard]] std::string formatDp(float value) {
    std::array<char, 32> buffer{};
    const int count = std::snprintf(buffer.data(), buffer.size(), "%.2fdp", value);
    if (count <= 0) {
        return "0dp";
    }
    return std::string{buffer.data(), static_cast<std::size_t>(std::min(count, static_cast<int>(buffer.size() - 1U)))};
}

[[nodiscard]] std::string titleCaseLabel(std::string_view value,
                                         const char* fallback,
                                         const bool strip_path_prefix) {
    if (strip_path_prefix) {
        const std::size_t separator = value.find_last_of(".:/");
        if (separator != std::string_view::npos) {
            value = value.substr(separator + 1U);
        }
    }

    std::string label{value};
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
    return label.empty() ? fallback : label;
}

[[nodiscard]] std::string humanizeMapName(std::string_view map_name) {
    return titleCaseLabel(map_name, "Map", false);
}

[[nodiscard]] std::string humanizeId(std::string_view id) {
    return titleCaseLabel(id, "Place", true);
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

[[nodiscard]] const char* markerKindString(const MapObjectMarkerKind kind) {
    switch (kind) {
        case MapObjectMarkerKind::Quest:
            return "quest";
        case MapObjectMarkerKind::Shop:
            return "shop";
        case MapObjectMarkerKind::Rest:
            return "rest";
        case MapObjectMarkerKind::Npc:
            return "npc";
    }
    return "npc";
}

[[nodiscard]] const char* markerKindTypeLabel(const MapObjectMarkerKind kind) {
    switch (kind) {
        case MapObjectMarkerKind::Quest:
            return "Quest";
        case MapObjectMarkerKind::Shop:
            return "Shop";
        case MapObjectMarkerKind::Rest:
            return "Rest Point";
        case MapObjectMarkerKind::Npc:
            return "NPC";
    }
    return "Place";
}

[[nodiscard]] const char* markerDecorator(const MapObjectMarkerKind kind) {
    switch (kind) {
        case MapObjectMarkerKind::Quest:
            return "image(map-marker-quest)";
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
        case MapObjectMarkerKind::Quest:
            return 40;
        case MapObjectMarkerKind::Shop:
            return 30;
        case MapObjectMarkerKind::Rest:
            return 20;
        case MapObjectMarkerKind::Npc:
            return 10;
    }
    return 10;
}

void populateQuestDetail(MapMarkerViewModel& marker,
                         const MapObjectMarker& source,
                         const game::data::QuestCatalog* quest_catalog) {
    const auto* quest = quest_catalog && !source.quest_id.empty() ? quest_catalog->findQuest(source.quest_id) : nullptr;
    if (quest) {
        marker.title = quest->title_.empty() ? source.quest_id : quest->title_;
        marker.description = quest->description_.empty() ? "Quest giver" : quest->description_;
        return;
    }
    marker.title = !source.object_name.empty() ? humanizeId(source.object_name) : humanizeId(source.quest_id);
    marker.description = "Quest giver";
    if (!source.quest_id.empty()) {
        spdlog::warn("MapTabContent: quest marker references missing quest_id='{}'.", source.quest_id);
    }
}

void populateShopDetail(MapMarkerViewModel& marker,
                        const MapObjectMarker& source,
                        const game::data::ShopCatalog* shop_catalog) {
    const auto* shop = shop_catalog && !source.shop_id.empty() ? shop_catalog->findShop(source.shop_id) : nullptr;
    if (shop) {
        marker.title = shop->title_.empty() ? source.shop_id : shop->title_;
        marker.description = shop->greeting_.empty() ? "Buy and sell supplies." : shop->greeting_;
        return;
    }
    marker.title = !source.object_name.empty() ? humanizeId(source.object_name) : humanizeId(source.shop_id);
    marker.description = "Buy and sell supplies.";
    if (!source.shop_id.empty()) {
        spdlog::warn("MapTabContent: shop marker references missing shop_id='{}'.", source.shop_id);
    }
}

void populateNpcDetail(MapMarkerViewModel& marker,
                       const MapObjectMarker& source,
                       const game::data::RpgCatalog* rpg_catalog) {
    const auto* actor = rpg_catalog && !source.recruit_actor_id.empty()
                            ? rpg_catalog->findActor(source.recruit_actor_id)
                            : nullptr;
    if (actor) {
        marker.title = actor->display_name_.empty() ? source.recruit_actor_id : actor->display_name_;
    } else {
        marker.title = !source.object_name.empty() ? humanizeId(source.object_name) : humanizeId(source.recruit_actor_id);
        if (!source.recruit_actor_id.empty()) {
            spdlog::warn("MapTabContent: npc marker references missing recruit_actor_id='{}'.", source.recruit_actor_id);
        }
    }
    marker.description = "Talk to this person.";
}

void populateObjectMarkerDetail(MapMarkerViewModel& marker,
                                const MapObjectMarker& source,
                                const game::data::QuestCatalog* quest_catalog,
                                const game::data::ShopCatalog* shop_catalog,
                                const game::data::RpgCatalog* rpg_catalog) {
    marker.type_label = markerKindTypeLabel(source.kind);
    switch (source.kind) {
        case MapObjectMarkerKind::Quest:
            populateQuestDetail(marker, source, quest_catalog);
            break;
        case MapObjectMarkerKind::Shop:
            populateShopDetail(marker, source, shop_catalog);
            break;
        case MapObjectMarkerKind::Rest:
            marker.title = source.object_name.empty() ? "Rest Point" : humanizeId(source.object_name);
            marker.description = "Recover and pass time.";
            break;
        case MapObjectMarkerKind::Npc:
            populateNpcDetail(marker, source, rpg_catalog);
            break;
    }
}

[[nodiscard]] MapMarkerViewModel makePlayerMarkerViewModel(const int marker_index,
                                                           const glm::vec2 map_local_position,
                                                           const MapPreviewLayout& layout,
                                                           const std::string_view map_title,
                                                           const bool selected) {
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
    marker.left = formatDp(top_left.x);
    marker.top = formatDp(top_left.y);
    marker.width = formatDp(marker_size);
    marker.height = formatDp(marker_size);
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
    marker.title = "Current Position";
    marker.type_label = "Player";
    marker.description = Rml::String{map_title.data(), map_title.size()};
    marker.is_selected = selected;
    return marker;
}

[[nodiscard]] MapMarkerViewModel makeObjectMarkerViewModel(const int marker_index,
                                                           const MapObjectMarker& source,
                                                           const MapPreviewLayout& layout,
                                                           const bool selected,
                                                           const game::data::QuestCatalog* quest_catalog,
                                                           const game::data::ShopCatalog* shop_catalog,
                                                           const game::data::RpgCatalog* rpg_catalog) {
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
    marker.left = formatDp(top_left.x);
    marker.top = formatDp(top_left.y);
    marker.width = formatDp(marker_size);
    marker.height = formatDp(marker_size);
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
    populateObjectMarkerDetail(marker, source, quest_catalog, shop_catalog, rpg_catalog);
    return marker;
}

void syncDetailFromSelection(MapTabViewState& state, int selected_marker_index) {
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
        state.map_detail_title = "No places marked";
        state.map_detail_type = "Map";
        state.map_detail_description = "Current area has no marked places.";
        state.has_map_detail = true;
        state.map_status_text = "No places marked";
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
    return state;
}

MapTabViewState buildMapTabViewState(const entt::registry& registry,
                                     const entt::entity player,
                                     const game::world::WorldState* world_state,
                                     const entt::id_type map_id,
                                     const MapTabPreviewInput& preview,
                                     const std::vector<MapObjectMarker>& object_markers,
                                     const game::data::QuestCatalog* quest_catalog,
                                     const game::data::ShopCatalog* shop_catalog,
                                     const game::data::RpgCatalog* rpg_catalog,
                                     const int selected_marker_index) {
    MapTabViewState state = buildMapTabViewState(registry, player, world_state, map_id, preview);
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
                marker_index == selected_marker_index));
            ++marker_index;
        }
    }

    for (const MapObjectMarker& object_marker : object_markers) {
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
            quest_catalog,
            shop_catalog,
            rpg_catalog));
        ++marker_index;
    }

    state.has_map_markers = !state.map_markers.empty();
    state.has_place_markers = !object_markers.empty();
    syncDetailFromSelection(state, selected_marker_index);
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
    const game::world::MapInfo* map_info = findMapInfo(world_state_, map_id);
    const MapTabPreviewInput preview = buildPreviewInput(map_id);
    std::vector<MapObjectMarker> object_markers{};
    if (map_info && !map_info->file_path.empty() && preview.width > 0 && preview.height > 0) {
        object_markers = marker_provider_.markersForMap(map_info->file_path);
    }

    const bool rebuilt_for_new_map = current_marker_map_id_ != map_id;
    if (rebuilt_for_new_map) {
        selected_marker_index_ = -1;
        current_marker_map_id_ = map_id;
    }

    const bool has_preview = !preview.source_uri.empty() && preview.width > 0 && preview.height > 0;
    const bool has_player_marker = has_preview && player_ != entt::null && game_registry_.valid(player_) &&
                                   game_registry_.try_get<engine::component::TransformComponent>(player_) != nullptr;
    const int marker_count = static_cast<int>(object_markers.size()) + (has_player_marker ? 1 : 0);
    if (selected_marker_index_ < 0 || selected_marker_index_ >= marker_count) {
        selected_marker_index_ = defaultMapMarkerSelection(has_player_marker, object_markers.size());
    }

    MapTabViewState state = buildMapTabViewState(
        game_registry_,
        player_,
        world_state_,
        map_id,
        preview,
        object_markers,
        quest_catalog_,
        shop_catalog_,
        rpg_catalog_,
        selected_marker_index_);

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
        map_detail_title_ = "No places marked";
        map_detail_type_ = "Map";
        map_detail_description_ = "Current area has no marked places.";
        map_status_text_ = "No places marked";
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
        marker.left = formatDp(top_left.x);
        marker.top = formatDp(top_left.y);
        marker.width = formatDp(marker_size);
        marker.height = formatDp(marker_size);
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
