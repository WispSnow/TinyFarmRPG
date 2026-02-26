#pragma once

#include "engine/debug/debug_panel.h"
#include <glm/vec2.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace engine::vfx {
class VfxService;
}

namespace engine::render::opengl {
class GLRenderer;
}

namespace engine::debug {

class VfxDebugPanel final : public DebugPanel {
public:
    explicit VfxDebugPanel(engine::vfx::VfxService& vfx_service,
                           engine::render::opengl::GLRenderer& gl_renderer);

    [[nodiscard]] std::string_view name() const override;
    void draw(bool& is_open) override;
    void onShow() override;

private:
    void scanEffectFiles();

    engine::vfx::VfxService& vfx_service_;
    engine::render::opengl::GLRenderer& gl_renderer_;

    int selected_effect_{0};
    std::vector<std::string> effect_paths_{};
    glm::vec2 spawn_position_{0.0f, 0.0f};
    float spawn_z_{0.0f};
    float spawn_scale_{1.0f};
    float camera_elevation_deg_{0.0f};
    bool auto_spawn_{false};
    float auto_spawn_interval_{2.0f};
    float auto_spawn_timer_{0.0f};
};

} // namespace engine::debug
