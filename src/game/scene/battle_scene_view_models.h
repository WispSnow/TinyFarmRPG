#pragma once

#include <RmlUi/Core/Types.h>

namespace game::scene {

/// @brief 战斗命令单项的表现层视图模型。
struct BattleCommandViewModel {
    int command_id{0};
    int entry_index{0};
    Rml::String label{};
    bool enabled{false};
    bool selected{false};

    friend bool operator==(const BattleCommandViewModel& lhs, const BattleCommandViewModel& rhs) = default;
};

/// @brief 技能列表或道具列表共用的条目视图模型。
struct BattleListEntryViewModel {
    int entry_index{0};
    Rml::String entry_id{};
    Rml::String label{};
    Rml::String sublabel{};
    bool enabled{false};
    bool selected{false};
};

/// @brief 目标选择菜单中单个战斗单位的视图模型。
struct BattleTargetEntryViewModel {
    int entry_index{0};
    int unit_id{0};
    Rml::String label{};
    Rml::String sublabel{};
    bool enabled{false};
    bool is_ally{false};
    bool is_dead{false};
    bool selected{false};
};

/// @brief 下方 HUD 中单个队友状态条目的视图模型。
struct BattlePartyStatusViewModel {
    int unit_id{0};
    Rml::String name{};
    Rml::String hp_text{};
    Rml::String mp_text{};
    Rml::String hp_ratio_percent{"0%"};
    Rml::String mp_ratio_percent{"0%"};
    Rml::String portrait_decorator{"none"};
    bool active{false};
    bool ko{false};
};

/// @brief 下方 HUD 中单个状态图标的扁平视图模型。
struct BattleStateIconViewModel {
    int unit_id{0};
    int entry_index{0};
    Rml::String state_id{};
    Rml::String display_name{};
    Rml::String description{};
    Rml::String turns_text{};
    Rml::String short_label{};
    Rml::String icon_decorator{"none"};
    bool known{false};

    friend bool operator==(const BattleStateIconViewModel& lhs, const BattleStateIconViewModel& rhs) = default;
};

/// @brief 状态图标 hover tooltip 的轻量视图模型。
struct BattleStateTooltipViewModel {
    int active_unit_id{0};
    Rml::String title{};
    Rml::String turns{};
    Rml::String description{};
    bool visible{false};

    friend bool operator==(const BattleStateTooltipViewModel& lhs, const BattleStateTooltipViewModel& rhs) = default;
};

/// @brief 滚动战斗日志中单行的 RmlUi 表现层视图模型。
struct BattleLogEntryViewModel {
    Rml::String text{};
    Rml::String tone_class{};

    friend bool operator==(const BattleLogEntryViewModel& lhs, const BattleLogEntryViewModel& rhs) = default;
};

/// @brief Victory overlay 中单个掉落条目的视图模型。
struct BattleVictoryRewardItemViewModel {
    int entry_index{0};
    Rml::String label{};
    Rml::String count_text{};
    Rml::String icon_decorator{"none"};

    friend bool operator==(const BattleVictoryRewardItemViewModel& lhs,
                           const BattleVictoryRewardItemViewModel& rhs) = default;
};

/// @brief Victory overlay 中单个升级条目的视图模型。
struct BattleVictoryLevelUpViewModel {
    int entry_index{0};
    Rml::String label{};
    Rml::String stat_text{};

    friend bool operator==(const BattleVictoryLevelUpViewModel& lhs,
                           const BattleVictoryLevelUpViewModel& rhs) = default;
};

/// @brief 顶部行动顺序条中单个单位的只读表现层条目。
struct BattleTurnOrderEntryViewModel {
    int unit_id{0};
    int entry_index{0};
    Rml::String name{};
    Rml::String short_label{};
    Rml::String badge_label{};
    Rml::String portrait_decorator{"none"};
    bool current{false};
    bool acted{false};
    bool ko{false};
    bool enemy{false};

    friend bool operator==(const BattleTurnOrderEntryViewModel& lhs,
                           const BattleTurnOrderEntryViewModel& rhs) = default;
};

} // namespace game::scene
