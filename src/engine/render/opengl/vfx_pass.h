#pragma once

#include "engine/render/opengl/render_pass.h"
#include "engine/vfx/vfx_types.h"

#include <cstdint>

namespace engine::vfx {
class VfxBackend;
}

namespace engine::render::opengl {

class VfxPass final : public RenderPass {
public:
    struct Stats {
        std::uint32_t draw_calls{0u};
        std::uint32_t instance_count{0u};
    };

    void setBackend(engine::vfx::VfxBackend* backend);
    [[nodiscard]] Stats flush(const engine::vfx::VfxRenderContext& context);
    void clean() override;

private:
    engine::vfx::VfxBackend* backend_{nullptr};
};

} // namespace engine::render::opengl
