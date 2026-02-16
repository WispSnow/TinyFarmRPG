#pragma once

#include <glad/glad.h>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <entt/core/fwd.hpp>
#include <memory>

#include "engine/utils/math.h"
#include "sprite_batch.h"

namespace engine::render::opengl {

class ShaderLibrary;
class ShaderProgram;

class UIPass {
    int viewport_width_{0};
    int viewport_height_{0};

    std::unique_ptr<SpriteBatch> sprite_batch_{};
    entt::id_type sprite_program_id_{0};
    SpriteBatch::FlushParams flush_params_{};
    glm::mat4 ui_view_proj_{1.0f};

    engine::utils::ColorOptions default_color_options_{};
    engine::utils::TransformOptions default_transform_options_{};

public:
    [[nodiscard]] static std::unique_ptr<UIPass> create(ShaderLibrary& library, const glm::vec2& viewport_size);
    ~UIPass();

    UIPass(const UIPass&) = delete;
    UIPass& operator=(const UIPass&) = delete;

    bool reload(ShaderLibrary& library);
    void clean();

    bool flush(const utils::Rect& viewport);

    bool queueSprite(GLuint texture, bool use_texture, const glm::vec4& rect,
                     const glm::vec4& uv_rect = {0.0f, 0.0f, 1.0f, 1.0f},
                     const engine::utils::ColorOptions* color = nullptr,
                     const engine::utils::TransformOptions* transform = nullptr);
    uint32_t getLastDrawCallCount() const { return sprite_batch_ ? sprite_batch_->getLastDrawCallCount() : 0; }
    uint32_t getLastVertexCount() const { return sprite_batch_ ? sprite_batch_->getLastVertexCount() : 0; }
    uint32_t getLastIndexCount() const { return sprite_batch_ ? sprite_batch_->getLastIndexCount() : 0; }
    uint32_t getLastSpriteCount() const { return sprite_batch_ ? sprite_batch_->getLastSpriteCount() : 0; }

    void setDefaultColorOptions(const engine::utils::ColorOptions& options) { default_color_options_ = options; }
    void setDefaultTransformOptions(const engine::utils::TransformOptions& options) { default_transform_options_ = options; }
    [[nodiscard]] const engine::utils::ColorOptions& getDefaultColorOptions() const { return default_color_options_; }
    [[nodiscard]] const engine::utils::TransformOptions& getDefaultTransformOptions() const { return default_transform_options_; }

private:
    UIPass(int viewport_width, int viewport_height)
        : viewport_width_(viewport_width), viewport_height_(viewport_height) {}

    [[nodiscard]] bool init(ShaderLibrary& library);
    void applyViewProjection(GLint location) const;
    bool setupShaderState(ShaderProgram* program);
};

}
