#pragma once

#include <glm/vec2.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace game::ui {

enum class MapObjectMarkerKind : std::uint8_t {
    Quest = 0,
    Shop,
    Rest,
    Npc,
};

struct MapObjectMarker {
    MapObjectMarkerKind kind{MapObjectMarkerKind::Quest};
    int object_id{0};
    std::string object_name{};
    std::string quest_id{};
    std::string shop_id{};
    std::string recruit_actor_id{};
    glm::vec2 map_position{};
};

class MapMarkerProvider final {
public:
    [[nodiscard]] std::vector<MapObjectMarker> markersForMap(std::string_view tmj_path);
    void clearCache();

private:
    std::unordered_map<std::string, std::vector<MapObjectMarker>> marker_cache_{};
};

} // namespace game::ui
