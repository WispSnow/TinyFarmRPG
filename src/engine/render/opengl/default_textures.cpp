#include "default_textures.h"
#include "gl_helper.h"
#include <cstdint>
#include <spdlog/spdlog.h>

namespace engine::render::opengl {
namespace {

struct SharedDefaultTextureState final {
    GLuint white_tex{0};
    GLuint black_tex{0};
    uint32_t ref_count{0};
};

SharedDefaultTextureState& sharedState() {
    static SharedDefaultTextureState state{};
    return state;
}

void destroy(SharedDefaultTextureState& state) {
    if (state.white_tex != 0) {
        glDeleteTextures(1, &state.white_tex);
        state.white_tex = 0;
    }
    if (state.black_tex != 0) {
        glDeleteTextures(1, &state.black_tex);
        state.black_tex = 0;
    }
}

bool createSolidTexture(uint32_t pixel, GLuint& out_tex) {
    out_tex = 0;
    glGenTextures(1, &out_tex);
    if (out_tex == 0) {
        spdlog::error("DefaultTextures::createSolidTexture: glGenTextures failed");
        return false;
    }

    glBindTexture(GL_TEXTURE_2D, out_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    const ScopedGLUnpackAlignment scoped_unpack_alignment(4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &pixel);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (!logGlErrors("DefaultTextures::createSolidTexture")) {
        glDeleteTextures(1, &out_tex);
        out_tex = 0;
        return false;
    }
    return true;
}

bool initialize(SharedDefaultTextureState& state) {
    destroy(state);
    if (!createSolidTexture(0xFFFFFFFFu, state.white_tex)) {
        return false;
    }
    if (!createSolidTexture(0x00000000u, state.black_tex)) {
        destroy(state);
        return false;
    }
    return true;
}

} // namespace

bool DefaultTextures::acquire(GLuint& out_white_tex, GLuint& out_black_tex) {
    auto& state = sharedState();
    if (state.white_tex == 0 || state.black_tex == 0) {
        if (!initialize(state)) {
            out_white_tex = 0;
            out_black_tex = 0;
            return false;
        }
    }
    ++state.ref_count;
    out_white_tex = state.white_tex;
    out_black_tex = state.black_tex;
    return true;
}

void DefaultTextures::release() {
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
