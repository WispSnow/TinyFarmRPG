#pragma once

#include "engine/utils/math.h"
#include "game/battle/battle_types.h"
#include "game/scene/battle_presentation_unit_anchor.h"

#include <glm/vec2.hpp>

#include <string>
#include <vector>

namespace game::scene {

/// @brief 伤害飘字的语义类型，用于选择默认颜色和动画风格。
enum class BattleDamagePopupKind {
    HpDamage,  ///< HP 伤害数字。
    HpRecover, ///< HP 恢复数字。
    MpRecover, ///< MP 恢复数字。
    Miss,      ///< 未命中文字。
    Critical   ///< 暴击提示文字。
};

/// @brief DamagePopup 的屏幕空间布局参数。
struct BattleDamagePopupLayoutConfig {
    glm::vec2 target_anchor_offset{0.0f, -52.0f}; ///< 目标锚点上方的屏幕偏移。
    glm::vec2 actor_anchor_offset{0.0f, -48.0f};  ///< 无明确目标时施放者锚点上方的屏幕偏移。
    float lane_spacing_x{18.0f};                  ///< 同一延迟下多个数值 popup 的横向错位距离。
};

/// @brief DamagePopup 的时间轴参数。
struct BattleDamagePopupTimingConfig {
    float impact_delay_seconds{0.22f};            ///< 与 1.2 受击 impact frame 对齐的默认延迟。
    float critical_number_stagger_seconds{0.12f}; ///< Critical 弹出后数值 popup 的追加延迟。
    float hp_damage_duration_seconds{0.85f};      ///< HP 伤害 popup 的播放时长。
    float recover_duration_seconds{0.75f};        ///< HP / MP 恢复 popup 的播放时长。
    float miss_duration_seconds{0.70f};           ///< Miss popup 的播放时长。
    float critical_duration_seconds{0.58f};       ///< Critical popup 的播放时长。
    float max_delta_seconds{0.25f};               ///< 单帧推进上限，避免卡顿帧跳过完整动画。
};

/// @brief 单个正在播放或等待播放的 DamagePopup。
struct BattleDamagePopup {
    std::string text{};                                        ///< 显示文本，例如 "24"、"+18"、"Miss"。
    BattleDamagePopupKind kind{BattleDamagePopupKind::HpDamage}; ///< popup 类型。
    glm::vec2 base_screen_position{0.0f, 0.0f};                ///< 生成时的屏幕空间基础位置。
    glm::vec2 offset{0.0f, 0.0f};                              ///< 当前动画偏移。
    float delay_seconds{0.0f};                                 ///< 播放前等待时间。
    float elapsed_seconds{0.0f};                               ///< 自生成以来累计时间。
    float duration_seconds{0.0f};                              ///< 延迟结束后的播放时长。
    float alpha{0.0f};                                         ///< 当前透明度。
    float scale{1.0f};                                         ///< 当前字形缩放。
    int lane_index{0};                                         ///< 同延迟多 popup 的横向错位序号。
    bool visible{false};                                       ///< 延迟结束且 alpha 可见时为 true。

    /// @brief 返回当前屏幕空间绘制锚点。
    [[nodiscard]] glm::vec2 screenPosition() const {
        return base_screen_position + offset;
    }
};

/// @brief 将 BattleActionResult 转换为短生命周期 DamagePopup，并推进其动画状态。
class BattleDamagePopupController final {
public:
    BattleDamagePopupController() = default;
    BattleDamagePopupController(BattleDamagePopupLayoutConfig layout, BattleDamagePopupTimingConfig timing);

    /// @brief 根据行动结果生成飘字。
    /// @param result 已结算的战斗行动结果。
    /// @param unit_anchors 当前战斗表现层单位锚点；无 target 时会降级使用 actor 锚点显示聚合数值。
    void spawnFromResult(const game::battle::BattleActionResult& result,
                         const std::vector<BattlePresentationUnitAnchor>& unit_anchors);
    void spawnFromResult(const game::battle::BattleActionResult& result,
                         const std::vector<BattlePresentationUnitAnchor>& unit_anchors,
                         float impact_time_seconds);

    /// @brief 推进所有 popup 的延迟、位移、缩放和透明度，并清理过期项。
    /// @param delta_time_seconds 本帧逻辑时间。
    void update(float delta_time_seconds);

    /// @brief 清空所有未完成 popup。
    void clear();

    /// @brief 获取当前仍在等待或播放的 popup 列表。
    [[nodiscard]] const std::vector<BattleDamagePopup>& activePopups() const { return popups_; }

private:
    [[nodiscard]] const BattlePresentationUnitAnchor* findAnchor(
        const std::vector<BattlePresentationUnitAnchor>& unit_anchors,
        game::battle::BattleUnitId unit_id) const;
    void spawnPopup(std::string text,
                    BattleDamagePopupKind kind,
                    glm::vec2 anchor_position,
                    float delay_seconds,
                    int lane_index);

    BattleDamagePopupLayoutConfig layout_{};
    BattleDamagePopupTimingConfig timing_{};
    std::vector<BattleDamagePopup> popups_{};
};

/// @brief 返回指定 popup 类型在当前 alpha 下的颜色。
[[nodiscard]] engine::utils::FColor battleDamagePopupColor(BattleDamagePopupKind kind, float alpha);

} // namespace game::scene
