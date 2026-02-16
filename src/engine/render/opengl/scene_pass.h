#pragma once

/**
 * @brief 场景渲染 pass 的轻量级 RAII 封装类。
 *
 * 负责管理场景渲染的 FBO 创建、销毁，以及 sprite 批处理的管理。
 */

#include <glad/glad.h>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <entt/core/fwd.hpp>
#include <functional>
#include <memory>
#include <utility>

#include "engine/utils/math.h"
#include "sprite_batch.h"

namespace engine::render::opengl {

class ShaderLibrary;
class ShaderProgram;

class ScenePass {
    int viewport_width_{0};
    int viewport_height_{0};
    GLuint fbo_{0};
    GLuint color_tex_{0};
    std::unique_ptr<SpriteBatch> sprite_batch_{};
    SpriteBatch::FlushParams sprite_flush_params_{};

    engine::utils::ColorOptions default_color_options_{};
    engine::utils::TransformOptions default_transform_options_{};

    // 精灵着色器配置
    entt::id_type sprite_program_id_{0};

public:
    [[nodiscard]] static std::unique_ptr<ScenePass> create(ShaderLibrary& library, const glm::vec2& viewport_size);
    ~ScenePass();
    ScenePass(const ScenePass&) = delete;
    ScenePass& operator=(const ScenePass&) = delete;
    ScenePass(ScenePass&& other) noexcept = delete;
    ScenePass& operator=(ScenePass&& other) noexcept = delete;

    void clean();
    bool reload(ShaderLibrary& library);

    void clear(const glm::vec4& color);
    bool flush(const utils::Rect& viewport);

    GLuint getFBO() const { return fbo_; }
    GLuint getColorTex() const { return color_tex_; }

    /** 
    * @brief 将精灵入队
    * @param texture 纹理
    * @param use_texture 是否使用纹理
    * @param rect 精灵的渲染矩形（目标）
    * @param uv_rect 精灵的纹理UV矩形（源）
    * @param color 精灵的颜色
    * @return 是否成功入队
    */
    bool queueSprite(GLuint texture, bool use_texture, const glm::vec4& rect,
                     const glm::vec4& uv_rect = {0.0f, 0.0f, 1.0f, 1.0f},
                     const engine::utils::ColorOptions* color = nullptr,
                     const engine::utils::TransformOptions* transform = nullptr);
    uint32_t getLastDrawCallCount() const { return sprite_batch_ ? sprite_batch_->getLastDrawCallCount() : 0; }
    uint32_t getLastVertexCount() const { return sprite_batch_ ? sprite_batch_->getLastVertexCount() : 0; }
    uint32_t getLastIndexCount() const { return sprite_batch_ ? sprite_batch_->getLastIndexCount() : 0; }
    uint32_t getLastSpriteCount() const { return sprite_batch_ ? sprite_batch_->getLastSpriteCount() : 0; }

    void setApplyViewProjection(std::function<void(GLint)> applier) {
        sprite_flush_params_.apply_view_projection_ = std::move(applier);
    }

    void setDefaultColorOptions(const engine::utils::ColorOptions& options) { default_color_options_ = options; }
    void setDefaultTransformOptions(const engine::utils::TransformOptions& options) { default_transform_options_ = options; }
    [[nodiscard]] const engine::utils::ColorOptions& getDefaultColorOptions() const { return default_color_options_; }
    [[nodiscard]] const engine::utils::TransformOptions& getDefaultTransformOptions() const { return default_transform_options_; }

private:
    ScenePass(int viewport_width, int viewport_height)
        : viewport_width_(viewport_width), viewport_height_(viewport_height) {}

    [[nodiscard]] bool init(ShaderLibrary& library);
    [[nodiscard]] bool createFBO(int width, int height);
    void destroyFBO();
    bool setupSpriteShaderState(ShaderProgram* program);
};

} // namespace engine::render::opengl
