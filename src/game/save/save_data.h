#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <nlohmann/json_fwd.hpp>

namespace game::save {

constexpr std::uint32_t SAVE_SCHEMA_VERSION = 4;

namespace json_keys {
inline constexpr std::string_view SCHEMA_VERSION = "schema_version";
inline constexpr std::string_view TIMESTAMP = "timestamp";
inline constexpr std::string_view WORLD_FILE = "world_file";
inline constexpr std::string_view GAME_TIME = "game_time";
inline constexpr std::string_view DAY = "day";
inline constexpr std::string_view QUEST_STATE = "quest_state";
inline constexpr std::string_view SKILL_STATE = "skill_state";
inline constexpr std::string_view APPEARANCE_STATE = "appearance_state";
inline constexpr std::string_view PARTY_STATE = "party_state";
inline constexpr std::string_view EQUIPMENT_STATE = "equipment_state";
inline constexpr std::string_view PARTY_RUNTIME_STATE = "party_runtime_state";
inline constexpr std::string_view COMBAT_STATE = "combat_state";
inline constexpr std::string_view ACTIVE_QUESTS = "active_quests";
inline constexpr std::string_view COMPLETED_QUESTS = "completed_quests";
inline constexpr std::string_view OBJECTIVE_PROGRESS = "objective_progress";
inline constexpr std::string_view LEARNED_SKILLS = "learned_skills";
inline constexpr std::string_view SKILL_LEVELS = "skill_levels";
inline constexpr std::string_view SKILL_COOLDOWNS = "skill_cooldowns";
inline constexpr std::string_view PENDING_BATTLE = "pending_battle";
inline constexpr std::string_view TROOP_ID = "troop_id";
inline constexpr std::string_view ACTOR_IDS = "actor_ids";
inline constexpr std::string_view RECRUITED_ACTOR_IDS = "recruited_actor_ids";
inline constexpr std::string_view ACTIVE_ACTOR_IDS = "active_actor_ids";
inline constexpr std::string_view LOADOUTS = "loadouts";
inline constexpr std::string_view ACTOR_STATES = "actor_states";
inline constexpr std::string_view CURRENT_HP = "current_hp";
inline constexpr std::string_view CURRENT_MP = "current_mp";
inline constexpr std::string_view ITEM_STOCKS = "item_stocks";
inline constexpr std::string_view ESCAPE_ATTEMPT_COUNT = "escape_attempt_count";
inline constexpr std::string_view DEFEATED_ENCOUNTERS = "defeated_encounters";
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
    std::uint64_t item_id{0};
    int count{0};
};

struct InventorySaveData {
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
    std::uint64_t drop_item_id{0};
};

struct MapSaveData {
    std::string map_name{};
    std::uint32_t last_updated_day{0};
    std::vector<int> opened_chests{};   // 已打开的宝箱（chest_id）
    std::vector<int> defeated_encounters{}; // 已击败的一次性遭遇（encounter_id）
    std::vector<Vec2i> tilled_tiles{};
    std::vector<Vec2i> wet_tiles{};
    std::vector<CropSaveData> crops{};
    std::optional<std::vector<ResourceNodeSaveData>> resource_nodes{};
};

struct QuestStateSaveData {
    std::vector<std::string> active_quests{};
    std::vector<std::string> completed_quests{};
    std::unordered_map<std::string, int> objective_progress{};
};

struct SkillStateSaveData {
    std::vector<std::string> learned_skills{};
    std::unordered_map<std::string, int> skill_levels{};
    std::unordered_map<std::string, int> skill_cooldowns{};
};

struct AppearanceStateSaveData {
    std::string gender{"male"};
    std::unordered_map<std::string, std::string> slots{};
};

struct PartyStateSaveData {
    std::vector<std::string> recruited_actor_ids{"actor.player"};
    std::vector<std::string> active_actor_ids{"actor.player"};
};

struct ActorEquipmentSaveData {
    std::unordered_map<std::string, std::uint64_t> slots{};
};

struct EquipmentStateSaveData {
    std::unordered_map<std::string, ActorEquipmentSaveData> loadouts{};
};

struct ActorRuntimeStateSaveData {
    int current_hp{0};
    int current_mp{0};
};

struct PartyRuntimeStateSaveData {
    std::unordered_map<std::string, ActorRuntimeStateSaveData> actor_states{};
};

struct CombatStateSaveData {
    bool pending_battle{false};
    std::string troop_id{};
    std::vector<std::string> actor_ids{};
    std::unordered_map<std::string, int> item_stocks{};
    std::uint32_t escape_attempt_count{0};
};

struct SaveData {
    std::uint32_t schema_version{SAVE_SCHEMA_VERSION};
    std::optional<std::string> timestamp{};
    std::optional<std::string> world_file{};

    GameTimeSaveData game_time{};
    PlayerSaveData player{};
    std::vector<MapSaveData> maps{};
    QuestStateSaveData quest_state{};
    SkillStateSaveData skill_state{};
    AppearanceStateSaveData appearance_state{};
    PartyStateSaveData party_state{};
    EquipmentStateSaveData equipment_state{};
    PartyRuntimeStateSaveData party_runtime_state{};
    CombatStateSaveData combat_state{};
};

[[nodiscard]] nlohmann::json serialize(const SaveData& data);
[[nodiscard]] bool deserialize(const nlohmann::json& json, SaveData& out, std::string& out_error);

} // namespace game::save
