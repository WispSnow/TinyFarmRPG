#pragma once

#include "engine/debug/debug_panel.h"
#include "engine/core/game_state.h"

namespace engine::render {
    class Camera;
}

namespace engine::debug {

class GameStateDebugPanel final : public DebugPanel {
    engine::core::GameState& game_state_;
    engine::render::Camera& camera_;
    engine::core::State selected_state_;

public:
    GameStateDebugPanel(engine::core::GameState& game_state, engine::render::Camera& camera);

    std::string_view name() const override;
    void draw(bool& is_open) override;
    void onShow() override;

private:
    void syncFromGameState();
    const char* stateToString(engine::core::State state) const;
};

} // namespace engine::debug
