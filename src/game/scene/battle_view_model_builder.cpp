#include "game/scene/battle_view_model_builder.h"

#include "game/data/item_catalog.h"
#include "game/data/rpg_catalog.h"
#include "game/data/rpg_data.h"
#include "game/defs/party_ids.h"
#include "game/factory/blueprint.h"
#include "game/factory/blueprint_manager.h"
#include "game/scene/game_scene_reward_feedback.h"
#include "game/ui/rml_item_icon_helpers.h"

#include <RmlUi/Core/Types.h>
#include <entt/core/hashed_string.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>

namespace {

using namespace entt::literals;

/// @brief 行动顺序条敌方图标解析结果；不可用时调用方回退到文本编号。
struct BattleEnemyIconDescriptor {
    Rml::String decorator{"none"};
    bool available{false};
};

[[nodiscard]] Rml::String makeRmlString(std::string_view value) {
    return Rml::String{value.data(), value.size()};
}

[[nodiscard]] Rml::String ratioPercentString(const int value, const int max_value) {
    const float ratio = max_value > 0
        ? std::clamp(static_cast<float>(value) / static_cast<float>(max_value), 0.0F, 1.0F)
        : 0.0F;
    return std::to_string(static_cast<int>(std::round(ratio * 100.0F))) + "%";
}

[[nodiscard]] Rml::String battleLogToneClass(const game::battle::BattleLogTone tone) {
    switch (tone) {
        case game::battle::BattleLogTone::Normal:
            return "normal";
        case game::battle::BattleLogTone::Damage:
            return "damage";
        case game::battle::BattleLogTone::Recovery:
            return "recovery";
        case game::battle::BattleLogTone::State:
            return "state";
        case game::battle::BattleLogTone::System:
            return "system";
        case game::battle::BattleLogTone::Error:
            return "error";
    }

    return "normal";
}

[[nodiscard]] Rml::String portraitDecoratorForUnit(const game::battle::BattleUnit& unit) {
    if (unit.source_actor_id) {
        if (*unit.source_actor_id == game::defs::kDefaultPlayerActorId) {
            return "image(portrait-player)";
        }
        if (*unit.source_actor_id == "actor.lyria") {
            return "image(portrait-lyria)";
        }
        if (*unit.source_actor_id == "actor.tori") {
            return "image(portrait-tori)";
        }
    }

    if (unit.portrait.valid()) {
        if (unit.portrait.path.ends_with("/1.png")) {
            return "image(portrait-player)";
        }
        if (unit.portrait.path.ends_with("/9.png")) {
            return "image(portrait-lyria)";
        }
        if (unit.portrait.path.ends_with("/2.png")) {
            return "image(portrait-tori)";
        }
    }

    return "none";
}

[[nodiscard]] Rml::String turnOrderFallbackLabel(const game::battle::BattleSide side, const std::size_t side_index) {
    const char prefix = side == game::battle::BattleSide::Player ? 'P' : 'E';
    return Rml::String{1, prefix} + std::to_string(side_index + 1U);
}

/// @brief 把敌方 sprite blueprint id 规范化为 RmlUi spritesheet 中的 sprite 名称。
[[nodiscard]] std::string battleEnemyIconSpriteName(std::string_view sprite_blueprint_id) {
    std::string normalized;
    bool previous_was_separator = true;

    for (const unsigned char character : sprite_blueprint_id) {
        if (std::isalnum(character) != 0) {
            normalized.push_back(static_cast<char>(std::tolower(character)));
            previous_was_separator = false;
            continue;
        }

        if (!previous_was_separator) {
            normalized.push_back('-');
            previous_was_separator = true;
        }
    }

    while (!normalized.empty() && normalized.back() == '-') {
        normalized.pop_back();
    }

    return normalized.empty() ? std::string{} : "battle-enemy-icon-" + normalized;
}

/// @brief 查找行动顺序条头像使用的朝下 idle 动画；简单蓝图可回退到 bare idle。
[[nodiscard]] const game::factory::AnimationBlueprint*
findEnemyIdleDownAnimation(const game::factory::ActorBlueprint& blueprint) {
    if (const auto down_it = blueprint.animations_.find("idle_down"_hs);
        down_it != blueprint.animations_.end()) {
        return &down_it->second;
    }

    if (const auto idle_it = blueprint.animations_.find("idle"_hs);
        idle_it != blueprint.animations_.end()) {
        return &idle_it->second;
    }

    return nullptr;
}

/// @brief 为敌方行动顺序条解析 RmlUi image decorator，失败时返回不可用结果。
[[nodiscard]] BattleEnemyIconDescriptor enemyTurnOrderIconDecorator(
    const game::battle::BattleUnit& unit,
    const game::data::RpgCatalog* rpg_catalog,
    const game::factory::BlueprintManager* blueprint_manager) {
    if (!unit.source_enemy_id.has_value() ||
        !rpg_catalog ||
        !blueprint_manager) {
        return {};
    }

    const auto* enemy = rpg_catalog->findEnemy(*unit.source_enemy_id);
    if (!enemy || !enemy->battle_visual_.valid()) {
        return {};
    }

    const entt::id_type blueprint_hash = enemy->battle_visual_.sprite_blueprint_id_hash_;
    if (!blueprint_manager->hasActorBlueprint(blueprint_hash)) {
        return {};
    }

    const auto& blueprint = blueprint_manager->getActorBlueprint(blueprint_hash);
    const auto* idle_down_animation = findEnemyIdleDownAnimation(blueprint);
    if (!idle_down_animation || idle_down_animation->frames_.empty()) {
        return {};
    }

    const std::string sprite_name = battleEnemyIconSpriteName(enemy->battle_visual_.sprite_blueprint_id_);
    if (sprite_name.empty()) {
        return {};
    }

    return BattleEnemyIconDescriptor{
        .decorator = makeRmlString("image(" + sprite_name + ")"),
        .available = true
    };
}

[[nodiscard]] std::string battleStateIconKeyFromId(std::string_view state_id) {
    constexpr std::string_view STATE_PREFIX = "state.";
    if (state_id.rfind(STATE_PREFIX, 0) == 0) {
        state_id.remove_prefix(STATE_PREFIX.size());
    }

    std::string normalized;
    bool previous_was_separator = true;
    for (const unsigned char character : state_id) {
        if (std::isalnum(character) != 0) {
            normalized.push_back(static_cast<char>(std::tolower(character)));
            previous_was_separator = false;
            continue;
        }
        if (!previous_was_separator) {
            normalized.push_back('-');
            previous_was_separator = true;
        }
    }
    while (!normalized.empty() && normalized.back() == '-') {
        normalized.pop_back();
    }
    return normalized;
}

[[nodiscard]] Rml::String battleStateIconDecorator(const game::data::StateData* state) {
    if (!state) {
        return "none";
    }

    std::string icon_key = state->icon_key_.empty()
        ? battleStateIconKeyFromId(state->id_)
        : state->icon_key_;
    if (icon_key.empty()) {
        return "none";
    }

    return makeRmlString("image(battle-state-icon-" + icon_key + ")");
}

[[nodiscard]] Rml::String battleStateShortLabel(std::string_view state_id) {
    constexpr std::string_view STATE_PREFIX = "state.";
    if (state_id.rfind(STATE_PREFIX, 0) == 0) {
        state_id.remove_prefix(STATE_PREFIX.size());
    }

    for (const unsigned char character : state_id) {
        if (std::isalpha(character) != 0) {
            return Rml::String{1, static_cast<char>(std::toupper(character))};
        }
    }

    return "?";
}

} // namespace

