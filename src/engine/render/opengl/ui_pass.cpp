#include "ui_pass.h"
#include "sprite_batch.h"
#include "shader_asset_paths.h"
#include "shader_library.h"
#include "shader_program.h"
#include "gl_helper.h"
#include <entt/core/hashed_string.hpp>
#include <spdlog/spdlog.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace entt::literals;

namespace engine::render::opengl {

std::unique_ptr<UIPass> UIPass::create(ShaderLibrary& library, const glm::vec2& viewport_size) {
    auto pass = std::unique_ptr<UIPass>(new UIPass(static_cast<int>(std::round(viewport_size.x)),
                                                   static_cast<int>(std::round(viewport_size.y))));
    if (!pass->init(library)) {
        spdlog::error("初始化 UIPass 失败");
        return nullptr;
    }
    return pass;
}

UIPass::~UIPass() {
    clean();
}

bool UIPass::init(ShaderLibrary& library) {
    sprite_program_id_ = "sprite"_hs;
    // shader
    ShaderProgram* sprite_program =
        library.loadProgram(sprite_program_id_, shader_assets::SPRITE_VERTEX_PATH, shader_assets::SPRITE_FRAGMENT_PATH);
    if (!sprite_program) {
        spdlog::error("为 UIPass 加载 sprite 着色器失败");
        return false;
    }
    if (!setupShaderState(sprite_program)) {
        spdlog::error("UIPass 初始化着色器状态失败");
        return false;
    }

    // batch
    sprite_batch_ = SpriteBatch::create();
    if (!sprite_batch_) {
        spdlog::error("为 UIPass 创建 SpriteBatch 失败");
        return false;
    }

    return true;
}

void UIPass::clean() {
    if (sprite_batch_) {
        sprite_batch_->clean();
        sprite_batch_.reset();
    }
    flush_params_ = {};
    ui_view_proj_ = glm::mat4(1.0f);
    sprite_program_id_ = 0;
}

bool UIPass::flush(const utils::Rect& viewport) {
    // UI 直接绘制到默认帧缓冲
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(static_cast<int>(std::round(viewport.pos.x)),
               static_cast<int>(std::round(viewport.pos.y)),
               static_cast<int>(std::round(viewport.size.x)),
               static_cast<int>(std::round(viewport.size.y)));

    if (flush_params_.program_ != 0 && flush_params_.u_view_proj_ >= 0) {
        // UI 坐标系以逻辑画面左上角为原点，Y 轴向下。
        const float width = static_cast<float>(viewport_width_);
        const float height = static_cast<float>(viewport_height_);
        ui_view_proj_ = glm::ortho(0.0f, width,
                                   height, 0.0f,
                                   -1.0f, 1.0f);
    } else {
        ui_view_proj_ = glm::mat4(1.0f);
    }

    if (sprite_batch_ && flush_params_.program_ != 0) {
        return sprite_batch_->flush(flush_params_);
    }
    spdlog::error("UIPass::flush: 没有 sprite batch 或 program");
    return false;
}

bool UIPass::queueSprite(GLuint texture, bool use_texture, const glm::vec4& rect,
                         const glm::vec4& uv_rect,
                         const engine::utils::ColorOptions* color,
                         const engine::utils::TransformOptions* transform) {
    if (!sprite_batch_) {
        spdlog::error("UIPass::queueSprite: 没有 sprite batch");
        return false;
    }
    GLuint tex = use_texture ? texture : 0;
    const auto* resolved_color = color ? color : &default_color_options_;
    const auto* resolved_transform = transform ? transform : &default_transform_options_;
    return sprite_batch_->queueSprite(tex, use_texture, rect, uv_rect,
                                      resolved_color, resolved_transform);
}

bool UIPass::reload(ShaderLibrary& library) {
    if (!library.reloadProgram(sprite_program_id_)) {
        spdlog::error("UIPass::reload: 重新加载着色器失败");
        return false;
    }
    ShaderProgram* sprite_program = library.getProgram(sprite_program_id_);
    if (!sprite_program) {
        spdlog::error("UIPass::reload: 获取重新加载的 sprite program 失败");
        return false;
    }
    return setupShaderState(sprite_program);
}

bool UIPass::setupShaderState(ShaderProgram* program) {
    if (!program) {
        spdlog::error("UIPass::setupShaderState: sprite program 不存在");
        return false;
    }
    program->use();
    GLint u_view_proj = program->uniformLocation("uViewProj");
    GLint u_use_texture = program->uniformLocation("uUseTexture");
    GLint u_tex = program->uniformLocation("uTex");
    if (u_tex >= 0) glUniform1i(u_tex, 0);
    program->unuse();

    flush_params_.program_ = program->program();
    flush_params_.u_use_texture_ = u_use_texture;
    flush_params_.u_tex_ = u_tex;
    flush_params_.u_view_proj_ = u_view_proj;
    flush_params_.apply_view_projection_ = [this](GLint location) { applyViewProjection(location); };
    return logGlErrors("UIPass::setupShaderState");
}

void UIPass::applyViewProjection(GLint location) const {
    if (location >= 0) {
        glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(ui_view_proj_));
    }
}

} // namespace engine::render::opengl
