#pragma once

#include "game/battle/battle_types.h"

#include <glm/vec2.hpp>

#include <optional>
#include <vector>

namespace game::scene {

/// @brief 敌方 HP 条的布局与时间轴配置。
struct BattleEnemyHpBarConfig {
    glm::vec2 size{32.0f, 5.0f};      ///< 血条主体尺寸，单位为逻辑屏幕像素。
    float border_thickness{1.0f};     ///< 外框厚度，单位为逻辑屏幕像素。
    float above_sprite_margin{0.0f};  ///< 血条与敌方精灵顶部之间的屏幕间距。
    float reveal_seconds{2.0f};       ///< 真实行动反馈触发后的显示时长。
    float fade_seconds{0.35f};        ///< 不再强制显示后的淡出时长。
    float ratio_lerp_speed{10.0f};    ///< display_ratio 每秒靠近 target_ratio 的最大比例。
    float change_delay_seconds{0.22f}; ///< 血量比例开始变化前的延迟，用于对齐命中帧。
};

/// @brief 单个敌方单位的 HP 条表现层状态。
struct BattleEnemyHpBarState {
    game::battle::BattleUnitId unit_id{0};        ///< 战斗单位 ID。
    int hp{0};                                    ///< 最近同步的 HP。
    int max_hp{1};                                ///< 最近同步的最大 HP，至少为 1。
    float target_ratio{1.0f};                     ///< 真实 HP 比例，范围 [0, 1]。
    float display_ratio{1.0f};                    ///< 绘制用 HP 比例，会平滑追随 target_ratio。
    float ratio_change_delay_seconds_remaining{0.0f}; ///< 血量变化后到命中帧前的等待时间。
    float visible_seconds_remaining{0.0f};        ///< 受击 / 回复 reveal 的剩余显示时间。
    bool highlighted{false};                      ///< 目标选择阶段是否强制显示并高亮。
    float alpha{0.0f};                            ///< 当前绘制透明度。
    bool visible{false};                          ///< alpha 或高亮使其需要绘制时为 true。
    bool initialized{false};                      ///< 是否完成过首次快照同步。
};

/// @brief 管理敌方 HP 条的显示、淡出与血量比例平滑。
class BattleEnemyHpBarController final {
public:
    BattleEnemyHpBarController() = default;
    explicit BattleEnemyHpBarController(BattleEnemyHpBarConfig config);

    /// @brief 从战斗快照同步所有敌方单位 HP；首次同步不播放血量平滑。
    /// @param snapshot 当前完整战斗快照。
    void syncFromSnapshot(const game::battle::BattleSnapshot& snapshot);

    /// @brief 根据行动结果决定是否显示目标敌方 HP 条。
    /// @param result 已结算的战斗行动结果。
    void revealFromResult(const game::battle::BattleActionResult& result);
    void stageSnapshot(const game::battle::BattleSnapshot& snapshot);
    void applyStagedSnapshotAndReveal(const game::battle::BattleActionResult& result);

    /// @brief 设置目标选择阶段高亮的敌方单位。
    /// @param unit_id 当前高亮目标；为空时取消所有高亮。
    void setHighlightedTarget(std::optional<game::battle::BattleUnitId> unit_id);

    /// @brief 推进淡出倒计时、impact 延迟与 display_ratio 平滑。
    /// @param delta_time_seconds 本帧逻辑时间。
    void update(float delta_time_seconds);

    /// @brief 清空所有 HP 条状态。
    void clear();

    /// @brief 启用 / 禁用敌方 HP 条显示。禁用时 reveal/highlight 不抬高 alpha，
    ///        `update()` 将已显示的 HP 条按 fade_seconds 节奏淡出至消失。
    /// @note 切换到 disabled 时立即清空 reveal 计时器与 highlight，
    ///       否则 update() 会因 `had_reveal_time` / `highlighted` 把 alpha 维持在 1。
    void setEnabled(bool enabled);
    [[nodiscard]] bool isEnabled() const { return enabled_; }

    /// @brief 返回所有已跟踪敌方 HP 条状态。
    [[nodiscard]] const std::vector<BattleEnemyHpBarState>& activeBars() const { return bars_; }

    /// @brief 返回指定敌方单位的 HP 条状态。
    [[nodiscard]] const BattleEnemyHpBarState* findBar(game::battle::BattleUnitId unit_id) const;

    /// @brief 返回当前配置。
    [[nodiscard]] const BattleEnemyHpBarConfig& config() const { return config_; }

private:
    [[nodiscard]] BattleEnemyHpBarState* findMutableBar(game::battle::BattleUnitId unit_id);
    void syncBarFromUnit(BattleEnemyHpBarState& bar, const game::battle::BattleUnit& unit);

    BattleEnemyHpBarConfig config_{};
    std::vector<BattleEnemyHpBarState> bars_{};
    std::optional<game::battle::BattleSnapshot> staged_snapshot_{};
    bool enabled_{true};
};

} // namespace game::scene
