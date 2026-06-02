#pragma once

#include <memory>

namespace engine::core {
class GameApp;
}

namespace game {

void initializeEnvironment();
[[nodiscard]] std::unique_ptr<engine::core::GameApp> createApp();
int run();

} // namespace game
