#pragma once

#include <string_view>
#include <glad/glad.h>
#include <spdlog/spdlog.h>

namespace engine::render::opengl {
/**
 * @brief 在DEBUG模式下检查 OpenGL 错误并记录日志（在RELEASE模式下不检查）
 * @param label 错误标签
 * @return 是否存在错误
 */
inline bool logGlErrors(std::string_view label) {
#ifndef NDEBUG
    bool ok = true;
    GLenum error = GL_NO_ERROR;
    while ((error = glGetError()) != GL_NO_ERROR) {
        spdlog::error("[OpenGL] {} error: 0x{}", label, error);
        ok = false;
    }
    return ok;
#else
    (void)label;  // 在RELEASE模式下不检查，避免编译器警告
    return true;
#endif // NDEBUG
}

/**
 * @brief RAII 保存并恢复 glPixelStorei(GL_UNPACK_ALIGNMENT, ...)
 */
class ScopedGLUnpackAlignment final {
    GLint previous_{4};

public:
    explicit ScopedGLUnpackAlignment(GLint alignment) {
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &previous_);
        if (previous_ == 0) {
            previous_ = 4;
        }
        glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
    }

    ~ScopedGLUnpackAlignment() {
        glPixelStorei(GL_UNPACK_ALIGNMENT, previous_);
    }

    ScopedGLUnpackAlignment(const ScopedGLUnpackAlignment&) = delete;
    ScopedGLUnpackAlignment& operator=(const ScopedGLUnpackAlignment&) = delete;
};

struct GLColorAttachmentDesc final {
    GLenum internal_format{GL_RGBA8};
    GLenum format{GL_RGBA};
    GLenum type{GL_UNSIGNED_BYTE};
    GLint min_filter{GL_NEAREST};
    GLint mag_filter{GL_NEAREST};
    GLint wrap_s{GL_CLAMP_TO_EDGE};
    GLint wrap_t{GL_CLAMP_TO_EDGE};
    GLint unpack_alignment{4};
};

[[nodiscard]] bool createFBOWithColorAttachment(int width,
                                                int height,
                                                const GLColorAttachmentDesc& desc,
                                                GLuint& out_fbo,
                                                GLuint& out_tex);

class ScopedGLBlendFunc final {
    GLboolean previous_enabled_{GL_FALSE};
    GLint previous_src_rgb_{GL_ONE};
    GLint previous_dst_rgb_{GL_ZERO};
    GLint previous_src_alpha_{GL_ONE};
    GLint previous_dst_alpha_{GL_ZERO};
    void capturePreviousState();
    void restorePreviousState() const;

public:
    ScopedGLBlendFunc(GLenum src_factor, GLenum dst_factor, bool ensure_enabled = true);
    ScopedGLBlendFunc(GLenum src_rgb,
                      GLenum dst_rgb,
                      GLenum src_alpha,
                      GLenum dst_alpha,
                      bool ensure_enabled = true);
    ~ScopedGLBlendFunc();

    ScopedGLBlendFunc(const ScopedGLBlendFunc&) = delete;
    ScopedGLBlendFunc& operator=(const ScopedGLBlendFunc&) = delete;
};

} // namespace engine::render::opengl
