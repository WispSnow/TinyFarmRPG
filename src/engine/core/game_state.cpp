#include "game_state.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace engine::core {

std::unique_ptr<GameState> GameState::create(SDL_Window* window, State initial_state) {
    if (window == nullptr) {
        spdlog::error("创建 GameState 失败：窗口为空。");
        return nullptr;
    }
    return std::unique_ptr<GameState>(new GameState(window, initial_state));
}

GameState::GameState(SDL_Window* window, State initial_state) : window_(window), current_state_(initial_state) {
    int width = 0;
    int height = 0;
    SDL_GetWindowSize(window_, &width, &height);
    logical_size_ = glm::vec2(
        std::max(1, width),
        std::max(1, height)
    );
    spdlog::trace("游戏状态初始化完成。");
}

void GameState::setState(State new_state) {
    if (current_state_ != new_state) {
        spdlog::debug("游戏状态改变");
        current_state_ = new_state;
    } else {
        spdlog::debug("尝试设置相同的游戏状态，跳过");
    }
}

glm::vec2 GameState::getWindowSize() const
{
    int width, height;
    // SDL3获取窗口大小的方法
    SDL_GetWindowSize(window_, &width, &height);
    return glm::vec2(width, height);
}

void GameState::setWindowSize(const glm::vec2& window_size)
{
    SDL_SetWindowSize(window_, static_cast<int>(window_size.x), static_cast<int>(window_size.y));
}

glm::vec2 GameState::getLogicalSize() const
{
    return logical_size_;
}


void GameState::setLogicalSize(const glm::vec2& logical_size)
{
    logical_size_ = glm::vec2(
        std::max(1.0f, logical_size.x),
        std::max(1.0f, logical_size.y)
    );
}


} // namespace engine::core 
