#pragma once

#include <glad/glad.h>

namespace engine::render::opengl {

class FullscreenQuad final {
public:
    [[nodiscard]] static bool acquire(GLuint& out_vao, GLsizei& out_vertex_count);
    static void release();

private:
    FullscreenQuad() = delete;
};

} // namespace engine::render::opengl
