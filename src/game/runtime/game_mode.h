#pragma once

namespace game::runtime {

enum class GameMode {
    Exploration,
    Battle,
    PauseOverlay,
    Cutscene
};

[[nodiscard]] inline const char* toString(GameMode mode) {
    switch (mode) {
        case GameMode::Exploration:
            return "Exploration";
        case GameMode::Battle:
            return "Battle";
        case GameMode::PauseOverlay:
            return "PauseOverlay";
        case GameMode::Cutscene:
            return "Cutscene";
    }

    return "Unknown";
}

} // namespace game::runtime
