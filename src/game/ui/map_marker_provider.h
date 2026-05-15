#pragma once

#include <glm/vec2.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace game::ui {

enum class MapObjectMarkerKind : std::uint8_t {
    Shop = 0,
    Rest,
    Npc,
};

struct MapObjectMarker {
    MapObjectMarkerKind kind{MapObjectMarkerKind::Shop};
    int object_id{0};
    std::string object_name{};
    std::string shop_id{};
    std::string recruit_actor_id{};
    glm::vec2 map_position{};
};

/// @brief Tiled quest giver actor location used as runtime quest marker input.
///
/// Only actors that resolve to a pure quest giver at runtime enter this list. Hybrid shop+quest
/// actors remain static shop markers so the map mirrors interaction priority.
struct QuestGiverLocation {
    int object_id{0};
    std::string object_name{};
    std::string quest_id{};
    glm::vec2 map_position{};
};

struct MapMarkerSnapshot {
    std::vector<MapObjectMarker> static_place_markers{};
    std::vector<QuestGiverLocation> quest_giver_markers{};
};

class MapMarkerProvider final {
public:
    [[nodiscard]] MapMarkerSnapshot snapshotForMap(std::string_view tmj_path);
    void clearCache();

private:
    std::unordered_map<std::string, MapMarkerSnapshot> marker_cache_{};
};

} // namespace game::ui
