#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json_fwd.hpp>

namespace game::save {

constexpr std::uint32_t SAVE_SCHEMA_VERSION = 2;

namespace json_keys {
inline constexpr std::string_view SCHEMA_VERSION = "schema_version";
inline constexpr std::string_view TIMESTAMP = "timestamp";
inline constexpr std::string_view WORLD_FILE = "world_file";
inline constexpr std::string_view GAME_TIME = "game_time";
inline constexpr std::string_view DAY = "day";
} // namespace json_keys

struct Vec2f {
    float x{0.0f};
    float y{0.0f};
};

struct Vec2i {
    int x{0};
    int y{0};
};

struct GameTimeSaveData {
    std::uint32_t day{1};
    float hour{6.0f};
    float minute{0.0f};
    float time_scale{1.0f};
    bool paused{false};
};

struct ItemStackSaveData {
    std::uint32_t item_id{0};
    int count{0};
};

struct InventorySaveData {
    int active_page{0};
    std::vector<ItemStackSaveData> slots{};
};

struct HotbarSaveData {
    int active_slot{0};
    std::vector<int> inventory_slot_indices{};
};

struct PlayerStateSaveData {
    std::string action{"idle"};
    std::string direction{"down"};
};

struct PlayerSaveData {
    std::string map_name{};
    Vec2f position{};
    PlayerStateSaveData state{};
    InventorySaveData inventory{};
    HotbarSaveData hotbar{};
    int hp{100};
    int gold{0};
};

struct CropSaveData {
    Vec2i tile{};
    std::string crop_type{"unknown"};
    std::string growth_stage{"seed"};
    std::uint32_t planted_day{0};
    int stage_countdown{0};
};

struct ResourceNodeSaveData {
    Vec2i tile{};
    std::string node_type{"unknown"};
    int hit_count{0};
    int hits_to_break{0};
    std::uint32_t drop_item_id{0};
};

struct MapSaveData {
    std::string map_name{};
    std::uint32_t last_updated_day{0};
    std::vector<int> opened_chests{};   // 已打开的宝箱（chest_id）
    std::vector<Vec2i> tilled_tiles{};
    std::vector<Vec2i> wet_tiles{};
    std::vector<CropSaveData> crops{};
    std::optional<std::vector<ResourceNodeSaveData>> resource_nodes{};
};

struct SaveData {
    std::uint32_t schema_version{SAVE_SCHEMA_VERSION};
    std::optional<std::string> timestamp{};
    std::optional<std::string> world_file{};

    GameTimeSaveData game_time{};
    PlayerSaveData player{};
    std::vector<MapSaveData> maps{};
};

[[nodiscard]] nlohmann::json serialize(const SaveData& data);
[[nodiscard]] bool deserialize(const nlohmann::json& json, SaveData& out, std::string& out_error);

} // namespace game::save
