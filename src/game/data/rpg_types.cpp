#include "game/data/rpg_types.h"

namespace game::data {

const char* toString(const ParamIndex value) {
    switch (value) {
        case ParamIndex::Mhp:
            return "mhp";
        case ParamIndex::Mmp:
            return "mmp";
        case ParamIndex::Atk:
            return "atk";
        case ParamIndex::Def:
            return "def";
        case ParamIndex::Mat:
            return "mat";
        case ParamIndex::Mdf:
            return "mdf";
        case ParamIndex::Agi:
            return "agi";
        case ParamIndex::Luk:
            return "luk";
        case ParamIndex::Count:
            break;
    }
    return "unknown";
}

const char* toString(const Scope value) {
    switch (value) {
        case Scope::None:
            return "none";
        case Scope::OneEnemy:
            return "one_enemy";
        case Scope::AllEnemies:
            return "all_enemies";
        case Scope::OneAlly:
            return "one_ally";
        case Scope::AllAllies:
            return "all_allies";
        case Scope::Self:
            return "self";
    }
    return "none";
}

const char* toString(const DamageType value) {
    switch (value) {
        case DamageType::None:
            return "none";
        case DamageType::HpDamage:
            return "hp_damage";
        case DamageType::MpDamage:
            return "mp_damage";
        case DamageType::HpRecover:
            return "hp_recover";
        case DamageType::MpRecover:
            return "mp_recover";
        case DamageType::HpDrain:
            return "hp_drain";
        case DamageType::MpDrain:
            return "mp_drain";
    }
    return "none";
}

const char* toString(const HitType value) {
    switch (value) {
        case HitType::Certain:
            return "certain";
        case HitType::Physical:
            return "physical";
        case HitType::Magical:
            return "magical";
    }
    return "certain";
}

const char* toString(const SkillMotionStyle value) {
    switch (value) {
        case SkillMotionStyle::Auto:
            return "auto";
        case SkillMotionStyle::WeaponAttack:
            return "weapon_attack";
        case SkillMotionStyle::Cast:
            return "cast";
        case SkillMotionStyle::Guard:
            return "guard";
        case SkillMotionStyle::Escape:
            return "escape";
        case SkillMotionStyle::Simple:
            return "simple";
    }
    return "auto";
}

const char* toString(const TraitType value) {
    switch (value) {
        case TraitType::ParamRate:
            return "param_rate";
        case TraitType::ElementRate:
            return "element_rate";
        case TraitType::StateRate:
            return "state_rate";
        case TraitType::StateImmunity:
            return "state_immunity";
        case TraitType::Unknown:
            return "unknown";
    }
    return "unknown";
}

const char* toString(const EffectType value) {
    switch (value) {
        case EffectType::RecoverHp:
            return "recover_hp";
        case EffectType::RecoverMp:
            return "recover_mp";
        case EffectType::AddState:
            return "add_state";
        case EffectType::RemoveState:
            return "remove_state";
        case EffectType::AddItem:
            return "add_item";
        case EffectType::Unknown:
            return "unknown";
    }
    return "unknown";
}

const char* toString(const EquipmentSlotId value) {
    switch (value) {
        case EquipmentSlotId::Weapon:
            return "weapon";
        case EquipmentSlotId::Offhand:
            return "offhand";
        case EquipmentSlotId::Head:
            return "head";
        case EquipmentSlotId::Body:
            return "body";
        case EquipmentSlotId::Accessory:
            return "accessory";
        case EquipmentSlotId::Unknown:
            return "unknown";
    }
    return "unknown";
}

std::optional<ParamIndex> paramIndexFromString(const std::string_view value) {
    if (value == "mhp") {
        return ParamIndex::Mhp;
    }
    if (value == "mmp") {
        return ParamIndex::Mmp;
    }
    if (value == "atk") {
        return ParamIndex::Atk;
    }
    if (value == "def") {
        return ParamIndex::Def;
    }
    if (value == "mat") {
        return ParamIndex::Mat;
    }
    if (value == "mdf") {
        return ParamIndex::Mdf;
    }
    if (value == "agi") {
        return ParamIndex::Agi;
    }
    if (value == "luk") {
        return ParamIndex::Luk;
    }
    return std::nullopt;
}

std::optional<Scope> scopeFromString(const std::string_view value) {
    if (value == "none") {
        return Scope::None;
    }
    if (value == "one_enemy") {
        return Scope::OneEnemy;
    }
    if (value == "all_enemies") {
        return Scope::AllEnemies;
    }
    if (value == "one_ally") {
        return Scope::OneAlly;
    }
    if (value == "all_allies") {
        return Scope::AllAllies;
    }
    if (value == "self") {
        return Scope::Self;
    }
    return std::nullopt;
}

std::optional<DamageType> damageTypeFromString(const std::string_view value) {
    if (value == "none") {
        return DamageType::None;
    }
    if (value == "hp_damage") {
        return DamageType::HpDamage;
    }
    if (value == "mp_damage") {
        return DamageType::MpDamage;
    }
    if (value == "hp_recover") {
        return DamageType::HpRecover;
    }
    if (value == "mp_recover") {
        return DamageType::MpRecover;
    }
    if (value == "hp_drain") {
        return DamageType::HpDrain;
    }
    if (value == "mp_drain") {
        return DamageType::MpDrain;
    }
    return std::nullopt;
}

std::optional<HitType> hitTypeFromString(const std::string_view value) {
    if (value == "certain") {
        return HitType::Certain;
    }
    if (value == "physical") {
        return HitType::Physical;
    }
    if (value == "magical") {
        return HitType::Magical;
    }
    return std::nullopt;
}

std::optional<SkillMotionStyle> skillMotionStyleFromString(const std::string_view value) {
    if (value == "auto") {
        return SkillMotionStyle::Auto;
    }
    if (value == "weapon_attack") {
        return SkillMotionStyle::WeaponAttack;
    }
    if (value == "cast") {
        return SkillMotionStyle::Cast;
    }
    if (value == "guard") {
        return SkillMotionStyle::Guard;
    }
    if (value == "escape") {
        return SkillMotionStyle::Escape;
    }
    if (value == "simple") {
        return SkillMotionStyle::Simple;
    }
    return std::nullopt;
}

std::optional<TraitType> traitTypeFromString(const std::string_view value) {
    if (value == "param_rate") {
        return TraitType::ParamRate;
    }
    if (value == "element_rate") {
        return TraitType::ElementRate;
    }
    if (value == "state_rate") {
        return TraitType::StateRate;
    }
    if (value == "state_immunity") {
        return TraitType::StateImmunity;
    }
    return std::nullopt;
}

std::optional<EffectType> effectTypeFromString(const std::string_view value) {
    if (value == "recover_hp") {
        return EffectType::RecoverHp;
    }
    if (value == "recover_mp") {
        return EffectType::RecoverMp;
    }
    if (value == "add_state") {
        return EffectType::AddState;
    }
    if (value == "remove_state") {
        return EffectType::RemoveState;
    }
    if (value == "add_item") {
        return EffectType::AddItem;
    }
    return std::nullopt;
}

std::optional<EquipmentSlotId> equipmentSlotIdFromString(const std::string_view value) {
    if (value == "weapon") {
        return EquipmentSlotId::Weapon;
    }
    if (value == "offhand") {
        return EquipmentSlotId::Offhand;
    }
    if (value == "head") {
        return EquipmentSlotId::Head;
    }
    if (value == "body") {
        return EquipmentSlotId::Body;
    }
    if (value == "accessory") {
        return EquipmentSlotId::Accessory;
    }
    return std::nullopt;
}

} // namespace game::data
