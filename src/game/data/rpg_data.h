#pragma once

#include "game/data/rpg_types.h"

#include <entt/core/fwd.hpp>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace game::data {

struct TraitData {
    TraitType type{TraitType::Unknown};
    std::string target{};
    float value{0.0F};
};

struct EffectData {
    EffectType type{EffectType::Unknown};
    std::string target_id{};
    float value1{0.0F};
    float value2{0.0F};
    int count{0};
};

struct DamageFormulaData {
    DamageType type{DamageType::None};
    std::string formula{};
    int variance{0};
    bool critical{false};
};

struct SkillData {
    std::string id_{};
    entt::id_type id_hash_{};
    std::string display_name_{};
    std::string description_{};
    Scope scope_{Scope::None};
    HitType hit_type_{HitType::Certain};
    int mp_cost_{0};
    int success_rate_{100};
    int repeats_{1};
    DamageFormulaData damage_{};
    std::vector<EffectData> effects_{};
};

struct ClassData {
    std::string id_{};
    entt::id_type id_hash_{};
    std::string display_name_{};
    ParamArray base_params_{};
};

struct PortraitRefData {
    std::string path_{};
    entt::id_type path_hash_{};
    int x_{0};
    int y_{0};
    int width_{0};
    int height_{0};

    [[nodiscard]] bool valid() const { return !path_.empty() && width_ > 0 && height_ > 0; }
};

struct ActorData {
    std::string id_{};
    entt::id_type id_hash_{};
    std::string display_name_{};
    std::string class_id_{};
    int initial_level_{1};
    int max_level_{99};
    std::vector<std::string> skill_ids_{};
    std::string map_actor_id_{};
    entt::id_type map_actor_id_hash_{};
    PortraitRefData portrait_{};
};

struct StateData {
    std::string id_{};
    entt::id_type id_hash_{};
    std::string display_name_{};
    int priority_{50};
    int min_turns_{1};
    int max_turns_{1};
    std::vector<TraitData> traits_{};
};

struct EnemyDropData {
    std::string item_id_{};
    float chance_{0.0F};
};

struct EnemyActionData {
    std::string skill_id_{};
    int rating_{0};
};

struct BattleVisualData {
    std::string sprite_blueprint_id_{};
    entt::id_type sprite_blueprint_id_hash_{};
    std::string idle_animation_{"idle_right"};
    float scale_{1.0F};

    [[nodiscard]] bool valid() const { return !sprite_blueprint_id_.empty(); }
};

struct EnemyData {
    std::string id_{};
    entt::id_type id_hash_{};
    std::string display_name_{};
    ParamArray params_{};
    int exp_reward_{0};
    int gold_reward_{0};
    std::vector<EnemyDropData> drops_{};
    std::vector<EnemyActionData> actions_{};
    BattleVisualData battle_visual_{};
};

struct TroopMemberData {
    std::string enemy_id_{};
    float x_{0.0F};
    float y_{0.0F};
};

struct TroopData {
    std::string id_{};
    entt::id_type id_hash_{};
    std::string display_name_{};
    std::string battle_background_id_{};
    std::vector<TroopMemberData> members_{};
};

struct RpgManifest {
    std::uint32_t schema_version{0};
    std::unordered_map<std::string, std::uint32_t> content_versions_{};
    std::unordered_map<std::string, bool> features_{};
    std::unordered_map<std::string, std::string> files_{};
};

} // namespace game::data
