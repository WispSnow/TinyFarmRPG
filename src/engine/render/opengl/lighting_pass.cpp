// -----------------------------------------------------------------------------
// lighting_pass.cpp
// -----------------------------------------------------------------------------
// 实现由 GLRenderer 使用的光照累积通道。
// 本类负责维护实现点光源、聚光灯体积绘制及屏幕空间方向渐变（用于昼夜系统）的着色器、四边形几何及混合状态。
// -----------------------------------------------------------------------------
#include "gl_helper.h"
#include "lighting_pass.h"
#include "shader_asset_paths.h"
#include "shader_library.h"
#include "shader_program.h"
#include <utility>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <entt/core/hashed_string.hpp>

using namespace entt::literals;

namespace engine::render::opengl {

std::unique_ptr<LightingPass> LightingPass::create(ShaderLibrary& library, const glm::vec2& viewport_size) {
    auto pass = std::unique_ptr<LightingPass>(new LightingPass(static_cast<int>(std::round(viewport_size.x)),
                                                               static_cast<int>(std::round(viewport_size.y))));
    if (!pass->init(library)) {
        spdlog::error("Failed to initialize lighting pass");
        return nullptr;
    }
    return pass;
}

LightingPass::~LightingPass() {
    clean();
}

bool LightingPass::init(ShaderLibrary& library) {
    if (!program_) {
        program_ = library.loadProgram("light"_hs, shader_assets::LIGHT_VERTEX_PATH, shader_assets::LIGHT_FRAGMENT_PATH);
        if (!program_) {
            spdlog::error("Failed to load lighting program");
            return false;
        }
        program_->use();
        u_view_proj_ = program_->uniformLocation("uViewProj");
        u_light_color_ = program_->uniformLocation("uLightColor");
        u_light_intensity_ = program_->uniformLocation("uLightIntensity");
        u_light_type_ = program_->uniformLocation("uLightType");
        u_spot_dir_ = program_->uniformLocation("uSpotDir");
        u_spot_inner_cos_ = program_->uniformLocation("uSpotInnerCos");
        u_spot_outer_cos_ = program_->uniformLocation("uSpotOuterCos");
        u_midday_blend_ = program_->uniformLocation("uMiddayBlend");
        u_dir_2d_ = program_->uniformLocation("uDir2D");
        u_dir_offset_ = program_->uniformLocation("uDirOffset");
        u_dir_softness_ = program_->uniformLocation("uDirSoftness");
        glUseProgram(0);
    }

    if (!createBuffers()) {
        spdlog::error("Failed to create buffers");
        return false;
    }
    if (!createFBO(viewport_width_, viewport_height_)) {
        spdlog::error("Failed to create light target");
        return false;
    }
    return true;
}

bool LightingPass::flush(const utils::Rect& viewport,
                         const std::function<void(GLint)>& apply_view_projection) {
    if (!program_ || vao_ == 0 || vbo_ == 0 || fbo_ == 0) {
        last_draw_calls_ = 0;
        last_vertex_count_ = 0;
        return false;
    }
    apply_view_projection_ = apply_view_projection;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, static_cast<int>(std::round(viewport.size.x)), static_cast<int>(std::round(viewport.size.y)));
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    program_->use();
    if (apply_view_projection_) {
        apply_view_projection_(u_view_proj_);
    }
    if (u_light_type_ >= 0) {
        glUniform1i(u_light_type_, 0);
    }
    glBlendFunc(GL_ONE, GL_ONE);

    // 执行批处理渲染 (2D光照通常不会同屏太多，目前实现基本够用。未来可考虑使用实例化的批处理方式)
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    uint32_t draw_calls = 0;
    uint32_t vertex_count = 0;
    for (const LightCommand& cmd : commands_) {
        if (u_light_color_ >= 0) glUniform3fv(u_light_color_, 1, glm::value_ptr(cmd.color));
        if (u_light_intensity_ >= 0) glUniform1f(u_light_intensity_, cmd.intensity);
        if (u_light_type_ >= 0) glUniform1i(u_light_type_, static_cast<int>(cmd.type));

        if (cmd.type == LightType::Point || cmd.type == LightType::Spot) {
            const float left = cmd.pos.x - cmd.radius;
            const float top = cmd.pos.y - cmd.radius;
            const float right = cmd.pos.x + cmd.radius;
            const float bottom = cmd.pos.y + cmd.radius;
            const float verts[] = {
                left,  top,     0.f, 0.f,
                right, top,     1.f, 0.f,
                right, bottom,  1.f, 1.f,
                left,  top,     0.f, 0.f,
                right, bottom,  1.f, 1.f,
                left,  bottom,  0.f, 1.f,
            };
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
            if (cmd.type == LightType::Spot) {
                if (u_spot_dir_ >= 0) glUniform2fv(u_spot_dir_, 1, glm::value_ptr(cmd.spot_dir));
                if (u_spot_inner_cos_ >= 0) glUniform1f(u_spot_inner_cos_, cmd.spot_inner_cos);
                if (u_spot_outer_cos_ >= 0) glUniform1f(u_spot_outer_cos_, cmd.spot_outer_cos);
            }
            glDrawArrays(GL_TRIANGLES, 0, 6);
            ++draw_calls;
            vertex_count += 6;
        } else if (cmd.type == LightType::Directional) {
            const float left = 0.0f;
            const float top = 0.0f;
            const float right = static_cast<float>(viewport_width_);
            const float bottom = static_cast<float>(viewport_height_);
            const float verts[] = {
                left,  top,     0.f, 0.f,
                right, top,     1.f, 0.f,
                right, bottom,  1.f, 1.f,
                left,  top,     0.f, 0.f,
                right, bottom,  1.f, 1.f,
                left,  bottom,  0.f, 1.f,
            };
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
            if (u_dir_2d_ >= 0) {
                glm::vec2 nd = glm::length(cmd.dir2d) > 1e-5f
                                   ? glm::normalize(glm::vec2(cmd.dir2d.x * bottom, cmd.dir2d.y * right))
                                   : glm::vec2(0.0f, -1.0f);
                glUniform2fv(u_dir_2d_, 1, glm::value_ptr(nd));
            }
            if (u_dir_offset_ >= 0) glUniform1f(u_dir_offset_, cmd.dir_offset);
            if (u_dir_softness_ >= 0) {
                float soft_uv = cmd.dir_softness * cmd.zoom;
                soft_uv = glm::clamp(soft_uv, 1e-4f, 0.49f);
                glUniform1f(u_dir_softness_, soft_uv);
            }
            if (u_midday_blend_ >= 0) glUniform1f(u_midday_blend_, glm::clamp(cmd.midday_blend, 0.0f, 1.0f));

            // 使用屏幕空间投影
            if (apply_view_projection_) {
                glm::mat4 screenProj = glm::ortho(0.0f, right, bottom, 0.0f, -1.0f, 1.0f);
                if (u_view_proj_ >= 0) glUniformMatrix4fv(u_view_proj_, 1, GL_FALSE, glm::value_ptr(screenProj));
            }
            glDrawArrays(GL_TRIANGLES, 0, 6);
            ++draw_calls;
            vertex_count += 6;
            // 恢复相机视图投影
            if (apply_view_projection_) {
                apply_view_projection_(u_view_proj_);
            }
        }
    }
    last_draw_calls_ = draw_calls;
    last_vertex_count_ = vertex_count;
    commands_.clear();

