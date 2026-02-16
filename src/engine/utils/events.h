#pragma once
#include <memory>
#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

namespace engine::scene {
    class Scene;
}

namespace engine::utils {

// --- 控制流事件（推荐 trigger<T>() 立即分发） ---
// 这些事件往往会改变主循环控制流/场景栈结构，通常希望尽快触达监听者。
// 注意：即使使用 trigger，同步监听者也可以选择“延迟落地”（例如 SceneManager 记录 pending action，在 update 末尾统一改栈）。

/// @brief 退出事件（推荐 trigger<QuitEvent>()）
struct QuitEvent {};

/// @brief 弹出场景事件（推荐 trigger<PopSceneEvent>()）
struct PopSceneEvent {};

/// @brief 压入场景事件（推荐 trigger<PushSceneEvent>()）
struct PushSceneEvent {
    std::unique_ptr<engine::scene::Scene> scene; 
};

/// @brief 替换场景事件（推荐 trigger<ReplaceSceneEvent>()）
struct ReplaceSceneEvent {
    std::unique_ptr<engine::scene::Scene> scene; 
};

// --- 资源/渲染相关事件（通常推荐 enqueue + dispatcher.update()） ---
struct FontUnloadedEvent {
    entt::id_type font_id{entt::null};
    int font_size{0};
};

struct FontsClearedEvent {};

// --- 窗口/渲染相关事件（推荐 trigger<T>() 立即分发） ---
struct WindowResizedEvent {
    int width{0};
    int height{0};
    bool pixel_size{false}; // true 表示像素大小(高DPI)，false 表示窗口坐标大小
};

// --- 动画/音频等反馈事件（通常推荐 enqueue + dispatcher.update()） ---

/// @brief 播放动画事件（通常 enqueue，避免在更新链路中递归触发）
struct PlayAnimationEvent {
    entt::entity entity_{entt::null};           ///< @brief 目标实体
    entt::id_type animation_id_{entt::null};    ///< @brief 动画ID
    bool loop_{true};                           ///< @brief 是否循环
};

/// @brief 动画播放完成事件（通常 enqueue，在帧尾结算）
struct AnimationFinishedEvent {
    entt::entity entity_{entt::null};           ///< @brief 目标实体
    entt::id_type animation_id_{entt::null};    ///< @brief 动画ID
};

/// @brief 动画事件（通常 enqueue，在帧尾结算）
struct AnimationEvent {
    entt::entity entity_{entt::null};           ///< @brief 目标实体
    entt::id_type event_name_id_{entt::null};   ///< @brief 事件名称ID
    entt::id_type animation_name_id_{entt::null};   ///< @brief 动画名称ID
};

/// @brief 播放音效事件（通常 enqueue，在帧尾结算）
struct PlaySoundEvent {
    entt::entity entity_{entt::null};           ///< @brief 目标实体（可以为空，即播放全局音效）
    entt::id_type sound_id_{entt::null};        ///< @brief 音效ID
};

/// @brief 添加自动图块事件（通常 enqueue）
struct AddAutoTileEntityEvent {
    entt::id_type rule_id_{entt::null};        ///< @brief 自动图块规则ID
    glm::vec2 world_pos_{0.0f, 0.0f};          ///< @brief 世界位置
};

/// @brief 移除自动图块事件（通常 enqueue）
struct RemoveAutoTileEntityEvent {
    entt::id_type rule_id_{entt::null};        ///< @brief 自动图块规则ID
    glm::vec2 world_pos_{0.0f, 0.0f};          ///< @brief 世界位置
};

// --- 时间系统相关事件 ---
// 典型数据事件：可能在一帧内批量产生，适合 enqueue 并在帧尾统一分发。

/// @brief 新的一天开始事件（通常 enqueue）
struct DayChangedEvent {
    std::uint32_t day_{1};                     ///< @brief 新的天数
};

/// @brief 小时变化事件（通常 enqueue）
struct HourChangedEvent {
    std::uint32_t day_{1};                     ///< @brief 当前天数
    float hour_{0.0f};                         ///< @brief 新的小时
};

/// @brief 时段变化事件（通常 enqueue）
struct TimeOfDayChangedEvent {
    std::uint32_t day_{1};                     ///< @brief 当前天数
    float hour_{0.0f};                         ///< @brief 当前小时
    std::uint8_t time_of_day_{0};              ///< @brief 新的时段（TimeOfDay 枚举值）
};

} // namespace engine::utils
