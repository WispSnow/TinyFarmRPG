#include "fullscreen_quad.h"
#include "gl_helper.h"
#include <cstdint>
#include <spdlog/spdlog.h>

namespace engine::render::opengl {
namespace {

struct SharedFullscreenQuadState final {
    GLuint vao{0};
    GLuint vbo{0};
    GLsizei vertex_count{6};
    uint32_t ref_count{0};
};

SharedFullscreenQuadState& sharedState() {
    static SharedFullscreenQuadState state{};
    return state;
}

void destroy(SharedFullscreenQuadState& state) {
    if (state.vbo != 0) {
        glDeleteBuffers(1, &state.vbo);
        state.vbo = 0;
    }
    if (state.vao != 0) {
        glDeleteVertexArrays(1, &state.vao);
        state.vao = 0;
    }
}

bool initialize(SharedFullscreenQuadState& state) {
    destroy(state);
    glGenVertexArrays(1, &state.vao);
    glGenBuffers(1, &state.vbo);
    if (state.vao == 0 || state.vbo == 0) {
        spdlog::error("FullscreenQuad::initialize: glGenVertexArrays/glGenBuffers failed");
        destroy(state);
        return false;
    }

    const float quad[] = {
        -1.f, -1.f, 0.f, 0.f,
         1.f, -1.f, 1.f, 0.f,
         1.f,  1.f, 1.f, 1.f,
        -1.f, -1.f, 0.f, 0.f,
         1.f,  1.f, 1.f, 1.f,
        -1.f,  1.f, 0.f, 1.f,
    };

    glBindVertexArray(state.vao);
    glBindBuffer(GL_ARRAY_BUFFER, state.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    if (!logGlErrors("FullscreenQuad::initialize")) {
        destroy(state);
        return false;
    }
    return true;
}

} // namespace

bool FullscreenQuad::acquire(GLuint& out_vao, GLsizei& out_vertex_count) {
    auto& state = sharedState();
    if (state.vao == 0 || state.vbo == 0) {
        if (!initialize(state)) {
            out_vao = 0;
            out_vertex_count = 0;
            return false;
        }
    }
    ++state.ref_count;
    out_vao = state.vao;
    out_vertex_count = state.vertex_count;
    return true;
}

void FullscreenQuad::release() {
    auto& state = sharedState();
    if (state.ref_count == 0) {
        return;
    }
    --state.ref_count;
    if (state.ref_count == 0) {
        destroy(state);
    }
}

} // namespace engine::render::opengl
