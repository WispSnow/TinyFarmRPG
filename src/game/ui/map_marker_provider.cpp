#include "game/ui/map_marker_provider.h"

#include "engine/loader/tiled_json_cache.h"
#include "engine/loader/tiled_json_helpers.h"
#include "game/loader/tiled_conventions.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace game::ui {
namespace {

[[nodiscard]] bool boolPropertyIsTrue(const nlohmann::json& object, const std::string_view name) {
    const auto properties_it = object.find("properties");
    if (properties_it == object.end() || !properties_it->is_array()) {
        return false;
    }

    for (const auto& property : *properties_it) {
        if (!property.is_object() || property.value("name", std::string{}) != name) {
            continue;
        }
        const auto value_it = property.find("value");
        return value_it != property.end() && value_it->is_boolean() && value_it->get<bool>();
    }
    return false;
}

[[nodiscard]] std::optional<std::string> findStringProperty(const nlohmann::json& object,
                                                            const std::string_view name) {
    const auto properties_it = object.find("properties");
    if (properties_it == object.end() || !properties_it->is_array()) {
        return std::nullopt;
    }

    for (const auto& property : *properties_it) {
        if (!property.is_object() || property.value("name", std::string{}) != name ||
            property.value("type", std::string{}) != "string") {
            continue;
        }
        if (const auto value_it = property.find("value");
            value_it != property.end() && value_it->is_string() && !value_it->get<std::string>().empty()) {
            return value_it->get<std::string>();
        }
    }
    return std::nullopt;
}

[[nodiscard]] float jsonFloatOr(const nlohmann::json& object, const char* key, const float fallback) {
    return engine::loader::tiled::jsonFloatOr(object, key, fallback);
}

[[nodiscard]] std::optional<glm::vec2> polygonBottomCenter(const nlohmann::json& object) {
    const auto points_it = object.find("polygon");
    const auto polyline_it = object.find("polyline");
    const nlohmann::json* points = nullptr;
    if (points_it != object.end() && points_it->is_array()) {
        points = &(*points_it);
    } else if (polyline_it != object.end() && polyline_it->is_array()) {
        points = &(*polyline_it);
    }
    if (!points || points->empty()) {
        return std::nullopt;
    }

    float min_x = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();
    float max_y = std::numeric_limits<float>::lowest();
    bool has_point = false;
    for (const auto& point : *points) {
        if (!point.is_object()) {
            continue;
        }
        const float x = jsonFloatOr(point, "x", 0.0F);
        const float y = jsonFloatOr(point, "y", 0.0F);
        min_x = std::min(min_x, x);
        max_x = std::max(max_x, x);
        max_y = std::max(max_y, y);
        has_point = true;
    }
    if (!has_point) {
        return std::nullopt;
    }

    return glm::vec2{
        jsonFloatOr(object, "x", 0.0F) + (min_x + max_x) * 0.5F,
        jsonFloatOr(object, "y", 0.0F) + max_y,
    };
}

[[nodiscard]] std::optional<glm::vec2> markerPositionForObject(const nlohmann::json& object) {
    const float x = jsonFloatOr(object, "x", 0.0F);
    const float y = jsonFloatOr(object, "y", 0.0F);
    if (object.value("point", false)) {
        return glm::vec2{x, y};
    }
    if (object.contains("polygon") || object.contains("polyline")) {
        return polygonBottomCenter(object);
    }

    const float width = jsonFloatOr(object, "width", 0.0F);
    const float height = jsonFloatOr(object, "height", 0.0F);
    if (width < 0.0F || height < 0.0F) {
        return std::nullopt;
    }
    return glm::vec2{x + width * 0.5F, y + height};
}

[[nodiscard]] int objectId(const nlohmann::json& object) {
    return engine::loader::tiled::jsonIntOr(object, "id", 0);
}

void appendMarkerForActorObject(const nlohmann::json& object, MapMarkerSnapshot& out_snapshot) {
    const auto position = markerPositionForObject(object);
    if (!position) {
        spdlog::warn("MapMarkerProvider: actor object id={} has unsupported marker shape.", objectId(object));
        return;
    }

    const auto quest_id = findStringProperty(object, game::loader::tiled::ACTOR_PROP_QUEST_OFFER_ID);
    const auto shop_id = findStringProperty(object, game::loader::tiled::ACTOR_PROP_SHOP_ID);
    const auto recruit_actor_id = findStringProperty(object, game::loader::tiled::ACTOR_PROP_RECRUIT_ACTOR_ID);
    if (!quest_id && !shop_id && !recruit_actor_id) {
        return;
    }

    if (shop_id) {
        if (quest_id) {
            spdlog::warn(
                "MapMarkerProvider: actor object id={} has both shop_id='{}' and quest_offer_id='{}'; using shop marker to match runtime interaction.",
                objectId(object),
                *shop_id,
                *quest_id);
        }

        MapObjectMarker marker{};
        marker.kind = MapObjectMarkerKind::Shop;
        marker.object_id = objectId(object);
        marker.object_name = object.value("name", std::string{});
        marker.shop_id = *shop_id;
        marker.recruit_actor_id = recruit_actor_id.value_or(std::string{});
        marker.map_position = *position;
        out_snapshot.static_place_markers.push_back(std::move(marker));
        return;
    }

    if (quest_id) {
        QuestGiverLocation giver{};
        giver.object_id = objectId(object);
        giver.object_name = object.value("name", std::string{});
        giver.quest_id = *quest_id;
        giver.map_position = *position;
        out_snapshot.quest_giver_markers.push_back(std::move(giver));
        return;
    }

    MapObjectMarker marker{};
    marker.kind = MapObjectMarkerKind::Npc;
    marker.object_id = objectId(object);
    marker.object_name = object.value("name", std::string{});
    marker.recruit_actor_id = recruit_actor_id.value_or(std::string{});
    marker.map_position = *position;
    out_snapshot.static_place_markers.push_back(std::move(marker));
}

[[nodiscard]] std::optional<MapObjectMarker> markerForRestObject(const nlohmann::json& object) {
    const auto position = markerPositionForObject(object);
    if (!position) {
        spdlog::warn("MapMarkerProvider: rest object id={} has unsupported marker shape.", objectId(object));
        return std::nullopt;
    }

    MapObjectMarker marker{};
    marker.kind = MapObjectMarkerKind::Rest;
    marker.object_id = objectId(object);
    marker.object_name = object.value("name", std::string{});
    marker.map_position = *position;
    return marker;
}

[[nodiscard]] int markerSortRank(const MapObjectMarkerKind kind) {
    switch (kind) {
        case MapObjectMarkerKind::Shop:
            return 0;
        case MapObjectMarkerKind::Rest:
            return 1;
        case MapObjectMarkerKind::Npc:
            return 2;
    }
    return 3;
}

void appendMarkersFromLayer(const nlohmann::json& layer, MapMarkerSnapshot& out_snapshot) {
    if (!layer.is_object() || layer.value("type", "") != "objectgroup" || layer.value("visible", true) == false ||
        boolPropertyIsTrue(layer, "invisible")) {
        return;
    }

    const auto objects_it = layer.find("objects");
    if (objects_it == layer.end() || !objects_it->is_array()) {
        return;
    }

    for (const auto& object : *objects_it) {
        if (!object.is_object() || object.value("visible", true) == false || boolPropertyIsTrue(object, "invisible")) {
            continue;
        }

        const std::string type = object.value("type", std::string{});
        if (type == game::loader::tiled::OBJECT_TYPE_ACTOR) {
            appendMarkerForActorObject(object, out_snapshot);
        } else if (type == game::loader::tiled::OBJECT_TYPE_REST) {
            if (std::optional<MapObjectMarker> marker = markerForRestObject(object)) {
                out_snapshot.static_place_markers.push_back(std::move(*marker));
            }
        }
    }
}

void appendMarkersFromLayers(const nlohmann::json& layers, MapMarkerSnapshot& out_snapshot) {
    if (!layers.is_array()) {
        return;
    }

    for (const auto& layer : layers) {
        if (!layer.is_object() || layer.value("visible", true) == false || boolPropertyIsTrue(layer, "invisible")) {
            continue;
        }
        if (layer.value("type", "") == "group") {
            if (const auto child_layers = layer.find("layers"); child_layers != layer.end()) {
                appendMarkersFromLayers(*child_layers, out_snapshot);
            }
            continue;
        }
        appendMarkersFromLayer(layer, out_snapshot);
    }
}

} // namespace

