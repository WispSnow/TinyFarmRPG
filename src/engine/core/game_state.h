#pragma once
#include <glm/vec2.hpp>
#include <SDL3/SDL_video.h>
#include <memory>

namespace engine::core {

/**
 * @enum State
 * @brief 定义游戏可能处于的宏观状态。
 */
enum class State {
    Title,          ///< @brief 标题界面
    Playing,        ///< @brief 正常游戏进行中
    Paused,         ///< @brief 游戏暂停（通常覆盖菜单界面）
    GameOver,       ///< @brief 游戏结束界面
    LevelClear      ///< @brief 关卡过关界面
    // 可以根据需要添加更多状态，如 Cutscene, SettingsMenu 等
};

/**
 * @brief 管理和查询游戏的全局宏观状态。
 *
 * 提供一个中心点来确定游戏当前处于哪个主要模式，
 * 以便其他系统（输入、渲染、更新等）可以相应地调整其行为。
 */
class GameState final {
private:    
    SDL_Window* window_ = nullptr;              ///< @brief SDL窗口，用于获取窗口大小
    State current_state_ = State::Title;        ///< @brief 当前游戏状态
    glm::vec2 logical_size_{1.0f, 1.0f};        ///< @brief 当前逻辑分辨率

    explicit GameState(SDL_Window* window, State initial_state);

public:
    /**
     * @brief 创建并初始化游戏状态。
     * @param window SDL窗口，必须传入有效值。
     * @param initial_state 游戏的初始状态，默认为 Title。
     * @return 创建成功返回实例；失败返回 nullptr。
     */
    [[nodiscard]] static std::unique_ptr<GameState> create(SDL_Window* window, State initial_state = State::Title);

    State getCurrentState() const { return current_state_; }
    void setState(State new_state);
    glm::vec2 getWindowSize() const;
    void setWindowSize(const glm::vec2& window_size);   // 这里并不涉及到(成员变量)赋值，所以不需要move
    glm::vec2 getLogicalSize() const;
    void setLogicalSize(const glm::vec2& logical_size);


    // --- 便捷查询方法 ---

    bool isInTitle() const { return current_state_ == State::Title; }
    bool isPlaying() const { return current_state_ == State::Playing; }
    bool isPaused() const { return current_state_ == State::Paused; }
    bool isGameOver() const { return current_state_ == State::GameOver; }
    bool isLevelClear() const { return current_state_ == State::LevelClear; }

};

} // namespace engine::core
