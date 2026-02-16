#include "world_state.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <fstream>
#include <filesystem>

namespace game::world {

bool WorldState::loadFromWorldFile(std::string_view world_path, entt::id_type initial_map_id, std::string_view maps_root) {
    maps_.clear();
    name_to_id_.clear();
    current_map_id_ = initial_map_id;
    maps_root_ = maps_root;

    std::ifstream file{std::filesystem::path{std::string{world_path}}};
    if (!file.is_open()) {
        spdlog::error("WorldState: 无法打开 world 文件: {}", world_path);
        return false;
    }

    nlohmann::json json;
    try {
        file >> json;
    } catch (const std::exception& e) {
        spdlog::error("WorldState: 解析 world 文件失败: {}", e.what());
        return false;
    }

    if (!json.contains("maps") || !json["maps"].is_array()) {
        spdlog::error("WorldState: world 文件缺少 maps 数组");
        return false;
    }

    const auto& maps_json = json["maps"];
    for (const auto& entry : maps_json) {
        if (!entry.contains("fileName")) {
            continue;
        }
        MapState state{};
        std::string file_name = entry.value("fileName", "");
        state.info.name = stripExtension(file_name);
        state.info.file_path = std::string(maps_root) + file_name;
        state.info.world_pos_px = {entry.value("x", 0), entry.value("y", 0)};
        state.info.size_px = {entry.value("width", 0), entry.value("height", 0)};
        state.info.id = entt::hashed_string(state.info.name.c_str()).value();
        state.info.in_world = true;

        name_to_id_.emplace(state.info.name, state.info.id);
        maps_.emplace(state.info.id, std::move(state));
    }

    resolveAdjacency();

    if (maps_.empty()) {
        spdlog::error("WorldState: 加载 world 文件失败，地图数量为0");
        return false;
    }
    spdlog::info("WorldState: 成功加载 world 文件 '{}', 地图数量: {}", world_path, maps_.size());
    return true;
}

bool WorldState::hasMap(std::string_view name) const {
    return name_to_id_.contains(name);
}

entt::id_type WorldState::ensureExternalMap(std::string_view name) {
    auto it = name_to_id_.find(name);
    if (it != name_to_id_.end()) {
        return it->second;
    }
    MapState state{};
    state.info.name = std::string(name);
    state.info.file_path = maps_root_ + state.info.name + ".tmj";
    state.info.id = entt::hashed_string(state.info.name.c_str()).value();
    state.info.in_world = false;
    auto id = state.info.id;
    name_to_id_.emplace(state.info.name, id);
    maps_.emplace(id, std::move(state));
    spdlog::info("WorldState: 注册外部地图 '{}'", name);
    return id;
}

const MapState* WorldState::getMapState(entt::id_type map_id) const {
    if (auto it = maps_.find(map_id); it != maps_.end()) {
        return &it->second;
    }
    return nullptr;
}

const MapState* WorldState::getMapState(std::string_view name) const {
    auto it = name_to_id_.find(name);
    if (it == name_to_id_.end()) return nullptr;
    return getMapState(it->second);
}

MapState* WorldState::getMapStateMutable(entt::id_type map_id) {
    if (auto it = maps_.find(map_id); it != maps_.end()) {
        return &it->second;
    }
    return nullptr;
}

MapState* WorldState::getMapStateMutable(std::string_view name) {
    auto it = name_to_id_.find(name);
    if (it == name_to_id_.end()) return nullptr;
    return getMapStateMutable(it->second);
}

std::optional<entt::id_type> WorldState::getNeighbor(entt::id_type map_id, engine::spatial::Direction dir) const {
    if (auto it = maps_.find(map_id); it != maps_.end()) {
        const auto& neighbors = it->second.info.neighbors;
        switch (dir) {
            case engine::spatial::Direction::North: return neighbors.north == entt::null ? std::optional<entt::id_type>{} : neighbors.north;
            case engine::spatial::Direction::South: return neighbors.south == entt::null ? std::optional<entt::id_type>{} : neighbors.south;
            case engine::spatial::Direction::East:  return neighbors.east  == entt::null ? std::optional<entt::id_type>{} : neighbors.east;
            case engine::spatial::Direction::West:  return neighbors.west  == entt::null ? std::optional<entt::id_type>{} : neighbors.west;
        }
    }
    return std::nullopt;
}

NeighborInfo WorldState::neighborsOf(entt::id_type map_id) const {
    if (const auto* state = getMapState(map_id)) {
        return state->info.neighbors;
    }
    return NeighborInfo{};
}

std::span<const TriggerInfo> WorldState::outgoingTriggers(entt::id_type map_id) const {
    if (const auto* state = getMapState(map_id)) {
        return state->triggers;
    }
    return {};
}

void WorldState::resolveAdjacency() {
    for (auto& [id, map] : maps_) {
        for (auto& [other_id, other] : maps_) {
            if (id == other_id) continue;
            const auto& a = map.info;
            const auto& b = other.info;

            const bool same_row = a.world_pos_px.y == b.world_pos_px.y;
            const bool same_col = a.world_pos_px.x == b.world_pos_px.x;

            if (same_row && (a.world_pos_px.x + a.size_px.x == b.world_pos_px.x)) {
                map.info.neighbors.east = other_id;
            }
            if (same_row && (b.world_pos_px.x + b.size_px.x == a.world_pos_px.x)) {
                map.info.neighbors.west = other_id;
            }
            if (same_col && (a.world_pos_px.y + a.size_px.y == b.world_pos_px.y)) {
                map.info.neighbors.south = other_id;
            }
            if (same_col && (b.world_pos_px.y + b.size_px.y == a.world_pos_px.y)) {
                map.info.neighbors.north = other_id;
            }
        }
    }

    // 双向校验提示
    for (const auto& [id, map] : maps_) {
        const auto check = [&](engine::spatial::Direction dir, entt::id_type neighbor_id) {
            if (neighbor_id == entt::null) return;
            const auto* neighbor = getMapState(neighbor_id);
            if (!neighbor) return;
            auto opp = neighbor->info.neighbors;
            entt::id_type back = entt::null;
            switch (dir) {
                case engine::spatial::Direction::North: back = opp.south; break;
                case engine::spatial::Direction::South: back = opp.north; break;
                case engine::spatial::Direction::East:  back = opp.west;  break;
                case engine::spatial::Direction::West:  back = opp.east;  break;
            }
            if (back != id) {
                spdlog::warn("WorldState: 地图 '{}' 邻接方向 {} 未找到对称记录",
                    map.info.name,
                    static_cast<int>(dir));
            }
        };

        check(engine::spatial::Direction::North, map.info.neighbors.north);
        check(engine::spatial::Direction::South, map.info.neighbors.south);
        check(engine::spatial::Direction::East, map.info.neighbors.east);
        check(engine::spatial::Direction::West, map.info.neighbors.west);
    }
}

std::string WorldState::stripExtension(std::string_view filename) {
    return std::filesystem::path{filename}.stem().string();
}

} // namespace game::world
