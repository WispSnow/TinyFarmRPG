#pragma once

#include <glm/vec3.hpp>
#include <string_view>
#include "engine/debug/debug_panel.h"

namespace engine::render::opengl {
class GLRenderer;
}

namespace engine::render::opengl {

class GLRendererDebugPanel final : public engine::debug::DebugPanel {
    GLRenderer& renderer_;

    bool vsync_enabled_{true};
    int swap_interval_{1};
    bool pixel_snap_enabled_{true};
    bool viewport_clipping_enabled_{true};

    bool point_lights_enabled_{true};
    bool spot_lights_enabled_{true};
    bool directional_lights_enabled_{true};
    bool emissive_enabled_{true};
    bool bloom_enabled_{true};

    glm::vec3 ambient_{0.5f, 0.5f, 0.5f};
    float bloom_strength_{1.0f};
    float bloom_sigma_{2.5f};

public:
    explicit GLRendererDebugPanel(GLRenderer& renderer);

    // DebugPanel overrides
    std::string_view name() const override;
    void draw(bool& is_open) override;
    void onShow() override;

private:
    void syncFromRenderer();
};

} // namespace engine::render::opengl
