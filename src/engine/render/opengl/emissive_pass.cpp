#include "emissive_pass.h"
#include "shader_library.h"
#include "shader_program.h"
#include "shader_asset_paths.h"
#include "gl_helper.h"
#include "sprite_batch.h"
#include <algorithm>
#include <utility>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <entt/core/hashed_string.hpp>

using namespace entt::literals;

namespace {
[[nodiscard]] engine::utils::ColorOptions applyIntensity(
    const engine::utils::ColorOptions& color, float intensity) {
    auto result = color;
    result.start_color.r *= intensity;
    result.start_color.g *= intensity;
    result.start_color.b *= intensity;
    result.start_color.a = std::clamp(result.start_color.a * intensity, 0.0f, 1.0f);
    result.end_color.r *= intensity;
    result.end_color.g *= intensity;
    result.end_color.b *= intensity;
    result.end_color.a = std::clamp(result.end_color.a * intensity, 0.0f, 1.0f);
    return result;
}
} // namespace

namespace engine::render::opengl {

std::unique_ptr<EmissivePass> EmissivePass::create(ShaderLibrary& library, const glm::vec2& viewport_size) {
    auto pass = std::unique_ptr<EmissivePass>(new EmissivePass(static_cast<int>(std::round(viewport_size.x)),
                                                               static_cast<int>(std::round(viewport_size.y))));
    if (!pass->init(library)) {
        spdlog::error("EmissivePass 初始化失败");
        return nullptr;
    }
    return pass;
}

EmissivePass::~EmissivePass() {
    clean();
}

bool EmissivePass::init(ShaderLibrary& library) {
    program_id_ = "emissive"_hs;
    ShaderProgram* program =
        library.loadProgram(program_id_, shader_assets::EMISSIVE_VERTEX_PATH, shader_assets::EMISSIVE_FRAGMENT_PATH);
    if (!program) {
        spdlog::error("EmissivePass 加载着色器失败");
        return false;
    }
    if (!setupShaderState(program)) {
        spdlog::error("EmissivePass 初始化着色器状态失败");
        return false;
    }
    if (!createFBO(viewport_width_, viewport_height_)) {
        spdlog::error("EmissivePass 创建 FBO 失败");
        return false;
    }
    // 创建批处理
    sprite_batch_ = SpriteBatch::create();
    if (!sprite_batch_) {
        spdlog::error("EmissivePass 创建 SpriteBatch 失败");
        return false;
    }
    return logGlErrors("EmissivePass::init");
}

void EmissivePass::clear() {
    if (fbo_ == 0) {
        spdlog::error("EmissivePass::clear: 没有 FBO");
        return;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void EmissivePass::clean() {
    destroyFBO();
    if (sprite_batch_) {
        sprite_batch_->clean();
        sprite_batch_.reset();
    }
    flush_params_ = {};
    program_id_ = 0;
    active_ = false;
}

bool EmissivePass::createFBO(int width, int height) {
    destroyFBO();

    // 发光缓冲在进行泛光(Bloom)处理前用于存储辉光遮罩及透明度遮罩，使用高精度 RGBA16F 以避免叠加时的色彩丢失。
    const ScopedGLUnpackAlignment scoped_unpack_alignment(4);
    GLuint fbo = 0;
    GLuint color = 0;
    glGenFramebuffers(1, &fbo);
    glGenTextures(1, &color);
    glBindTexture(GL_TEXTURE_2D, color);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        glDeleteTextures(1, &color);
        glDeleteFramebuffers(1, &fbo);
        spdlog::error("Emissive FBO incomplete");
        return false;
    }
    fbo_ = fbo;
    color_tex_ = color;
    return logGlErrors("EmissivePass::createEmissiveTarget");
}

void EmissivePass::destroyFBO() {
    if (color_tex_ != 0) {
        glDeleteTextures(1, &color_tex_);
        color_tex_ = 0;
    }
    if (fbo_ != 0) {
        glDeleteFramebuffers(1, &fbo_);
        fbo_ = 0;
    }
}

void EmissivePass::setApplyViewProjection(std::function<void(GLint)> applier) {
    flush_params_.apply_view_projection_ = std::move(applier);
}

bool EmissivePass::flush(const utils::Rect& viewport) {
    if (flush_params_.program_ == 0 || fbo_ == 0) {
        spdlog::error("EmissivePass::flush: 没有 FBO 或 program");
        return false;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, static_cast<int>(std::round(viewport.size.x)), static_cast<int>(std::round(viewport.size.y)));
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    // flush batched emissive sprites
    bool ok = true;
    if (sprite_batch_ && flush_params_.program_ != 0) {
        // Always call flush so SpriteBatch can reset its "last frame" stats to 0 when empty.
        ok = sprite_batch_->flush(flush_params_);
    }
    glBindVertexArray(0);
    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    return ok && logGlErrors("EmissivePass::flush");
}

void EmissivePass::addRect(const glm::vec4& rect,
                           float intensity,
                           const engine::utils::ColorOptions* color,
                           const engine::utils::TransformOptions* transform) {
    if (!sprite_batch_) {
        return;
    }
    const auto* resolved_color = color ? color : &default_color_options_;
    const auto* resolved_transform = transform ? transform : &default_transform_options_;
    auto applied_color = applyIntensity(*resolved_color, intensity);

    sprite_batch_->queueSprite(0, false, rect, {0.0f, 0.0f, 1.0f, 1.0f},
                               &applied_color, resolved_transform);
}

void EmissivePass::addTexture(GLuint texture, const glm::vec4& dst_rect, const glm::vec4& uv_rect,
                              float intensity,
                              const engine::utils::ColorOptions* color,
                              const engine::utils::TransformOptions* transform) {
    if (!sprite_batch_ || texture == 0) {
        return;
    }
    const auto* resolved_color = color ? color : &default_color_options_;
    const auto* resolved_transform = transform ? transform : &default_transform_options_;
    auto applied_color = applyIntensity(*resolved_color, intensity);

    sprite_batch_->queueSprite(texture, true, dst_rect, uv_rect,
                               &applied_color, resolved_transform);
}

bool EmissivePass::reload(ShaderLibrary& library) {
    if (!library.reloadProgram(program_id_)) {
        spdlog::error("EmissivePass::reload: 重新加载着色器失败");
        return false;
    }
    ShaderProgram* program = library.getProgram(program_id_);
    if (!program) {
        spdlog::error("EmissivePass::reload: 获取重新加载的 program 失败");
        return false;
    }
    return setupShaderState(program);
}

bool EmissivePass::setupShaderState(ShaderProgram* program) {
    if (!program) {
        spdlog::error("EmissivePass::setupShaderState: program 不存在");
        return false;
    }
    program->use();
    GLint u_tex = program->uniformLocation("uTex");
    GLint u_use_texture = program->uniformLocation("uUseTexture");
    GLint u_color = program->uniformLocation("uColor");
    GLint u_view_proj = program->uniformLocation("uViewProj");
    if (u_tex >= 0) {
        glUniform1i(u_tex, 0);
    }
    if (u_color >= 0) {
        glUniform3f(u_color, 1.0f, 1.0f, 1.0f);
    }
    glUseProgram(0);

    flush_params_.program_ = program->program();
    flush_params_.u_use_texture_ = u_use_texture;
    flush_params_.u_tex_ = u_tex;
    flush_params_.u_view_proj_ = u_view_proj;
    return logGlErrors("EmissivePass::setupShaderState");
}

} // namespace engine::render::opengl
