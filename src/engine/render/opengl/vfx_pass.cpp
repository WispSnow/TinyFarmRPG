#include "engine/render/opengl/vfx_pass.h"

#include "engine/vfx/vfx_backend.h"

#include "engine/platform/gl_platform.h"

#include <cmath>

namespace engine::render::opengl {

void VfxPass::setBackend(engine::vfx::VfxBackend* backend) {
    backend_ = backend;
}

VfxPass::Stats VfxPass::flush(const engine::vfx::VfxRenderContext& context) {
    if (backend_ == nullptr) {
        return {};
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(
        static_cast<GLint>(std::round(context.viewport_pixels.pos.x)),
        static_cast<GLint>(std::round(context.viewport_pixels.pos.y)),
        static_cast<GLsizei>(std::round(context.viewport_pixels.size.x)),
        static_cast<GLsizei>(std::round(context.viewport_pixels.size.y)));

    backend_->render(context);

    return {
        backend_->getLastDrawCallCount(),
        backend_->getLastInstanceCount()
    };
}

void VfxPass::clean() {
    backend_ = nullptr;
}

} // namespace engine::render::opengl
