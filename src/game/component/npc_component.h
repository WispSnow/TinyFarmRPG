#pragma once

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <cstddef>

namespace game::component {

/// 通用 NPC 标签 —— 标记可对话/可漫游的 NPC 实体
struct NPCTag {};
/// 动物 NPC 标签 —— 标记动物实体（具有进食/睡眠行为）
struct AnimalTag {};

struct WanderComponent {
    glm::vec2 home_position_{0.0f};   ///< @brief 漫游中心（出生点）
    float radius_{0.0f};              ///< @brief 漫游半径
    glm::vec2 target_{0.0f};          ///< @brief 当前目标点
    bool has_target_{false};          ///< @brief 是否已有目标点
    float wait_timer_{0.0f};          ///< @brief 等待计时器（<=0 进入移动状态）
    bool moving_{false};              ///< @brief 当前是否处于移动阶段
    static constexpr float DEFAULT_MIN_WAIT = 0.6f;
    static constexpr float DEFAULT_MAX_WAIT = 1.8f;
    static constexpr float DEFAULT_STUCK_RESET = 1.0f;
    float min_wait_{DEFAULT_MIN_WAIT};    ///< @brief 最小等待时间
    float max_wait_{DEFAULT_MAX_WAIT};    ///< @brief 最大等待时间
    float last_distance_sq_{0.0f};    ///< @brief 上一帧与目标的距离平方
    float stuck_timer_{0.0f};         ///< @brief 移动时未接近目标的累计时间
    float stuck_reset_{DEFAULT_STUCK_RESET}; ///< @brief 判定卡住的时间阈值
};

struct DialogueComponent {
    entt::id_type dialogue_id_{entt::null};  ///< @brief 对话ID（为空则禁用对话）
    static constexpr float DEFAULT_INTERACT_DISTANCE = 96.0f;
    static constexpr float DEFAULT_COOLDOWN = 0.35f;
    float interact_distance_{DEFAULT_INTERACT_DISTANCE}; ///< @brief 触发对话的距离
    bool active_{false};                     ///< @brief 当前是否处于对话状态
    std::size_t current_line_{0};            ///< @brief 当前对话行
    float cooldown_{DEFAULT_COOLDOWN};        ///< @brief 交互冷却（秒）
    float cooldown_timer_{0.0f};             ///< @brief 当前冷却计时
};

struct SleepRoutine {
    bool sleep_at_night_{true};  ///< @brief 夜间是否睡觉
    bool is_sleeping_{false};    ///< @brief 当前是否处于睡眠
};

struct AnimalBehaviorState {
    bool eating_{false};                ///< @brief 是否正在进食
    float eat_cooldown_timer_{0.0f};    ///< @brief 距离下一次进食的时间
    float eat_duration_timer_{0.0f};    ///< @brief 当前进食剩余时间
    static constexpr float DEFAULT_EAT_INTERVAL_MIN = 4.0f;
    static constexpr float DEFAULT_EAT_INTERVAL_MAX = 8.0f;
    static constexpr float DEFAULT_EAT_DURATION = 1.6f;
    float eat_interval_min_{DEFAULT_EAT_INTERVAL_MIN};  ///< @brief 两次进食最小间隔
    float eat_interval_max_{DEFAULT_EAT_INTERVAL_MAX};  ///< @brief 两次进食最大间隔
    float eat_duration_{DEFAULT_EAT_DURATION};           ///< @brief 每次进食持续时间
};

} // namespace game::component
