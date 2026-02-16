#include "bloom_pass.h"
#include "shader_asset_paths.h"
#include "shader_library.h"
#include "shader_program.h"
#include "gl_helper.h"
#include <entt/core/hashed_string.hpp>

using namespace entt::literals;

namespace engine::render::opengl {

std::unique_ptr<BloomPass> BloomPass::create(ShaderLibrary& library, const glm::vec2& viewport_size) {
    auto pass = std::unique_ptr<BloomPass>(new BloomPass(static_cast<int>(std::round(viewport_size.x)),
                                                         static_cast<int>(std::round(viewport_size.y))));
    if (!pass->init(library)) {
        spdlog::error("BloomPass 初始化失败");
        return nullptr;
    }
    return pass;
}

BloomPass::~BloomPass() {
    clean();
}

bool BloomPass::init(ShaderLibrary& library) {
    if (!blur_program_) {
        blur_program_ = library.loadProgram("blur"_hs, shader_assets::BLUR_VERTEX_PATH, shader_assets::BLUR_FRAGMENT_PATH);
        if (!blur_program_) {
            spdlog::error("BloomPass 加载高斯模糊着色器失败");
            return false;
        }
        blur_program_->use();
        GLint u_tex = blur_program_->uniformLocation("uTex");
        if (u_tex >= 0) glUniform1i(u_tex, 0);
        u_blur_texel_size_ = blur_program_->uniformLocation("uTexelSize");
        u_blur_direction_ = blur_program_->uniformLocation("uDirection");
        u_blur_sigma_ = blur_program_->uniformLocation("uSigma");
        glUseProgram(0);
    }

    if (!createBuffers()) {
        spdlog::error("BloomPass 创建全屏四边形几何体失败");
        return false;
    }

    if (!createTargets(viewport_width_, viewport_height_)) {
        spdlog::error("BloomPass 创建泛光纹理失败");
        return false;
    }

    return true;
}

bool BloomPass::createBuffers() {
    if (vao_ != 0 && vbo_ != 0) return true;
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    const float quad[] = {
        -1.f, -1.f, 0.f, 0.f,
         1.f, -1.f, 1.f, 0.f,
         1.f,  1.f, 1.f, 1.f,
        -1.f, -1.f, 0.f, 0.f,
         1.f,  1.f, 1.f, 1.f,
        -1.f,  1.f, 0.f, 1.f,
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    return logGlErrors("BloomPass::createBuffers");
}

bool BloomPass::createTargets(int width, int height) {
    destroyTargets();
    if (width <= 0 || height <= 0) {
        spdlog::error("BloomPass::createTargets: 宽度或高度小于等于 0");
        return false;
    }
    int w = std::max(1, width / 2);
    int h = std::max(1, height / 2);
    // 创建每个级别的 FBO 和纹理
    for (size_t i = 0; i < BLOOM_LEVELS; ++i) {
        level_width_[i] = w;
        level_height_[i] = h;
        glGenFramebuffers(1, &ping_fbo_[i]);
        glGenTextures(1, &ping_tex_[i]);
        glBindTexture(GL_TEXTURE_2D, ping_tex_[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glBindFramebuffer(GL_FRAMEBUFFER, ping_fbo_[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ping_tex_[i], 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            spdlog::error("BloomPass::createTargets: ping FBO incomplete (level {})", i);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            destroyTargets();
            return false;
        }

        glGenFramebuffers(1, &pong_fbo_[i]);
        glGenTextures(1, &pong_tex_[i]);
        glBindTexture(GL_TEXTURE_2D, pong_tex_[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glBindFramebuffer(GL_FRAMEBUFFER, pong_fbo_[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pong_tex_[i], 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            spdlog::error("BloomPass::createTargets: pong FBO incomplete (level {})", i);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            destroyTargets();
            return false;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        w = std::max(1, w / 2);
        h = std::max(1, h / 2);
    }
    bloom_tex_ = BLOOM_LEVELS > 0 ? pong_tex_[0] : 0;
    return logGlErrors("BloomPass::createTargets");
}

void BloomPass::destroyTargets() {
    for (size_t i = 0; i < BLOOM_LEVELS; ++i) {
        if (ping_tex_[i]) { glDeleteTextures(1, &ping_tex_[i]); ping_tex_[i] = 0; }
        if (ping_fbo_[i]) { glDeleteFramebuffers(1, &ping_fbo_[i]); ping_fbo_[i] = 0; }
        if (pong_tex_[i]) { glDeleteTextures(1, &pong_tex_[i]); pong_tex_[i] = 0; }
        if (pong_fbo_[i]) { glDeleteFramebuffers(1, &pong_fbo_[i]); pong_fbo_[i] = 0; }
        level_width_[i] = 0;
        level_height_[i] = 0;
    }
    bloom_tex_ = 0;
}

void BloomPass::clean() {
    destroyTargets();
    if (vbo_ != 0) { glDeleteBuffers(1, &vbo_); vbo_ = 0; }
    if (vao_ != 0) { glDeleteVertexArrays(1, &vao_); vao_ = 0; }
    blur_program_ = nullptr;
    last_draw_calls_ = 0;
    last_levels_ = 0;
}

bool BloomPass::process(GLuint emissive_tex) {
    if (!blur_program_ || vao_ == 0 || emissive_tex == 0) {
        spdlog::error("BloomPass::process: 没有有效的 blur_program 或 vao 或 emissive_tex");
        last_draw_calls_ = 0;
        last_levels_ = 0;
        return false;
    }

    uint32_t draw_calls = 0;

    // 下采样 + 高斯模糊
    blur_program_->use();
    glBindVertexArray(vao_);

    for (size_t i = 0; i < BLOOM_LEVELS; ++i) {
        const int w = level_width_[i];
        const int h = level_height_[i];

        glBindFramebuffer(GL_FRAMEBUFFER, ping_fbo_[i]);
        glViewport(0, 0, w, h);
        glActiveTexture(GL_TEXTURE0);
        if (i == 0) glBindTexture(GL_TEXTURE_2D, emissive_tex);
        else glBindTexture(GL_TEXTURE_2D, pong_tex_[i - 1]);

        if (u_blur_texel_size_ >= 0) {
            float srcW = static_cast<float>(i == 0 ? viewport_width_ : level_width_[i - 1]);
            float srcH = static_cast<float>(i == 0 ? viewport_height_ : level_height_[i - 1]);
            glUniform2f(u_blur_texel_size_, 1.0f / std::max(1.0f, srcW), 1.0f / std::max(1.0f, srcH));
        }
        if (u_blur_direction_ >= 0) glUniform2f(u_blur_direction_, 1.0f, 0.0f);
        if (u_blur_sigma_ >= 0) glUniform1f(u_blur_sigma_, sigma_);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        ++draw_calls;

        glBindFramebuffer(GL_FRAMEBUFFER, pong_fbo_[i]);
        glViewport(0, 0, w, h);
        if (u_blur_texel_size_ >= 0) glUniform2f(u_blur_texel_size_, 1.0f / float(w), 1.0f / float(h));
        if (u_blur_direction_ >= 0) glUniform2f(u_blur_direction_, 0.0f, 1.0f);
        glBindTexture(GL_TEXTURE_2D, ping_tex_[i]);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        ++draw_calls;
    }

    // 上采样 + 添加
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);

    for (int i = BLOOM_LEVELS - 1; i > 0; --i) {
        const int w = level_width_[i - 1];
        const int h = level_height_[i - 1];
        glBindFramebuffer(GL_FRAMEBUFFER, pong_fbo_[i - 1]);
        glViewport(0, 0, w, h);
        if (u_blur_texel_size_ >= 0) glUniform2f(u_blur_texel_size_, 1.0f / level_width_[i], 1.0f / level_height_[i]);
        if (u_blur_direction_ >= 0) glUniform2f(u_blur_direction_, 1.0f, 0.0f);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, pong_tex_[i]);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        ++draw_calls;
    }

    glBindVertexArray(0);
    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    bloom_tex_ = pong_tex_[0];
    last_draw_calls_ = draw_calls;
    last_levels_ = static_cast<uint32_t>(BLOOM_LEVELS);
    return true;
}

void BloomPass::clear() {
    if (pong_fbo_[0] == 0) {
        last_draw_calls_ = 0;
        last_levels_ = 0;
        return;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, pong_fbo_[0]);
    glViewport(0, 0, level_width_[0], level_height_[0]);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    bloom_tex_ = pong_tex_[0];
    last_draw_calls_ = 0;
    last_levels_ = 0;
}

} // namespace engine::render::opengl
