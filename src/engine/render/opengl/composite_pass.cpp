// -----------------------------------------------------------------------------
// composite_pass.cpp
// -----------------------------------------------------------------------------
// 实现了 CompositePass，负责将场景、光照、自发光及泛光等缓冲区混合输出至最终的颜色缓冲。
// -----------------------------------------------------------------------------
#include "composite_pass.h"
#include "gl_helper.h"
#include "shader_asset_paths.h"
#include "shader_library.h"
#include "shader_program.h"
#include <spdlog/spdlog.h>
#include <glm/gtc/type_ptr.hpp>
#include <entt/core/hashed_string.hpp>

using namespace entt::literals;

namespace engine::render::opengl {

std::unique_ptr<CompositePass> CompositePass::create(ShaderLibrary& library) {
    auto pass = std::unique_ptr<CompositePass>(new CompositePass());
    if (!pass->init(library)) {
        spdlog::error("CompositePass 初始化失败");
        return nullptr;
    }
    if (!pass->createBuffers()) {
        spdlog::error("CompositePass 创建屏幕空间缓冲失败");
        return nullptr;
    }
    return pass;
}

CompositePass::~CompositePass() {
    clean();
}

bool CompositePass::init(ShaderLibrary& library) {
    if (!program_) {
        program_ =
            library.loadProgram("composite"_hs, shader_assets::COMPOSITE_VERTEX_PATH, shader_assets::COMPOSITE_FRAGMENT_PATH);
        if (!program_) {
            spdlog::error("CompositePass 加载着色器失败");
            return false;
        }
        program_->use();
        u_scene_tex_ = program_->uniformLocation("uSceneTex");
        u_light_tex_ = program_->uniformLocation("uLightTex");
        u_ambient_ = program_->uniformLocation("uAmbient");
        u_emissive_tex_ = program_->uniformLocation("uEmissiveTex");
        u_bloom_tex_ = program_->uniformLocation("uBloomTex");
        u_bloom_strength_ = program_->uniformLocation("uBloomStrength");
        if (u_scene_tex_ >= 0) glUniform1i(u_scene_tex_, 0);
        if (u_light_tex_ >= 0) glUniform1i(u_light_tex_, 1);
        if (u_emissive_tex_ >= 0) glUniform1i(u_emissive_tex_, 2);
        if (u_bloom_tex_ >= 0) glUniform1i(u_bloom_tex_, 3);
        glUseProgram(0);
        
        const ScopedGLUnpackAlignment scoped_unpack_alignment(4);

        // 创建默认的 1x1 白色纹理（用于光照）
        glGenTextures(1, &white_tex_);
        glBindTexture(GL_TEXTURE_2D, white_tex_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        const uint32_t white_pixel = 0xFFFFFFFFu;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &white_pixel);
        glBindTexture(GL_TEXTURE_2D, 0);
        // 默认使用白色纹理作为光照纹理
        light_tex_ = white_tex_;

        // 创建默认的 1x1 黑色纹理（用于自发光和泛光）
        glGenTextures(1, &black_tex_);
        glBindTexture(GL_TEXTURE_2D, black_tex_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        const uint32_t black_pixel = 0x00000000u;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &black_pixel);
        glBindTexture(GL_TEXTURE_2D, 0);
        // 默认使用黑色纹理作为场景纹理、自发光和泛光纹理
        scene_tex_ = black_tex_;
        emissive_tex_ = black_tex_;
        bloom_tex_ = black_tex_;
    }
    return logGlErrors("CompositePass::init");
}

bool CompositePass::createBuffers() {
    if (vao_ != 0) {
        return true;
    }
    const float quad[] = {
        // pos      // uv
        -1.f, -1.f, 0.f, 0.f,
         1.f, -1.f, 1.f, 0.f,
         1.f,  1.f, 1.f, 1.f,
        -1.f, -1.f, 0.f, 0.f,
         1.f,  1.f, 1.f, 1.f,
        -1.f,  1.f, 0.f, 1.f,
    };
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    vertex_count_ = 6;
    return logGlErrors("CompositePass::createBuffers");
}

bool CompositePass::render(const utils::Rect& viewport) {
    if (!program_ || vao_ == 0 || vertex_count_ <= 0) {
        spdlog::error("CompositePass::render: 没有有效的 program 或顶点缓冲");
        return false;
    }

    // 如果某些纹理未设置，则使用默认纹理：
    // - 光照纹理使用白色（全光照）
    // - 自发光和泛光纹理使用黑色（无发光）
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(
        static_cast<GLint>(std::round(viewport.pos.x)), 
        static_cast<GLint>(std::round(viewport.pos.y)), 
        static_cast<GLsizei>(std::round(viewport.size.x)), 
        static_cast<GLsizei>(std::round(viewport.size.y)));
    program_->use();
    if (u_scene_tex_ >= 0) glUniform1i(u_scene_tex_, 0);
    if (u_light_tex_ >= 0) glUniform1i(u_light_tex_, 1);
    if (u_emissive_tex_ >= 0) glUniform1i(u_emissive_tex_, 2);
    if (u_bloom_tex_ >= 0) glUniform1i(u_bloom_tex_, 3);
    if (u_ambient_ >= 0) glUniform3fv(u_ambient_, 1, glm::value_ptr(ambient_));
    if (u_bloom_strength_ >= 0) glUniform1f(u_bloom_strength_, bloom_strength_);

    // 绑定纹理，所有纹理都至少有默认值。
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, scene_tex_);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, light_tex_);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, emissive_tex_);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, bloom_enabled_ ? bloom_tex_ : black_tex_);

    // 绘制
    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, vertex_count_);
    glBindVertexArray(0);

    // 解除绑定所有使用的纹理单元
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    glUseProgram(0);
    return logGlErrors("CompositePass::render");
}

void CompositePass::clean() {
    if (vbo_ != 0) {
        glDeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
    if (vao_ != 0) {
        glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }
    vertex_count_ = 0;

    // 释放默认纹理
    if (white_tex_ != 0) {
        glDeleteTextures(1, &white_tex_);
        white_tex_ = 0;
    }
    if (black_tex_ != 0) {
        glDeleteTextures(1, &black_tex_);
        black_tex_ = 0;
    }
    
    program_ = nullptr;
    scene_tex_ = 0;
    light_tex_ = 0;
    emissive_tex_ = 0;
    bloom_tex_ = 0;
}

} // namespace engine::render::opengl
