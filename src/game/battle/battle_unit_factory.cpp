#include "game/battle/battle_unit_factory.h"

#include "game/data/rpg_catalog.h"
#include "game/data/rpg_types.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace game::battle {
namespace {

constexpr std::uint32_t kPlayerBattleUnitIdStart = 1U;
constexpr std::uint32_t kEnemyBattleUnitIdStart = 1001U;

[[nodiscard]] int clampPositiveStat(const int value) {
    return std::max(1, value);
}

[[nodiscard]] int paramValue(const game::data::ParamArray& params, const game::data::ParamIndex index) {
    return params[static_cast<std::size_t>(index)];
}

[[nodiscard]] std::string buildEnemyDisplayName(const game::data::EnemyData& enemy,
                                                std::unordered_map<std::string, int>& name_counts) {
    const std::string base_name = enemy.display_name_.empty() ? enemy.id_ : enemy.display_name_;
    const int ordinal = ++name_counts[base_name];
    if (ordinal <= 1) {
        return base_name;
    }
    return base_name + " " + std::to_string(ordinal);
}

[[nodiscard]] std::vector<std::string> collectEnemySkillIds(const game::data::EnemyData& enemy) {
    std::vector<std::string> skill_ids{};
    skill_ids.reserve(enemy.actions_.size());
    for (const auto& action : enemy.actions_) {
        if (std::find(skill_ids.begin(), skill_ids.end(), action.skill_id_) != skill_ids.end()) {
            continue;
        }
        skill_ids.push_back(action.skill_id_);
    }
    return skill_ids;
}

[[nodiscard]] BattlePortrait toBattlePortrait(const game::data::PortraitRefData& portrait) {
    if (!portrait.valid()) {
        return {};
    }
    return BattlePortrait{
        .path = portrait.path_,
        .x = portrait.x_,
        .y = portrait.y_,
        .width = portrait.width_,
        .height = portrait.height_,
    };
}

} // namespace

bool buildBattleUnitsFromCatalog(const game::data::RpgCatalog& catalog,
                                 const BattleUnitBuildOptions& options,
                                 std::vector<BattleUnit>& out_units,
                                 std::string& out_error) {
    std::vector<const game::data::ActorData*> actors{};
    if (options.actor_ids.empty()) {
        actors = catalog.listActors();
    } else {
        actors.reserve(options.actor_ids.size());
        for (const auto& actor_id : options.actor_ids) {
            const auto* actor = catalog.findActor(actor_id);
            if (!actor) {
                out_error = "RPG catalog does not contain actor '" + actor_id + "'";
                return false;
            }
            actors.push_back(actor);
        }
    }

    std::vector<BattleUnit> player_units{};
    player_units.reserve(actors.size());
    std::uint32_t next_player_id = kPlayerBattleUnitIdStart;
    for (const auto* actor : actors) {
        const auto* klass = catalog.findClass(actor->class_id_);
        if (!klass) {
            continue;
        }

        const int max_hp = clampPositiveStat(paramValue(klass->base_params_, game::data::ParamIndex::Mhp));
        const int max_mp = clampPositiveStat(paramValue(klass->base_params_, game::data::ParamIndex::Mmp));
        const int atk = clampPositiveStat(paramValue(klass->base_params_, game::data::ParamIndex::Atk));
        const int def = clampPositiveStat(paramValue(klass->base_params_, game::data::ParamIndex::Def));
        const int mat = clampPositiveStat(paramValue(klass->base_params_, game::data::ParamIndex::Mat));
        const int mdf = clampPositiveStat(paramValue(klass->base_params_, game::data::ParamIndex::Mdf));
        const int agi = clampPositiveStat(paramValue(klass->base_params_, game::data::ParamIndex::Agi));
        const int luk = clampPositiveStat(paramValue(klass->base_params_, game::data::ParamIndex::Luk));
        const std::string actor_name = actor->display_name_.empty() ? actor->id_ : actor->display_name_;
        player_units.push_back(BattleUnit{
            .id = next_player_id++,
            .name = actor_name,
            .side = BattleSide::Player,
            .hp = max_hp,
            .max_hp = max_hp,
            .mp = max_mp,
            .max_mp = max_mp,
            .attack = atk,
            .defense = def,
            .magic_attack = mat,
            .magic_defense = mdf,
            .speed = agi,
            .luck = luk,
            .skill_ids = actor->skill_ids_,
            .source_actor_id = actor->id_,
            .portrait = toBattlePortrait(actor->portrait_)
        });
    }

    if (player_units.empty()) {
        out_error = "RPG catalog does not contain any player actor/class pair.";
        return false;
    }

    const game::data::TroopData* selected_troop = nullptr;
    if (!options.troop_id.empty()) {
        selected_troop = catalog.findTroop(options.troop_id);
        if (!selected_troop) {
            out_error = "RPG catalog does not contain troop '" + options.troop_id + "'";
            return false;
        }
    } else {
        const auto troops = catalog.listTroops();
        for (const auto* troop : troops) {
            if (!troop->members_.empty()) {
                selected_troop = troop;
                break;
            }
        }
    }

    if (!selected_troop) {
        out_error = "RPG catalog does not contain a troop with members.";
        return false;
    }

    std::vector<BattleUnit> enemy_units{};
    enemy_units.reserve(selected_troop->members_.size());
    std::unordered_map<std::string, int> enemy_name_counts{};
    std::uint32_t next_enemy_id = kEnemyBattleUnitIdStart;
    for (const auto& member : selected_troop->members_) {
        const auto* enemy = catalog.findEnemy(member.enemy_id_);
        if (!enemy) {
            continue;
        }

        const int max_hp = clampPositiveStat(paramValue(enemy->params_, game::data::ParamIndex::Mhp));
        const int max_mp = clampPositiveStat(paramValue(enemy->params_, game::data::ParamIndex::Mmp));
        const int atk = clampPositiveStat(paramValue(enemy->params_, game::data::ParamIndex::Atk));
        const int def = clampPositiveStat(paramValue(enemy->params_, game::data::ParamIndex::Def));
        const int mat = clampPositiveStat(paramValue(enemy->params_, game::data::ParamIndex::Mat));
        const int mdf = clampPositiveStat(paramValue(enemy->params_, game::data::ParamIndex::Mdf));
        const int agi = clampPositiveStat(paramValue(enemy->params_, game::data::ParamIndex::Agi));
        const int luk = clampPositiveStat(paramValue(enemy->params_, game::data::ParamIndex::Luk));
        enemy_units.push_back(BattleUnit{
            .id = next_enemy_id++,
            .name = buildEnemyDisplayName(*enemy, enemy_name_counts),
            .side = BattleSide::Enemy,
            .hp = max_hp,
            .max_hp = max_hp,
            .mp = max_mp,
            .max_mp = max_mp,
            .attack = atk,
            .defense = def,
            .magic_attack = mat,
            .magic_defense = mdf,
            .speed = agi,
            .luck = luk,
            .skill_ids = collectEnemySkillIds(*enemy),
            .source_enemy_id = enemy->id_
        });
    }

    if (enemy_units.empty()) {
        out_error = "RPG catalog troop '" + selected_troop->id_ + "' does not produce enemy units.";
        return false;
    }

    out_units.clear();
    out_units.reserve(player_units.size() + enemy_units.size());
    out_units.insert(out_units.end(), player_units.begin(), player_units.end());
    out_units.insert(out_units.end(), enemy_units.begin(), enemy_units.end());

    out_error.clear();
    return true;
}

} // namespace game::battle
