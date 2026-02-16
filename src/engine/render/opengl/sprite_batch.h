#pragma once

/**
 * @file sprite_batch.h
 * @brief CPU 端精灵批处理工具类
 *
 * 该类用于在 CPU 侧批量收集四边形（精灵）信息，并一次性批量提交到 GPU。每个入队的精灵存储其几何数据、颜色信息和纹理使用状态；
 * 调用 flush() 时，将所有已缓存的数据上传，并按纹理分组进行绘制，从而减少 OpenGL 的绘制调用次数与状态切换。
 * 注意：由于渲染顺序（draw order）语义不可随意改变，这里的“按纹理分组”仅能合并**提交顺序中连续**使用同一纹理的精灵；
 * 不能为了减少 draw call 而跨越其他纹理的精灵去重排队列。
 * 该机制将每精灵的详细处理与记录工作从 GLRenderer 中解耦，使渲染器更加简洁高效。
 */

#include <glad/glad.h>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

#include "engine/utils/defs.h"

namespace engine::render::opengl {

class SpriteBatch final {
    constexpr static size_t MIN_SPRITE_CAPACITY = 64;
    /* @brief 每个顶点的数据布局 */
    struct Vertex {
        glm::vec2 pos_{0.0f};
        glm::vec2 uv_{0.0f};
        glm::vec4 color_{0.0f};
    };
    /* @brief 绘制命令，按纹理分组 */
    struct Command {
        GLuint texture_{0};
        uint32_t index_offset_{0};
        uint32_t index_count_{0};
        bool use_texture_{false};
    };

    GLuint vao_{0};
    GLuint vbo_{0};
    GLuint ebo_{0};
    size_t capacity_{0};
    size_t vertex_buffer_capacity_bytes_{0};
    size_t index_buffer_capacity_bytes_{0};

    std::vector<Vertex> vertices_;
    std::vector<uint32_t> indices_;
    std::vector<Command> commands_;
public:
    [[nodiscard]] static std::unique_ptr<SpriteBatch> create(size_t initial_capacity = MIN_SPRITE_CAPACITY);
    ~SpriteBatch();

    SpriteBatch(const SpriteBatch&) = delete;
    SpriteBatch& operator=(const SpriteBatch&) = delete;
    SpriteBatch(SpriteBatch&&) = delete;
    SpriteBatch& operator=(SpriteBatch&&) = delete;

    /* @brief 批处理参数 */
    struct FlushParams {
        GLuint program_{0};
        GLint u_use_texture_{-1};
        GLint u_tex_{-1};
        GLint u_view_proj_{-1};
        std::function<void(GLint)> apply_view_projection_{};
    };

    void clean();

    /* @brief 待绘制精灵入队 */
    bool queueSprite(GLuint texture, bool use_texture, const glm::vec4& rect,
                     const glm::vec4& uv_rect = {0.0f, 0.0f, 1.0f, 1.0f},
                     const engine::utils::ColorOptions* color = nullptr,
                     const engine::utils::TransformOptions* transform = nullptr);

    /* @brief 提交批处理，按纹理分组绘制 */
    bool flush(const FlushParams& params);
    void reset();
    bool empty() const { return indices_.empty(); }
    uint32_t getLastDrawCallCount() const { return last_draw_calls_; }
    uint32_t getLastVertexCount() const { return last_vertex_count_; }
    uint32_t getLastIndexCount() const { return last_index_count_; }
    uint32_t getLastSpriteCount() const { return last_sprite_count_; }

private:
    SpriteBatch() = default;

    [[nodiscard]] bool init(size_t initial_capacity = MIN_SPRITE_CAPACITY);

    /* @brief 容量不足时翻倍扩容 */
    bool ensureCapacity(size_t required_sprites);
    void destroyBuffers();
    bool ensureDefaultTexture();
    void destroyDefaultTexture();

    GLuint default_texture_{0};
    uint32_t last_draw_calls_{0};
    uint32_t last_vertex_count_{0};
    uint32_t last_index_count_{0};
    uint32_t last_sprite_count_{0};
};

} // namespace engine::render::opengl
