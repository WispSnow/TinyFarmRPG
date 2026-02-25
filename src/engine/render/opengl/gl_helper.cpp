#include "gl_helper.h"

namespace engine::render::opengl {

bool createFBOWithColorAttachment(int width,
                                  int height,
                                  const GLColorAttachmentDesc& desc,
                                  GLuint& out_fbo,
                                  GLuint& out_tex) {
    out_fbo = 0;
    out_tex = 0;
    if (width <= 0 || height <= 0) {
        spdlog::error("createFBOWithColorAttachment: invalid size {}x{}", width, height);
        return false;
    }

    const ScopedGLUnpackAlignment scoped_unpack_alignment(desc.unpack_alignment);

    GLuint fbo = 0;
    GLuint tex = 0;
    glGenFramebuffers(1, &fbo);
    glGenTextures(1, &tex);
    if (fbo == 0 || tex == 0) {
        if (tex != 0) {
            glDeleteTextures(1, &tex);
        }
        if (fbo != 0) {
            glDeleteFramebuffers(1, &fbo);
        }
        spdlog::error("createFBOWithColorAttachment: glGenFramebuffers/glGenTextures failed");
        return false;
    }

    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, desc.min_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, desc.mag_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, desc.wrap_s);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, desc.wrap_t);
    glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(desc.internal_format),
                 width, height, 0, desc.format, desc.type, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        glDeleteTextures(1, &tex);
        glDeleteFramebuffers(1, &fbo);
        spdlog::error("createFBOWithColorAttachment: framebuffer incomplete, status={:#x}",
                      static_cast<unsigned int>(status));
        return false;
    }

    out_fbo = fbo;
    out_tex = tex;
    return logGlErrors("createFBOWithColorAttachment");
}

void ScopedGLBlendFunc::capturePreviousState() {
    glGetBooleanv(GL_BLEND, &previous_enabled_);
    glGetIntegerv(GL_BLEND_SRC_RGB, &previous_src_rgb_);
    glGetIntegerv(GL_BLEND_DST_RGB, &previous_dst_rgb_);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &previous_src_alpha_);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &previous_dst_alpha_);
}

void ScopedGLBlendFunc::restorePreviousState() const {
    if (previous_enabled_ == GL_TRUE) {
        glEnable(GL_BLEND);
    } else {
        glDisable(GL_BLEND);
    }
    glBlendFuncSeparate(previous_src_rgb_, previous_dst_rgb_, previous_src_alpha_, previous_dst_alpha_);
}

ScopedGLBlendFunc::ScopedGLBlendFunc(GLenum src_factor, GLenum dst_factor, bool ensure_enabled) {
    capturePreviousState();
    if (ensure_enabled) {
        glEnable(GL_BLEND);
    }
    glBlendFunc(src_factor, dst_factor);
}

ScopedGLBlendFunc::ScopedGLBlendFunc(GLenum src_rgb,
                                     GLenum dst_rgb,
                                     GLenum src_alpha,
                                     GLenum dst_alpha,
                                     bool ensure_enabled) {
    capturePreviousState();
    if (ensure_enabled) {
        glEnable(GL_BLEND);
    }
    glBlendFuncSeparate(src_rgb, dst_rgb, src_alpha, dst_alpha);
}

ScopedGLBlendFunc::~ScopedGLBlendFunc() {
    restorePreviousState();
}

} // namespace engine::render::opengl
