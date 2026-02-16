#pragma once

namespace game::component {

// ── 角色/实体标记 ──
/// 死亡标签 —— 延时删除实体
struct DeadTag {};
/// 玩家标签 —— 标记玩家实体
struct PlayerTag {};

// ── 状态驱动 ──
/// 状态变化标签 —— StateSystem 重新计算动画
struct StateDirtyTag {};
/// 动作锁定标签 —— 让角色播放完当前动画再进行下一步动作（硬直）
struct ActionLockedTag {};

// ── 昼夜条件 ──
/// 夜晚生效标签（用于标记仅夜晚发光的实体）
struct NightOnlyTag {};
/// 白天生效标签（用于标记仅白天发光的实体）
struct DayOnlyTag {};

// ── 耕作相关 ──

} // namespace game::component
