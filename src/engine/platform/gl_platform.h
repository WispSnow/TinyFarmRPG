#pragma once

#if defined(__EMSCRIPTEN__)
#include <GLES3/gl3.h>
#define TF_GL_PLATFORM_WEBGL 1
#else
#include <glad/glad.h>
#define TF_GL_PLATFORM_WEBGL 0
#endif

#if defined(__EMSCRIPTEN__) && !defined(GL_FRAMEBUFFER_SRGB)
#define GL_FRAMEBUFFER_SRGB 0x8DB9
#endif

namespace engine::platform::gl {

inline constexpr bool kIsWebGL = TF_GL_PLATFORM_WEBGL != 0;
inline constexpr bool kSupportsDefaultFramebufferSrgb = !kIsWebGL;
inline constexpr bool kSupportsFloatColorFramebuffers = !kIsWebGL;
inline constexpr bool kSupportsLinearFloatFiltering = !kIsWebGL;
inline constexpr GLenum kTextureColorInternalFormat = kIsWebGL ? GL_RGBA8 : GL_SRGB8_ALPHA8;
inline constexpr const char* kGlslVersion =
    kIsWebGL ? "#version 300 es" : "#version 330 core";

inline void clearDepth(float depth) {
#if TF_GL_PLATFORM_WEBGL
    glClearDepthf(depth);
#else
    glClearDepth(static_cast<GLdouble>(depth));
#endif
}

} // namespace engine::platform::gl
