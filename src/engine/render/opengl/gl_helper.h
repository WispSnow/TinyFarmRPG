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

} // namespace engine::render::opengl
