#pragma once

#include "game/data/rpg_types.h"

#include <entt/core/fwd.hpp>
#include <glm/vec2.hpp>

#include <array>
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

struct SkillPresentationData {
    SkillMotionStyle motion_style_{SkillMotionStyle::Auto};
    float duration_seconds_{0.0F};
    float impact_time_seconds_{0.0F};
    float recovery_duration_seconds_{0.0F};
    float target_vfx_tail_seconds_{0.0F};
    /// @brief 目标位置播放的 VFX 语义 ID；为空时技能不触发目标特效。
    std::string target_vfx_id_{};
    /// @brief target_vfx_id_ 的哈希值，供 BattleScene 直接提交 PlayVfxCommand。
    entt::id_type target_vfx_id_hash_{};
    /// @brief 目标命中时播放的全局音效语义 ID；为空时不触发配置音效。
    std::string target_sfx_id_{};
    /// @brief target_sfx_id_ 的哈希值，供 BattleScene 提交 PlaySoundEvent。
    entt::id_type target_sfx_id_hash_{};
    /// @brief 目标特效缩放倍率，默认使用 Effekseer 资源原始尺寸。
    float target_vfx_scale_{1.0F};
    /// @brief 目标特效相对战斗单位脚底锚点的屏幕逻辑坐标偏移。
    glm::vec2 target_vfx_offset_{0.0F, 0.0F};
    bool configured_{false};
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
    SkillPresentationData presentation_{};
};

enum class ParamCurveShape : std::uint8_t {
    Linear = 0,
    Early,
    Late
};

struct ExpCurveData {
    int basis_{30};
    int extra_{20};
    int acc_a_{30};
    int acc_b_{30};
};

struct ParamCurveData {
    int level_1_{1};
    int level_max_{1};
    ParamCurveShape shape_{ParamCurveShape::Linear};
    bool configured_{false};
};

struct ClassData {
    std::string id_{};
    entt::id_type id_hash_{};
    std::string display_name_{};
    ExpCurveData exp_curve_{};
    ParamArray base_params_{};
    std::array<ParamCurveData, kParamCount> param_curves_{};
    bool has_param_curves_{false};
};

struct PortraitRefData {
    std::string path_{};
    entt::id_type path_hash_{};
    std::string decorator_{};
    int x_{0};
    int y_{0};
    int width_{0};
    int height_{0};

    [[nodiscard]] bool valid() const { return !path_.empty() && width_ > 0 && height_ > 0; }
};

struct BattleVisualData {
    std::string sprite_blueprint_id_{};
    entt::id_type sprite_blueprint_id_hash_{};
    std::string idle_animation_{"idle_right"};
    float sprite_scale_{1.0F};
    /// @brief 战斗阴影相对默认脚底锚点的屏幕逻辑坐标偏移。
    glm::vec2 shadow_offset_{0.0F, 0.0F};

    [[nodiscard]] bool valid() const { return !sprite_blueprint_id_.empty(); }
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
    BattleVisualData battle_visual_{};
    PortraitRefData portrait_{};
};

struct EquipmentData {
    std::string item_id_{};
    entt::id_type item_id_hash_{};
    EquipmentSlotId slot_{EquipmentSlotId::Unknown};
    ParamArray param_bonuses_{};
    std::vector<std::string> allowed_classes_{};
    std::vector<std::string> allowed_actors_{};
};

struct StateData {
    std::string id_{};
    entt::id_type id_hash_{};
    std::string display_name_{};
    std::string description_{};
    std::string icon_key_{};
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
