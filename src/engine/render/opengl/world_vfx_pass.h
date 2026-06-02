#pragma once

#include "engine/render/opengl/render_pass.h"
#include "engine/vfx/vfx_types.h"

#include "engine/platform/gl_platform.h"
#include <glm/vec2.hpp>

#include <cstdint>
#include <memory>

namespace engine::vfx {
class VfxBackend;
}

namespace engine::render::opengl {

class WorldVfxPass final : public RenderPass {
public:
    struct Stats {
        std::uint32_t draw_calls{0u};
        std::uint32_t instance_count{0u};
    };

    [[nodiscard]] static std::unique_ptr<WorldVfxPass> create(const glm::vec2& viewport_size);
    [[nodiscard]] static std::unique_ptr<WorldVfxPass> createHeadless();
    ~WorldVfxPass() override;

    WorldVfxPass(const WorldVfxPass&) = delete;
    WorldVfxPass& operator=(const WorldVfxPass&) = delete;
    WorldVfxPass(WorldVfxPass&&) = delete;
    WorldVfxPass& operator=(WorldVfxPass&&) = delete;

    void setBackend(engine::vfx::VfxBackend* backend);
    void clear();
    [[nodiscard]] Stats flush(const engine::vfx::VfxRenderContext& context);
    [[nodiscard]] GLuint getColorTex() const { return color_tex_; }
    void clean() override;

private:
    explicit WorldVfxPass(int viewport_width, int viewport_height, bool headless = false)
        : viewport_width_(viewport_width), viewport_height_(viewport_height), is_headless_(headless) {
    }

    [[nodiscard]] bool init();
    [[nodiscard]] bool createFBO(int width, int height);
    void destroyFBO();

    int viewport_width_{0};
    int viewport_height_{0};
    bool is_headless_{false};
    GLuint fbo_{0};
    GLuint color_tex_{0};
    engine::vfx::VfxBackend* backend_{nullptr};
};

} // namespace engine::render::opengl