    glBindVertexArray(0);
    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    apply_view_projection_ = nullptr;
    return true;
}

void LightingPass::addPointLight(const glm::vec2& pos, float radius,
                                 const engine::utils::PointLightOptions& options) {
    LightCommand cmd;
    cmd.type = LightType::Point;
    cmd.pos = pos;
    cmd.radius = radius;
    cmd.color = {options.color.r, options.color.g, options.color.b};
    cmd.intensity = options.intensity;
    commands_.push_back(cmd);
}

void LightingPass::addSpotLight(const glm::vec2& pos, float radius, const glm::vec2& dir,
                                const engine::utils::SpotLightOptions& options) {
    glm::vec2 ndir = glm::length(dir) > 1e-5f
                         ? glm::normalize(dir)
                         : glm::vec2(0.0f, -1.0f);
    float inner_cos = glm::cos(glm::radians(options.inner_angle_deg));
    float outer_cos = glm::cos(glm::radians(options.outer_angle_deg));
    if (inner_cos < outer_cos) std::swap(inner_cos, outer_cos);
    LightCommand cmd;
    cmd.type = LightType::Spot;
    cmd.pos = pos;
    cmd.radius = radius;
    cmd.color = {options.color.r, options.color.g, options.color.b};
    cmd.intensity = options.intensity;
    cmd.spot_dir = ndir;
    cmd.spot_inner_cos = inner_cos;
    cmd.spot_outer_cos = outer_cos;
    commands_.push_back(cmd);
}

void LightingPass::addDirectionalLight(const glm::vec2& dir,
                                       const engine::utils::DirectionalLightOptions& options) {
    LightCommand cmd;
    cmd.type = LightType::Directional;
    cmd.color = {options.color.r, options.color.g, options.color.b};
    cmd.intensity = options.intensity;
    cmd.dir2d = dir;
    cmd.dir_offset = options.offset;
    cmd.dir_softness = options.softness;
    cmd.midday_blend = options.midday_blend;
    cmd.zoom = options.zoom;
    commands_.push_back(cmd);
}

void LightingPass::clear() {
    if (fbo_ != 0) {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }
}

void LightingPass::clean() {
    destroyFBO();
    if (vbo_ != 0) {
        glDeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
    if (vao_ != 0) {
        glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }
    program_ = nullptr;
    active_ = false;
    apply_view_projection_ = nullptr;
}

bool LightingPass::createFBO(int width, int height) {
    destroyFBO();

    // 光照缓冲区使用 RGB 格式，因为光照强度以累加方式叠加，不需要 alpha 通道。
    // 保持更小的格式有助于在对 bloom 进行降采样时减少带宽消耗。
    const ScopedGLUnpackAlignment scoped_unpack_alignment(1);
    GLuint fbo = 0;
    GLuint color = 0;
    glGenFramebuffers(1, &fbo);
    glGenTextures(1, &color);
    glBindTexture(GL_TEXTURE_2D, color);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        glDeleteTextures(1, &color);
        glDeleteFramebuffers(1, &fbo);
        spdlog::error("Light FBO incomplete");
        return false;
    }

    fbo_ = fbo;
    color_tex_ = color;
    return logGlErrors("LightingPass::createLightTarget");
}

void LightingPass::destroyFBO() {
    if (color_tex_ != 0) {
        glDeleteTextures(1, &color_tex_);
        color_tex_ = 0;
    }
    if (fbo_ != 0) {
        glDeleteFramebuffers(1, &fbo_);
        fbo_ = 0;
    }
}

// --- private methods ---

bool LightingPass::createBuffers() {
    if (vao_ != 0 && vbo_ != 0) {
        return true;
    }

    // 全屏四边形几何体被所有类型的光源共享；通过 uniform 控制着色器行为，实现径向衰减（点光源）、锥形（聚光灯）或方向渐变（方向光）。
    // 仅需分配一次，直至 destroy() 时释放资源重复利用。
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, 6 * 4 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return logGlErrors("LightingPass::createBuffers");
}

} // namespace engine::render::opengl
