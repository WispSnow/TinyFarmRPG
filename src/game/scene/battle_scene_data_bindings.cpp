#include "battle_scene_data_bindings.h"

#include "game/scene/battle_scene_view_models.h"

#include <RmlUi/Core/DataModelHandle.h>

namespace game::scene {

bool registerBattleSceneViewModelStructs(Rml::DataModelConstructor& constructor) {
    if (auto command_handle = constructor.RegisterStruct<BattleCommandViewModel>()) {
        command_handle.RegisterMember("command_id", &BattleCommandViewModel::command_id);
        command_handle.RegisterMember("entry_index", &BattleCommandViewModel::entry_index);
        command_handle.RegisterMember("label", &BattleCommandViewModel::label);
        command_handle.RegisterMember("enabled", &BattleCommandViewModel::enabled);
    } else {
        return false;
    }

    if (auto entry_handle = constructor.RegisterStruct<BattleListEntryViewModel>()) {
        entry_handle.RegisterMember("entry_index", &BattleListEntryViewModel::entry_index);
        entry_handle.RegisterMember("entry_id", &BattleListEntryViewModel::entry_id);
        entry_handle.RegisterMember("label", &BattleListEntryViewModel::label);
        entry_handle.RegisterMember("sublabel", &BattleListEntryViewModel::sublabel);
        entry_handle.RegisterMember("enabled", &BattleListEntryViewModel::enabled);
    } else {
        return false;
    }

    if (auto target_handle = constructor.RegisterStruct<BattleTargetEntryViewModel>()) {
        target_handle.RegisterMember("entry_index", &BattleTargetEntryViewModel::entry_index);
        target_handle.RegisterMember("unit_id", &BattleTargetEntryViewModel::unit_id);
        target_handle.RegisterMember("label", &BattleTargetEntryViewModel::label);
        target_handle.RegisterMember("sublabel", &BattleTargetEntryViewModel::sublabel);
        target_handle.RegisterMember("enabled", &BattleTargetEntryViewModel::enabled);
        target_handle.RegisterMember("is_ally", &BattleTargetEntryViewModel::is_ally);
        target_handle.RegisterMember("is_dead", &BattleTargetEntryViewModel::is_dead);
    } else {
        return false;
    }

    if (auto party_handle = constructor.RegisterStruct<BattlePartyStatusViewModel>()) {
        party_handle.RegisterMember("unit_id", &BattlePartyStatusViewModel::unit_id);
        party_handle.RegisterMember("name", &BattlePartyStatusViewModel::name);
        party_handle.RegisterMember("hp_text", &BattlePartyStatusViewModel::hp_text);
        party_handle.RegisterMember("mp_text", &BattlePartyStatusViewModel::mp_text);
        party_handle.RegisterMember("hp_ratio_percent", &BattlePartyStatusViewModel::hp_ratio_percent);
        party_handle.RegisterMember("mp_ratio_percent", &BattlePartyStatusViewModel::mp_ratio_percent);
        party_handle.RegisterMember("portrait_decorator", &BattlePartyStatusViewModel::portrait_decorator);
        party_handle.RegisterMember("active", &BattlePartyStatusViewModel::active);
        party_handle.RegisterMember("ko", &BattlePartyStatusViewModel::ko);
    } else {
        return false;
    }

    if (auto state_icon_handle = constructor.RegisterStruct<BattleStateIconViewModel>()) {
        state_icon_handle.RegisterMember("unit_id", &BattleStateIconViewModel::unit_id);
        state_icon_handle.RegisterMember("entry_index", &BattleStateIconViewModel::entry_index);
        state_icon_handle.RegisterMember("state_id", &BattleStateIconViewModel::state_id);
        state_icon_handle.RegisterMember("display_name", &BattleStateIconViewModel::display_name);
        state_icon_handle.RegisterMember("description", &BattleStateIconViewModel::description);
        state_icon_handle.RegisterMember("turns_text", &BattleStateIconViewModel::turns_text);
        state_icon_handle.RegisterMember("short_label", &BattleStateIconViewModel::short_label);
        state_icon_handle.RegisterMember("icon_decorator", &BattleStateIconViewModel::icon_decorator);
        state_icon_handle.RegisterMember("known", &BattleStateIconViewModel::known);
    } else {
        return false;
    }

    if (auto state_tooltip_handle = constructor.RegisterStruct<BattleStateTooltipViewModel>()) {
        state_tooltip_handle.RegisterMember("active_unit_id", &BattleStateTooltipViewModel::active_unit_id);
        state_tooltip_handle.RegisterMember("title", &BattleStateTooltipViewModel::title);
        state_tooltip_handle.RegisterMember("turns", &BattleStateTooltipViewModel::turns);
        state_tooltip_handle.RegisterMember("description", &BattleStateTooltipViewModel::description);
        state_tooltip_handle.RegisterMember("visible", &BattleStateTooltipViewModel::visible);
    } else {
        return false;
    }

    if (auto battle_log_handle = constructor.RegisterStruct<BattleLogEntryViewModel>()) {
        battle_log_handle.RegisterMember("text", &BattleLogEntryViewModel::text);
        battle_log_handle.RegisterMember("tone_class", &BattleLogEntryViewModel::tone_class);
    } else {
        return false;
    }

    if (auto victory_item_handle = constructor.RegisterStruct<BattleVictoryRewardItemViewModel>()) {
        victory_item_handle.RegisterMember("entry_index", &BattleVictoryRewardItemViewModel::entry_index);
        victory_item_handle.RegisterMember("label", &BattleVictoryRewardItemViewModel::label);
        victory_item_handle.RegisterMember("count_text", &BattleVictoryRewardItemViewModel::count_text);
        victory_item_handle.RegisterMember("icon_decorator", &BattleVictoryRewardItemViewModel::icon_decorator);
    } else {
        return false;
    }

    if (auto victory_level_handle = constructor.RegisterStruct<BattleVictoryLevelUpViewModel>()) {
        victory_level_handle.RegisterMember("entry_index", &BattleVictoryLevelUpViewModel::entry_index);
        victory_level_handle.RegisterMember("label", &BattleVictoryLevelUpViewModel::label);
        victory_level_handle.RegisterMember("stat_text", &BattleVictoryLevelUpViewModel::stat_text);
    } else {
        return false;
    }

    if (auto turn_order_handle = constructor.RegisterStruct<BattleTurnOrderEntryViewModel>()) {
        turn_order_handle.RegisterMember("unit_id", &BattleTurnOrderEntryViewModel::unit_id);
        turn_order_handle.RegisterMember("entry_index", &BattleTurnOrderEntryViewModel::entry_index);
        turn_order_handle.RegisterMember("name", &BattleTurnOrderEntryViewModel::name);
        turn_order_handle.RegisterMember("short_label", &BattleTurnOrderEntryViewModel::short_label);
        turn_order_handle.RegisterMember("badge_label", &BattleTurnOrderEntryViewModel::badge_label);
        turn_order_handle.RegisterMember("portrait_decorator", &BattleTurnOrderEntryViewModel::portrait_decorator);
        turn_order_handle.RegisterMember("current", &BattleTurnOrderEntryViewModel::current);
        turn_order_handle.RegisterMember("acted", &BattleTurnOrderEntryViewModel::acted);
        turn_order_handle.RegisterMember("ko", &BattleTurnOrderEntryViewModel::ko);
        turn_order_handle.RegisterMember("enemy", &BattleTurnOrderEntryViewModel::enemy);
    } else {
        return false;
    }

    return true;
}

} // namespace game::scene
