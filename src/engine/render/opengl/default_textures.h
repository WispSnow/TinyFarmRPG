#pragma once

#include "engine/platform/gl_platform.h"

namespace engine::render::opengl {

class DefaultTextures final {
public:
    [[nodiscard]] static bool acquire(GLuint& out_white_tex, GLuint& out_black_tex);
    static void release();

private:
    DefaultTextures() = delete;
};

} // namespace engine::render::opengl
