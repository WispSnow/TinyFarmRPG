#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace game::data {

enum class ParamIndex : std::uint8_t {
    Mhp = 0,
    Mmp,
    Atk,
    Def,
    Mat,
    Mdf,
    Agi,
    Luk,
    Count
};

inline constexpr std::size_t kParamCount = static_cast<std::size_t>(ParamIndex::Count);
using ParamArray = std::array<int, kParamCount>;

/// @brief 角色范围，例如单个还是全部敌人/盟友。
enum class Scope : std::uint8_t {
    None = 0,
    OneEnemy,
    AllEnemies,
    OneAlly,
    AllAllies,
    Self
};

/// @brief 技能或状态的伤害类型，可针对HP/MP造成伤害或恢复。
enum class DamageType : std::uint8_t {
    None = 0,
    HpDamage,
    MpDamage,
    HpRecover,
    MpRecover,
    HpDrain,
    MpDrain
};

/// @brief 技能的命中类型，分物理和魔法
enum class HitType : std::uint8_t {
    Certain = 0,
    Physical,
    Magical
};

/// @brief 特性类型，例如参数倍率、元素倍率、状态倍率或状态免疫。
enum class TraitType : std::uint8_t {
    ParamRate = 0,
    ElementRate,
    StateRate,
    StateImmunity,
    Unknown
};

/// @brief 技能或状态的效果类型，例如恢复HP/MP、增减状态或增减道具。
enum class EffectType : std::uint8_t {
    RecoverHp = 0,
    RecoverMp,
    AddState,
    RemoveState,
    AddItem,
    Unknown
};

[[nodiscard]] const char* toString(ParamIndex value);
[[nodiscard]] const char* toString(Scope value);
[[nodiscard]] const char* toString(DamageType value);
[[nodiscard]] const char* toString(HitType value);
[[nodiscard]] const char* toString(TraitType value);
[[nodiscard]] const char* toString(EffectType value);

[[nodiscard]] std::optional<ParamIndex> paramIndexFromString(std::string_view value);
[[nodiscard]] std::optional<Scope> scopeFromString(std::string_view value);
[[nodiscard]] std::optional<DamageType> damageTypeFromString(std::string_view value);
[[nodiscard]] std::optional<HitType> hitTypeFromString(std::string_view value);
[[nodiscard]] std::optional<TraitType> traitTypeFromString(std::string_view value);
[[nodiscard]] std::optional<EffectType> effectTypeFromString(std::string_view value);

} // namespace game::data
