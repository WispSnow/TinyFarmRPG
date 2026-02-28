#pragma once
#include <string_view>
#include <string>
#include <map>
#include <unordered_map>
#include <vector>
#include <array>
#include <functional>
#include <memory>
#include <cstdint>
#include <SDL3/SDL.h>
#include <glm/vec2.hpp>
#include <entt/signal/sigh.hpp>
#include <entt/signal/fwd.hpp>

namespace engine::core {
    class GameState;
}

namespace engine::input {

/**
 * @brief 动作状态枚举, 除了表示状态外，还将用于函数数组索引(0~2)
 */
enum class ActionState {
    PRESSED,    // 动作在本帧刚刚被按下
    HELD,       // 动作被持续按下
    RELEASED,   // 动作在本帧刚刚被释放
    INACTIVE    // 动作未激活 (放在最后，不占用数组索引)
};

/// @brief 动作数据聚合：状态 + 回调信号 + 引用计数 + 调试名称
struct ActionEntry {
    static constexpr std::size_t CALLBACK_STATE_COUNT = static_cast<std::size_t>(ActionState::INACTIVE);

    ActionState state = ActionState::INACTIVE;
    uint8_t     active_count = 0;              ///< 当前有几个物理输入按住此动作
    std::string name;                           ///< 调试用动作名称
    std::array<entt::sigh<bool()>, CALLBACK_STATE_COUNT> signals;
};

/**
 * @brief 输入管理器类，负责处理输入事件和动作状态。
 *
 * 该类管理输入事件，将按键转换为动作状态，并提供查询动作状态的功能。
 * 它还处理鼠标位置的逻辑坐标转换。
 */
class InputManager final {
private:
    entt::dispatcher* dispatcher_;                                          ///< @brief 事件分发器
    engine::core::GameState* game_state_{nullptr};                           ///< @brief 游戏状态指针，用于查询窗口/逻辑尺寸

    /// @brief 动作 ID → 动作数据（状态 + 回调 + 引用计数 + 调试信息）
    std::unordered_map<entt::id_type, ActionEntry> actions_;

    /// @brief 动作 ID 的稳定顺序列表（按配置加载顺序），用于 dispatchActionCallbacks
    std::vector<entt::id_type> action_dispatch_order_;

    /// @brief 键盘 scancode → 关联的动作 ID 列表
    std::unordered_map<SDL_Scancode, std::vector<entt::id_type>> key_to_actions_;
    /// @brief 鼠标按钮 → 关联的动作 ID 列表
    std::unordered_map<Uint32, std::vector<entt::id_type>> mouse_to_actions_;

    /// @brief 键盘按键当前是否按下（边沿检测）
    std::unordered_map<SDL_Scancode, bool> key_down_states_;
    /// @brief 鼠标按钮当前是否按下（边沿检测）
    std::unordered_map<Uint32, bool> mouse_down_states_;

    glm::vec2 mouse_position_{0.0f, 0.0f};                          ///< @brief 鼠标位置 (针对屏幕坐标)
    glm::vec2 logical_mouse_position_{0.0f, 0.0f};                  ///< @brief 鼠标位置 (针对逻辑坐标)
    glm::vec2 mouse_wheel_delta_{0.0f, 0.0f};                       ///< @brief 鼠标滚轮 delta
#ifdef TF_ENABLE_DEBUG_UI
    std::function<void(const SDL_Event&)> imgui_event_callback_;
#endif

public:
    static constexpr std::string_view DEFAULT_CONFIG_PATH{"config/input.json"};

    /**
     * @brief 创建并初始化输入管理器。
     * @param dispatcher 事件分发器（不能为空）
     * @param game_state 游戏状态对象（不能为空）
     * @return 创建成功返回实例；失败返回 nullptr。
     */
    [[nodiscard]] static std::unique_ptr<InputManager> create(entt::dispatcher* dispatcher,
                                                              engine::core::GameState* game_state,
                                                              std::string_view config_path = DEFAULT_CONFIG_PATH);

    /**
     * @brief 注册一个动作的回调函数
     * @param action_name_id 动作名称哈希
     * @param action_state 动作状态, 默认为按下瞬间
     * @return 一个 sink 对象，用于注册回调函数
     */
    entt::sink<entt::sigh<bool()>> onAction(entt::id_type action_name_id, ActionState action_state = ActionState::PRESSED);


    /**
     * @brief 兼容入口：执行一次"消费上一 tick 的边沿状态 -> 采样事件 -> 分发回调"。
     *
     * 该接口保留给旧调用路径和测试；fixed-step 主循环应改用 sampleInputEvents /
     * dispatchActionCallbacks / consumeTick 三段式接口。
     */
    void update();

    /**
     * @brief 采样 SDL 事件（建议每个渲染帧调用一次）。
     *
     * 只负责更新动作状态与鼠标数据，不推进 PRESSED/RELEASED 的生命周期。
     */
    void sampleInputEvents();

    /**
     * @brief 按当前动作状态分发回调（建议每个逻辑 tick 调用一次，且在逻辑更新前）。
     */
    void dispatchActionCallbacks();

    /**
     * @brief 消费一个逻辑 tick：推进边沿状态生命周期并清理一次性输入（如滚轮 delta）。
     *
     * 状态推进：
     * - PRESSED -> HELD
     * - RELEASED -> INACTIVE
     */
    void consumeTick();

    void quit();                                      ///< @brief 退出游戏

    // 保留动作状态检查, 提供不同的使用选择
    bool isActionDown(entt::id_type action_name_id) const;        ///< @brief 动作当前是否触发 (持续按下或本帧按下)
    bool isActionPressed(entt::id_type action_name_id) const;     ///< @brief 动作是否在本帧刚刚按下
    bool isActionReleased(entt::id_type action_name_id) const;    ///< @brief 动作是否在本帧刚刚释放

    glm::vec2 getMousePosition() const;                              ///< @brief 获取鼠标位置 （屏幕坐标）
    glm::vec2 getLogicalMousePosition() const;                       ///< @brief 获取鼠标位置 （逻辑坐标）
    glm::vec2 getMouseWheelDelta() const;                            ///< @brief 获取鼠标滚轮 delta
    void setImGuiEventForwarder(std::function<void(const SDL_Event&)> callback);

    const std::unordered_map<entt::id_type, ActionEntry>& getActionsDebug() const { return actions_; }

    /**
     * @brief 调试方法：手动设置动作状态
     * @param action_name_id 动作名称哈希
     * @param state 要设置的状态
     * @note 仅用于调试目的，允许通过调试面板手动触发动作状态
     */
    void setActionStateDebug(entt::id_type action_name_id, ActionState state);

private:
    InputManager(entt::dispatcher* dispatcher,
                 engine::core::GameState* game_state,
                 std::string_view config_path);

    void processEvent(const SDL_Event& event);                      ///< @brief 处理 SDL 事件（将按键转换为动作状态）
    [[nodiscard]] bool loadConfig(std::string_view config_path);
    void initializeMappings(const std::map<std::string, std::vector<std::string>>& actions_to_keyname);

    SDL_Scancode scancodeFromString(std::string_view key_name);     ///< @brief 将字符串键名转换为 SDL_Scancode
    Uint32 mouseButtonFromString(std::string_view button_name);     ///< @brief 将字符串按钮名转换为 SDL_Button
    void recalculateLogicalMousePosition();                         ///< @brief 根据当前窗口/逻辑尺寸更新逻辑坐标
};

} // namespace engine::input