MapMarkerSnapshot MapMarkerProvider::snapshotForMap(const std::string_view tmj_path) {
    const std::string cache_key = std::filesystem::path{std::string{tmj_path}}.lexically_normal().string();
    if (const auto it = marker_cache_.find(cache_key); it != marker_cache_.end()) {
        return it->second;
    }

    MapMarkerSnapshot snapshot{};
    const auto map_json = engine::loader::tiled::getOrLoadLevelJson(tmj_path);
    if (!map_json) {
        marker_cache_.emplace(cache_key, snapshot);
        return snapshot;
    }

    if (const auto layers_it = map_json->find("layers"); layers_it != map_json->end()) {
        appendMarkersFromLayers(*layers_it, snapshot);
    }

    std::sort(snapshot.static_place_markers.begin(), snapshot.static_place_markers.end(), [](const MapObjectMarker& lhs, const MapObjectMarker& rhs) {
        const int lhs_rank = markerSortRank(lhs.kind);
        const int rhs_rank = markerSortRank(rhs.kind);
        if (lhs_rank != rhs_rank) {
            return lhs_rank < rhs_rank;
        }
        return lhs.object_id < rhs.object_id;
    });

    std::sort(snapshot.quest_giver_markers.begin(), snapshot.quest_giver_markers.end(), [](const QuestGiverLocation& lhs, const QuestGiverLocation& rhs) {
        if (lhs.quest_id != rhs.quest_id) {
            return lhs.quest_id < rhs.quest_id;
        }
        return lhs.object_id < rhs.object_id;
    });

    marker_cache_.emplace(cache_key, snapshot);
    return snapshot;
}

void MapMarkerProvider::clearCache() {
    marker_cache_.clear();
}

} // namespace game::ui