namespace game::scene {

BattleViewModelBuilder::BattleViewModelBuilder(const game::data::RpgCatalog* rpg_catalog,
                                               const game::data::ItemCatalog* item_catalog,
                                               const game::factory::BlueprintManager* blueprint_manager)
    : rpg_catalog_(rpg_catalog),
      item_catalog_(item_catalog),
      blueprint_manager_(blueprint_manager) {
}

std::vector<BattleTurnOrderEntryViewModel> BattleViewModelBuilder::buildTurnOrderEntries(
    const game::battle::BattleSession& session) const {
    const auto& turn_order = session.turnOrder();
    const auto current_actor_id = session.currentActorId();
    const auto current_order_it = current_actor_id
        ? std::find(turn_order.begin(), turn_order.end(), *current_actor_id)
        : turn_order.end();
    const bool has_current_order_index = current_order_it != turn_order.end();
    const std::size_t current_order_index = has_current_order_index
        ? static_cast<std::size_t>(std::distance(turn_order.begin(), current_order_it))
        : 0U;

    std::size_t player_index = 0U;
    std::size_t enemy_index = 0U;
    std::vector<BattleTurnOrderEntryViewModel> entries;
    entries.reserve(turn_order.size());

    for (std::size_t order_index = 0U; order_index < turn_order.size(); ++order_index) {
        const game::battle::BattleUnitId unit_id = turn_order[order_index];
        const auto* unit_ptr = session.findUnit(unit_id);
        if (!unit_ptr) {
            continue;
        }

        const auto& unit = *unit_ptr;
        const bool enemy = unit.side == game::battle::BattleSide::Enemy;
        const std::size_t side_index = enemy ? enemy_index++ : player_index++;
        Rml::String portrait_decorator = portraitDecoratorForUnit(unit);
        Rml::String badge_label{};
        if (enemy) {
            const BattleEnemyIconDescriptor icon =
                enemyTurnOrderIconDecorator(unit, rpg_catalog_, blueprint_manager_);
            if (icon.available) {
                portrait_decorator = icon.decorator;
                badge_label = std::to_string(side_index + 1U);
            }
        }
        const bool ko = !unit.isAlive();
        const bool current = !ko && current_actor_id.has_value() && *current_actor_id == unit.id;
        const bool acted = !ko && !current && has_current_order_index && order_index < current_order_index;
        const Rml::String fallback_label = portrait_decorator == "none"
            ? turnOrderFallbackLabel(unit.side, side_index)
            : Rml::String{};

        entries.push_back(BattleTurnOrderEntryViewModel{
            .unit_id = static_cast<int>(unit.id),
            .entry_index = static_cast<int>(entries.size()),
            .name = makeRmlString(unit.name),
            .short_label = fallback_label,
            .badge_label = badge_label,
            .portrait_decorator = portrait_decorator,
            .current = current,
            .acted = acted,
            .ko = ko,
            .enemy = enemy
        });
    }

    return entries;
}

BattlePartyHudViewModels BattleViewModelBuilder::buildPartyHud(const game::battle::BattleSession& session) const {
    const auto current_actor_id = session.currentActorId();
    const auto& active_unit_states = session.activeUnitStates();
    BattlePartyHudViewModels view_models{};

    for (const auto& unit : session.units()) {
        if (unit.side != game::battle::BattleSide::Player) {
            continue;
        }

        view_models.party_status.push_back(BattlePartyStatusViewModel{
            .unit_id = static_cast<int>(unit.id),
            .name = makeRmlString(unit.name),
            .hp_text = makeRmlString(std::to_string(std::max(0, unit.hp)) + "/" + std::to_string(std::max(0, unit.max_hp))),
            .mp_text = makeRmlString(std::to_string(std::max(0, unit.mp)) + "/" + std::to_string(std::max(0, unit.max_mp))),
            .hp_ratio_percent = ratioPercentString(unit.hp, unit.max_hp),
            .mp_ratio_percent = ratioPercentString(unit.mp, unit.max_mp),
            .portrait_decorator = portraitDecoratorForUnit(unit),
            .active = current_actor_id.has_value() && *current_actor_id == unit.id,
            .ko = !unit.isAlive()
        });

        if (!unit.isAlive()) {
            continue;
        }

        const auto states_it = std::find_if(
            active_unit_states.begin(),
            active_unit_states.end(),
            [&unit](const game::battle::BattleUnitStateSnapshot& state_snapshot) {
                return state_snapshot.unit_id == unit.id;
            });
        if (states_it == active_unit_states.end()) {
            continue;
        }

        int state_entry_index = 0;
        for (const auto& state_snapshot : states_it->states) {
            const auto* state = rpg_catalog_ ? rpg_catalog_->findState(state_snapshot.state_id) : nullptr;
            const Rml::String display_name = state ? makeRmlString(state->display_name_) : makeRmlString(state_snapshot.state_id);
            const Rml::String description = state && !state->description_.empty()
                ? makeRmlString(state->description_)
                : Rml::String{"No description"};
            const Rml::String icon_decorator = battleStateIconDecorator(state);
            view_models.party_state_icons.push_back(BattleStateIconViewModel{
                .unit_id = static_cast<int>(unit.id),
                .entry_index = state_entry_index,
                .state_id = makeRmlString(state_snapshot.state_id),
                .display_name = display_name,
                .description = description,
                .turns_text = makeRmlString(std::to_string(state_snapshot.turns_left)),
                .short_label = icon_decorator == "none" ? battleStateShortLabel(state_snapshot.state_id) : Rml::String{},
                .icon_decorator = icon_decorator,
                .known = state != nullptr
            });
            ++state_entry_index;
        }
    }

    return view_models;
}

std::vector<BattleLogEntryViewModel> BattleViewModelBuilder::buildBattleLogEntries(
    const std::vector<game::battle::BattleLogLine>& log_history,
    const std::size_t visible_limit) const {
    std::vector<BattleLogEntryViewModel> entries;
    const std::size_t visible_count = std::min(visible_limit, log_history.size());
    entries.reserve(visible_count);

    const auto begin_it = log_history.end() - static_cast<std::ptrdiff_t>(visible_count);
    for (auto it = begin_it; it != log_history.end(); ++it) {
        entries.push_back(BattleLogEntryViewModel{
            .text = makeRmlString(it->text),
            .tone_class = battleLogToneClass(it->tone)
        });
    }

    return entries;
}

std::vector<BattleVictoryRewardItemViewModel> BattleViewModelBuilder::buildVictoryRewardItems(
    const BattleVictoryFlowSnapshot& snapshot) const {
    std::vector<BattleVictoryRewardItemViewModel> items;
    items.reserve(snapshot.item_drops.size());
    int entry_index = 0;
    for (const auto& drop : snapshot.item_drops) {
        if (drop.count <= 0) {
            continue;
        }

        Rml::String label = makeRmlString(drop.item_id);
        if (item_catalog_) {
            if (const auto* item = item_catalog_->findItem(drop.item_id_hash);
                item && !item->display_name_.empty()) {
                label = makeRmlString(item->display_name_);
            }
        }

        items.push_back(BattleVictoryRewardItemViewModel{
            .entry_index = entry_index++,
            .label = label,
            .count_text = makeRmlString("x" + std::to_string(drop.count)),
            .icon_decorator = makeRmlString(game::ui::buildItemIconDecorator(item_catalog_, drop.item_id_hash))
        });
    }

    return items;
}

std::vector<BattleVictoryLevelUpViewModel> BattleViewModelBuilder::buildVictoryLevelUps(
    const BattleVictoryFlowSnapshot& snapshot) const {
    std::vector<BattleVictoryLevelUpViewModel> level_ups;
    level_ups.reserve(snapshot.level_ups.size());
    int level_entry_index = 0;
    for (const auto& grant : snapshot.level_ups) {
        if (!grant.leveledUp()) {
            continue;
        }
        level_ups.push_back(BattleVictoryLevelUpViewModel{
            .entry_index = level_entry_index++,
            .label = makeRmlString(grant.display_name + " Lv." + std::to_string(grant.new_level)),
            .stat_text = makeRmlString(game::scene::formatLevelUpStatText(grant)),
        });
    }

    return level_ups;
}

} // namespace game::scene
