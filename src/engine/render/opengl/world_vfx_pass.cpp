#include "engine/render/opengl/world_vfx_pass.h"

#include "engine/render/opengl/gl_helper.h"
#include "engine/vfx/vfx_backend.h"

#include <spdlog/spdlog.h>

#include <cmath>

namespace engine::render::opengl {

std::unique_ptr<WorldVfxPass> WorldVfxPass::create(const glm::vec2& viewport_size) {
    auto pass = std::unique_ptr<WorldVfxPass>(
        new WorldVfxPass(static_cast<int>(std::round(viewport_size.x)),
                         static_cast<int>(std::round(viewport_size.y))));
    if (!pass->init()) {
        spdlog::error("WorldVfxPass 初始化失败。");
        return nullptr;
    }
    return pass;
}

WorldVfxPass::~WorldVfxPass() {
    clean();
}

bool WorldVfxPass::init() {
    if (!createFBO(viewport_width_, viewport_height_)) {
        spdlog::error("WorldVfxPass 创建 FBO 失败。");
        return false;
    }
    return true;
}

bool WorldVfxPass::createFBO(const int width, const int height) {
    destroyFBO();

    GLuint fbo = 0;
    GLuint color = 0;
    GLColorAttachmentDesc desc{};
    desc.internal_format = GL_RGBA8;
    desc.format = GL_RGBA;
    desc.type = GL_UNSIGNED_BYTE;
    desc.min_filter = GL_LINEAR;
    desc.mag_filter = GL_LINEAR;
    desc.unpack_alignment = 4;
    if (!createFBOWithColorAttachment(width, height, desc, fbo, color)) {
        spdlog::error("WorldVfxPass: createFBOWithColorAttachment 失败。");
        return false;
    }

    fbo_ = fbo;
    color_tex_ = color;
    return logGlErrors("WorldVfxPass::createFBO");
}

void WorldVfxPass::destroyFBO() {
    if (color_tex_ != 0) {
        glDeleteTextures(1, &color_tex_);
        color_tex_ = 0;
    }
    if (fbo_ != 0) {
        glDeleteFramebuffers(1, &fbo_);
        fbo_ = 0;
    }
}

void WorldVfxPass::setBackend(engine::vfx::VfxBackend* backend) {
    backend_ = backend;
}

void WorldVfxPass::clear() {
    if (fbo_ == 0) {
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, viewport_width_, viewport_height_);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

WorldVfxPass::Stats WorldVfxPass::flush(const engine::vfx::VfxRenderContext& context) {
    if (fbo_ == 0 || backend_ == nullptr) {
        return {};
    }

    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, viewport_width_, viewport_height_);

    backend_->render(context);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return {
        backend_->getLastDrawCallCount(),
        backend_->getLastInstanceCount()
    };
}

void WorldVfxPass::clean() {
    backend_ = nullptr;
    destroyFBO();
}

} // namespace engine::render::opengl
