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

enum class Scope : std::uint8_t {
    None = 0,
    OneEnemy,
    AllEnemies,
    OneAlly,
    AllAllies,
    Self
};

enum class DamageType : std::uint8_t {
    None = 0,
    HpDamage,
    MpDamage,
    HpRecover,
    MpRecover,
    HpDrain,
    MpDrain
};

enum class HitType : std::uint8_t {
    Certain = 0,
    Physical,
    Magical
};

enum class TraitType : std::uint8_t {
    ParamRate = 0,
    ElementRate,
    StateRate,
    StateImmunity,
    Unknown
};

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
