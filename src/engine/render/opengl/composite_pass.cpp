// -----------------------------------------------------------------------------
// composite_pass.cpp
// -----------------------------------------------------------------------------
// 实现了 CompositePass，负责将场景、光照、自发光及泛光等缓冲区混合输出至最终的颜色缓冲。
// -----------------------------------------------------------------------------
#include "composite_pass.h"
#include "gl_helper.h"
#include "fullscreen_quad.h"
#include "default_textures.h"
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
        u_world_vfx_tex_ = program_->uniformLocation("uWorldVfxTex");
        u_bloom_strength_ = program_->uniformLocation("uBloomStrength");
        if (u_scene_tex_ >= 0) glUniform1i(u_scene_tex_, 0);
        if (u_light_tex_ >= 0) glUniform1i(u_light_tex_, 1);
        if (u_emissive_tex_ >= 0) glUniform1i(u_emissive_tex_, 2);
        if (u_bloom_tex_ >= 0) glUniform1i(u_bloom_tex_, 3);
        if (u_world_vfx_tex_ >= 0) glUniform1i(u_world_vfx_tex_, 4);
        glUseProgram(0);
    }

    if (!default_textures_acquired_) {
        if (!DefaultTextures::acquire(white_tex_, black_tex_)) {
            spdlog::error("CompositePass::init: acquire default textures failed");
            return false;
        }
        default_textures_acquired_ = true;
        // 默认使用白色纹理作为光照纹理，黑色纹理作为场景/自发光/泛光纹理
        light_tex_ = white_tex_;
        scene_tex_ = black_tex_;
        emissive_tex_ = black_tex_;
        bloom_tex_ = black_tex_;
        world_vfx_tex_ = black_tex_;
    }
    return logGlErrors("CompositePass::init");
}

bool CompositePass::createBuffers() {
    if (vao_ != 0 && vertex_count_ > 0) {
        return true;
    }
    if (!FullscreenQuad::acquire(vao_, vertex_count_)) {
        spdlog::error("CompositePass::createBuffers: acquire shared fullscreen quad failed");
        return false;
    }
    return true;
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
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, world_vfx_tex_);

    // 绘制
    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, vertex_count_);
    glBindVertexArray(0);

    // 解除绑定所有使用的纹理单元
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, 0);
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
    if (vao_ != 0) {
        FullscreenQuad::release();
        vao_ = 0;
    }
    vertex_count_ = 0;

    if (default_textures_acquired_) {
        DefaultTextures::release();
        default_textures_acquired_ = false;
    }
    white_tex_ = 0;
    black_tex_ = 0;
    
    program_ = nullptr;
    scene_tex_ = 0;
    light_tex_ = 0;
    emissive_tex_ = 0;
    bloom_tex_ = 0;
    world_vfx_tex_ = 0;
}

} // namespace engine::render::opengl
