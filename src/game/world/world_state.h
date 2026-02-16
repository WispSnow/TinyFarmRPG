#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <entt/core/hashed_string.hpp>
#include <entt/core/any.hpp>
#include <entt/entity/fwd.hpp>
#include "engine/utils/math.h"
#include "game/component/map_component.h"
#include "game/component/resource_node_component.h"

#include "engine/spatial/spatial_index_manager.h"

namespace game::world {

/**
 * @brief EnTT 快照存档（内存版）
 *
 * - 用于 MapManager 在内存中序列化/反序列化“地图动态实体”（耕地/湿润/作物/资源节点等）
 * - 目前仅用于运行时多地图切换与离线日更，不涉及磁盘持久化
 */
struct RegistrySnapshot {
    std::vector<entt::any> data{};
    std::size_t read_pos{0};
    bool valid{false};

    void clear() {
        data.clear();
        read_pos = 0;
        valid = false;
    }

    void resetRead() { read_pos = 0; }

    template <typename T>
    void operator()(T&& value) {
        data.emplace_back(std::forward<T>(value));
    }

    template <typename T>
    void operator()(T& value) requires (!std::is_const_v<T>) {
        value = entt::any_cast<std::remove_reference_t<T>>(data.at(read_pos++));
    }
};

struct TriggerInfo {
    engine::utils::Rect rect{};
    int self_id{0};
    int target_id{0};
    entt::id_type target_map{entt::null};
    std::string target_map_name{};
    game::component::StartOffset start_offset{game::component::StartOffset::None};
};

struct NeighborInfo {
    entt::id_type north{entt::null};
    entt::id_type south{entt::null};
    entt::id_type east{entt::null};
    entt::id_type west{entt::null};
};

struct MapInfo {
    std::string name{};              // 无扩展名的地图名称（如 "home_exterior"）
    std::string file_path{};         // 完整路径（如 assets/maps/home_exterior.tmj）
    glm::ivec2 world_pos_px{};       // 世界坐标（像素）
    glm::ivec2 size_px{};            // 地图尺寸（像素）
    entt::id_type id{entt::null};    // hashed map id
    NeighborInfo neighbors{};
    bool in_world{true};             // 是否属于 world 布局
    std::optional<glm::vec3> ambient_override{}; // 地图属性覆盖的环境光（仅室内使用）
};

struct ResourceNodeState {
    glm::ivec2 tile_coord{};
    game::component::ResourceNodeType type_{game::component::ResourceNodeType::Unknown};
    int hit_count_{0};
};

struct MapPersistentState {
    RegistrySnapshot snapshot{};
    std::uint32_t last_updated_day{0};   // 记录快照更新的游戏天数
    std::optional<std::vector<ResourceNodeState>> pending_resource_nodes{};
    std::unordered_set<int> opened_chests{};   // 记录已打开的宝箱（chest_id）
};

struct MapState {
    MapInfo info{};
    std::vector<TriggerInfo> triggers{};
    MapPersistentState persistent{};
};

class WorldState {
    std::string maps_root_{"assets/maps/"};
    std::unordered_map<entt::id_type, MapState> maps_;
    // 异构查找：允许用 std::string_view 查 map，避免每次 find(name) 时分配临时 std::string
    struct StringHash {
        // 标记为“透明”：表示本哈希支持对多种类型算哈希，而不仅是 map 的 key 类型
        using is_transparent = void;
        // 用 string_view 算哈希（查找时传入的通常是 string_view，走这里，零分配）
        std::size_t operator()(std::string_view sv) const { return std::hash<std::string_view>{}(sv); }
        // 用 string 算哈希（插入时 key 是 string，走这里）
        std::size_t operator()(const std::string& s) const { return std::hash<std::string>{}(s); }
    };
    // 第4个模板参数 std::equal_to<>：透明比较器，允许 string_view 与 map 内的 string 直接比较
    std::unordered_map<std::string, entt::id_type, StringHash, std::equal_to<>> name_to_id_;
    entt::id_type current_map_id_{entt::null};

public:
    bool loadFromWorldFile(std::string_view world_path, entt::id_type initial_map_id, std::string_view maps_root = "assets/maps/");

    [[nodiscard]] bool hasMap(std::string_view name) const;
    [[nodiscard]] entt::id_type ensureExternalMap(std::string_view name);
    [[nodiscard]] const MapState* getMapState(entt::id_type map_id) const;
    [[nodiscard]] const MapState* getMapState(std::string_view name) const;
    [[nodiscard]] MapState* getMapStateMutable(entt::id_type map_id);
    [[nodiscard]] MapState* getMapStateMutable(std::string_view name);
    [[nodiscard]] NeighborInfo neighborsOf(entt::id_type map_id) const;
    [[nodiscard]] std::span<const TriggerInfo> outgoingTriggers(entt::id_type map_id) const;
    [[nodiscard]] std::optional<entt::id_type> getNeighbor(entt::id_type map_id, engine::spatial::Direction dir) const;
    [[nodiscard]] const std::unordered_map<entt::id_type, MapState>& maps() const { return maps_; }
    [[nodiscard]] std::unordered_map<entt::id_type, MapState>& mapsMutable() { return maps_; }

    void setCurrentMap(entt::id_type map_id) { current_map_id_ = map_id; }
    [[nodiscard]] entt::id_type getCurrentMap() const { return current_map_id_; }

private:
    void resolveAdjacency();
    static std::string stripExtension(std::string_view filename);
};

} // namespace game::world
