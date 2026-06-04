#include "scene_pass.h"
#include "gl_helper.h"
#include "shader_asset_paths.h"
#include "shader_program.h"
#include "shader_library.h"
#include <spdlog/spdlog.h>
#include <entt/core/hashed_string.hpp>

namespace engine::render::opengl {

using namespace entt::literals;

std::unique_ptr<ScenePass> ScenePass::create(ShaderLibrary& library, const glm::vec2& viewport_size) {
    auto pass = std::unique_ptr<ScenePass>(new ScenePass(static_cast<int>(std::round(viewport_size.x)),
                                                         static_cast<int>(std::round(viewport_size.y))));
    if (!pass->init(library)) {
        spdlog::error("为 ScenePass 初始化失败");
        return nullptr;
    }
    return pass;
}

ScenePass::~ScenePass() {
    clean();
}

bool ScenePass::init(ShaderLibrary& library) {
    sprite_program_id_ = "sprite"_hs;
    if (!createFBO(viewport_width_, viewport_height_)) {
        spdlog::error("为 ScenePass 创建 FBO 失败");
        return false;
    }
    // 初始化批处理
    sprite_batch_ = SpriteBatch::create();
    if (!sprite_batch_) {
        spdlog::error("为 ScenePass 创建 SpriteBatch 失败");
        return false;
    }
    // 初始化 sprite 着色器
    ShaderProgram* sprite_program =
        library.loadProgram(sprite_program_id_, shader_assets::SPRITE_VERTEX_PATH, shader_assets::SPRITE_FRAGMENT_PATH);
    if (!sprite_program) {
        spdlog::error("为 ScenePass 加载 sprite 着色器失败");
        return false;
    }
    if (!setupSpriteShaderState(sprite_program)) {
        spdlog::error("ScenePass 初始化 sprite 着色器状态失败");
        return false;
    }
    return logGlErrors("ScenePass::init");
}

bool ScenePass::createFBO(int width, int height) {
    destroyFBO();

    GLuint fbo = 0;
    GLuint color = 0;
    GLColorAttachmentDesc desc{};
    desc.internal_format = GL_RGBA8;
    desc.format = GL_RGBA;
    desc.type = GL_UNSIGNED_BYTE;
    desc.min_filter = GL_NEAREST;
    desc.mag_filter = GL_NEAREST;
    desc.unpack_alignment = 4;
    if (!createFBOWithColorAttachment(width, height, desc, fbo, color)) {
        spdlog::error("场景 FBO 创建失败");
        return false;
    }

    fbo_ = fbo;
    color_tex_ = color;
    return logGlErrors("ScenePass::createFBO");
}

void ScenePass::destroyFBO() {
    if (color_tex_ != 0) {
        glDeleteTextures(1, &color_tex_);
        color_tex_ = 0;
    }
    if (fbo_ != 0) {
        glDeleteFramebuffers(1, &fbo_);
        fbo_ = 0;
    }
}

void ScenePass::clean() {
    destroyFBO();
    if (sprite_batch_) {
        sprite_batch_->clean();
        sprite_batch_.reset();
    }
    sprite_flush_params_ = {};
    sprite_program_id_ = 0;
}

void ScenePass::clear(const glm::vec4& color) {
    if (fbo_ == 0) {
        spdlog::error("ScenePass::clear: 没有 FBO");
        return;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, viewport_width_, viewport_height_);
    glClearColor(color.r, color.g, color.b, color.a);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

bool ScenePass::flush(const utils::Rect& viewport) {
    if (fbo_ == 0) {
        spdlog::error("ScenePass::flush: 没有 FBO");
        return false;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, static_cast<int>(std::round(viewport.size.x)), static_cast<int>(std::round(viewport.size.y)));
    // flush batched sprites
    if (sprite_batch_ && sprite_flush_params_.program_ != 0) {
        return sprite_batch_->flush(sprite_flush_params_);
    }
    spdlog::error("ScenePass::flush: 没有 sprite batch 或 program");
    return false;
}

bool ScenePass::queueSprite(GLuint texture, bool use_texture, const glm::vec4& rect,
                     const glm::vec4& uv_rect,
                     const engine::utils::ColorOptions* color,
                     const engine::utils::TransformOptions* transform,
                     SpriteBatch::TextureMode texture_mode) {
    if (!sprite_batch_) {
        spdlog::error("ScenePass::queueSprite: 没有 sprite batch");
        return false;
    }
    const auto* resolved_color = color ? color : &default_color_options_;
    const auto* resolved_transform = transform ? transform : &default_transform_options_;
    return sprite_batch_->queueSprite(texture, use_texture, rect, uv_rect, resolved_color, resolved_transform, texture_mode);
}

bool ScenePass::reload(ShaderLibrary& library) {
    if (!library.reloadProgram(sprite_program_id_)) {
        spdlog::error("ScenePass::reload: 重新加载着色器失败");
        return false;
    }
    ShaderProgram* sprite_program = library.getProgram(sprite_program_id_);
    if (!sprite_program) {
        spdlog::error("ScenePass::reload: 获取重新加载的 sprite program 失败");
        return false;
    }
    return setupSpriteShaderState(sprite_program);
}

bool ScenePass::setupSpriteShaderState(ShaderProgram* program) {
    if (!program) {
        spdlog::error("ScenePass::setupSpriteShaderState: sprite program 不存在");
        return false;
    }
    program->use();
    GLint u_view_proj = program->uniformLocation("uViewProj");
    GLint u_use_texture = program->uniformLocation("uUseTexture");
    GLint u_texture_mode = program->uniformLocation("uTextureMode");
    GLint u_tex = program->uniformLocation("uTex");
    if (u_tex >= 0) glUniform1i(u_tex, 0);
    program->unuse();

    sprite_flush_params_.program_ = program->program();
    sprite_flush_params_.u_use_texture_ = u_use_texture;
    sprite_flush_params_.u_texture_mode_ = u_texture_mode;
    sprite_flush_params_.u_tex_ = u_tex;
    sprite_flush_params_.u_view_proj_ = u_view_proj;
    return logGlErrors("ScenePass::setupSpriteShaderState");
}

} // namespace engine::render::opengl
