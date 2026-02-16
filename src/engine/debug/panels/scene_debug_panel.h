#pragma once

#include "engine/debug/debug_panel.h"

namespace engine::scene {
class SceneManager;
}

namespace engine::debug {

class SceneDebugPanel final : public DebugPanel {
    engine::scene::SceneManager& scene_manager_;

public:
    explicit SceneDebugPanel(engine::scene::SceneManager& scene_manager);

    [[nodiscard]] std::string_view name() const override;
    void draw(bool& is_open) override;
};

} // namespace engine::debug
