#pragma once

/**
 * @brief 管理自发光通道专用的着色器和OpenGL缓冲区的对象。
 *
 * EmissivePass 拥有用于渲染自发光效果的着色器和GL缓冲区。
 * 从而使本通道负责所有着色器 uniform 设定、加法混合与视口管理等细节。
 */
#include <glad/glad.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <entt/core/fwd.hpp>
#include <functional>
#include <memory>
#include "engine/utils/math.h"
#include "sprite_batch.h"

namespace engine::render::opengl {

class ShaderLibrary;
class ShaderProgram;

class EmissivePass {
    int viewport_width_{0};
    int viewport_height_{0};
    entt::id_type program_id_{0};
    GLuint fbo_{0};
    GLuint color_tex_{0};
    bool active_{true};

    std::unique_ptr<SpriteBatch> sprite_batch_{};
    SpriteBatch::FlushParams flush_params_{};

    engine::utils::ColorOptions default_color_options_{};
    engine::utils::TransformOptions default_transform_options_{};

public:
    [[nodiscard]] static std::unique_ptr<EmissivePass> create(ShaderLibrary& library, const glm::vec2& viewport_size);
    ~EmissivePass();
    EmissivePass(const EmissivePass&) = delete;
    EmissivePass& operator=(const EmissivePass&) = delete;
    EmissivePass(EmissivePass&&) = delete;
    EmissivePass& operator=(EmissivePass&&) = delete;

    bool reload(ShaderLibrary& library);
    void clear();
    void clean();
    bool flush(const utils::Rect& viewport);
    void setApplyViewProjection(std::function<void(GLint)> applier);
    uint32_t getLastDrawCallCount() const { return sprite_batch_ ? sprite_batch_->getLastDrawCallCount() : 0; }
    uint32_t getLastVertexCount() const { return sprite_batch_ ? sprite_batch_->getLastVertexCount() : 0; }
    uint32_t getLastIndexCount() const { return sprite_batch_ ? sprite_batch_->getLastIndexCount() : 0; }
    uint32_t getLastSpriteCount() const { return sprite_batch_ ? sprite_batch_->getLastSpriteCount() : 0; }
    
    void addRect(const glm::vec4& rect,
                 float intensity,
                 const engine::utils::ColorOptions* color = nullptr,
                 const engine::utils::TransformOptions* transform = nullptr);
    void addTexture(GLuint texture, const glm::vec4& dst_rect, const glm::vec4& uv_rect,
                    float intensity,
                    const engine::utils::ColorOptions* color = nullptr,
                    const engine::utils::TransformOptions* transform = nullptr);

    bool isActive() const { return active_; }
    GLuint getFBO() const { return fbo_; }
    GLuint getColorTex() const { return color_tex_; }

    void setDefaultColorOptions(const engine::utils::ColorOptions& options) { default_color_options_ = options; }
    void setDefaultTransformOptions(const engine::utils::TransformOptions& options) { default_transform_options_ = options; }
    [[nodiscard]] const engine::utils::ColorOptions& getDefaultColorOptions() const { return default_color_options_; }
    [[nodiscard]] const engine::utils::TransformOptions& getDefaultTransformOptions() const { return default_transform_options_; }

private:
    EmissivePass(int viewport_width, int viewport_height) : viewport_width_(viewport_width), viewport_height_(viewport_height) {}

    [[nodiscard]] bool init(ShaderLibrary& library);
    [[nodiscard]] bool createFBO(int width, int height);
    void destroyFBO();
    bool setupShaderState(ShaderProgram* program);
};

}
