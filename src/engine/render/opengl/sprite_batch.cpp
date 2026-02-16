// -----------------------------------------------------------------------------
// sprite_batch.cpp
// -----------------------------------------------------------------------------
// 实现了 GLRenderer 使用的 CPU 端批处理层。将精灵队列到 CPU 向量中，批量刷新以最小化绘制调用次数与状态切换。
// -----------------------------------------------------------------------------
#include "sprite_batch.h"
#include "gl_helper.h"
#include <spdlog/spdlog.h>
#include <glm/gtc/epsilon.hpp>
#include <glm/geometric.hpp>
#include <glm/common.hpp>
#include <cmath>
#include <limits>

namespace engine::render::opengl {

std::unique_ptr<SpriteBatch> SpriteBatch::create(size_t initial_capacity) {
    auto sprite_batch = std::unique_ptr<SpriteBatch>(new SpriteBatch());
    if (!sprite_batch->init(initial_capacity)) {
        return nullptr;
    }
    return sprite_batch;
}

SpriteBatch::~SpriteBatch() {
    clean();
}

bool SpriteBatch::init(size_t initial_capacity) {
    clean();

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);
    if (vao_ == 0 || vbo_ == 0 || ebo_ == 0) {
        destroyBuffers();
        spdlog::error("创建 SpriteBatch 失败。");
        clean();
        return false;
    }

    // 设置顶点属性指针，一共三个属性：位置、纹理坐标、颜色
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, pos_)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, uv_)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, color_)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    // 确保容量足够
    if (!ensureCapacity(initial_capacity)) {
        clean();
        return false;
    }
    reset();
    if (!ensureDefaultTexture()) {
        clean();
        return false;
    }

    return logGlErrors("SpriteBatch::init (vertex setup)");
}

void SpriteBatch::clean() {
    destroyDefaultTexture();
    destroyBuffers();
    capacity_ = 0;
    vertex_buffer_capacity_bytes_ = 0;
    index_buffer_capacity_bytes_ = 0;
    vertices_.clear();
    indices_.clear();
    commands_.clear();
}

bool SpriteBatch::queueSprite(GLuint texture, bool use_texture, const glm::vec4& rect,
                              const glm::vec4& uv_rect,
                              const engine::utils::ColorOptions* color_options,
                              const engine::utils::TransformOptions* transform_options) {
    if (rect.z <= 0.0f || rect.w <= 0.0f) {
        return true;
    }

    // 确保容量足够
    size_t current_sprites = vertices_.size() / 4;
    if (current_sprites + 1 > capacity_) {
        if (!ensureCapacity(current_sprites + 1)) {
            spdlog::error("SpriteBatch::queueSprite: ensureCapacity failed");
            return false;
        }
    }

    // 创建四个顶点
    Vertex verts[4];
    const engine::utils::TransformOptions default_transform{};
    const auto* transform = transform_options ? transform_options : &default_transform;
    const bool has_rotation = !glm::epsilonEqual(transform->rotation_radians, 0.0f, 1e-5f);
    if (!has_rotation) {
        verts[0].pos_ = {rect.x, rect.y};
        verts[1].pos_ = {rect.x + rect.z, rect.y};
        verts[2].pos_ = {rect.x + rect.z, rect.y + rect.w};
        verts[3].pos_ = {rect.x, rect.y + rect.w};
    } else {
        const glm::vec2 size{rect.z, rect.w};
        const glm::vec2 pivot_offset = size * transform->pivot;
        const glm::vec2 pivot_world = glm::vec2{rect.x, rect.y} + pivot_offset;
        const glm::vec2 local_corners[4] = {
            {-pivot_offset.x,        -pivot_offset.y},
            {size.x - pivot_offset.x, -pivot_offset.y},
            {size.x - pivot_offset.x, size.y - pivot_offset.y},
            {-pivot_offset.x,         size.y - pivot_offset.y}
        };
        const float s = std::sin(transform->rotation_radians);
        const float c = std::cos(transform->rotation_radians);
        for (int i = 0; i < 4; ++i) {
            const glm::vec2 rotated{
                local_corners[i].x * c - local_corners[i].y * s,
                local_corners[i].x * s + local_corners[i].y * c
            };
            verts[i].pos_ = pivot_world + rotated;
        }
    }

    const float u0 = uv_rect.x;
    const float v0 = uv_rect.y;
    const float u1 = uv_rect.z;
    const float v1 = uv_rect.w;

    const float u_left = transform->flip_horizontal ? u1 : u0;
    const float u_right = transform->flip_horizontal ? u0 : u1;

    verts[0].uv_ = {u_left, v0};
    verts[1].uv_ = {u_right, v0};
    verts[2].uv_ = {u_right, v1};
    verts[3].uv_ = {u_left, v1};

    const engine::utils::ColorOptions default_color{};
    const auto* color = color_options ? color_options : &default_color;

    const bool has_gradient = color->use_gradient;
    glm::vec2 direction{0.0f, 1.0f};
    float range_start = 0.0f;
    float range_end = 1.0f;
    glm::vec4 start_color_vec{
        color->start_color.r, color->start_color.g, color->start_color.b, color->start_color.a};
    glm::vec4 end_color_vec{
        color->end_color.r, color->end_color.g, color->end_color.b, color->end_color.a};
    if (has_gradient) {
        const float angle = color->angle_radians;
        direction = {std::cos(angle), std::sin(angle)};
        const float len_sq = direction.x * direction.x + direction.y * direction.y;
        if (len_sq <= std::numeric_limits<float>::epsilon()) {
            direction = {0.0f, 1.0f};
        }
        float min_projection = std::numeric_limits<float>::max();
        float max_projection = std::numeric_limits<float>::lowest();
        for (int i = 0; i < 4; ++i) {
            const float projection = glm::dot(verts[i].pos_, direction);
            min_projection = std::min(min_projection, projection);
            max_projection = std::max(max_projection, projection);
        }
        range_start = min_projection;
        range_end = max_projection;
        if (glm::epsilonEqual(range_start, range_end, 1e-5f)) {
            range_end = range_start + 1.0f;
        }
    }

    for (int i = 0; i < 4; ++i) {
        if (has_gradient) {
            const float projection = glm::dot(verts[i].pos_, direction);
            const float t = glm::clamp((projection - range_start) / (range_end - range_start), 0.0f, 1.0f);
            verts[i].color_ = glm::mix(start_color_vec, end_color_vec, t);
        } else {
            verts[i].color_ = start_color_vec;
        }
        vertices_.push_back(verts[i]);
    }

    // 将索引添加到索引缓冲区（对于四边形，索引数为6）
    uint32_t base_index = static_cast<uint32_t>(vertices_.size() - 4);
    uint32_t index_offset = static_cast<uint32_t>(indices_.size());
    indices_.push_back(base_index + 0);
    indices_.push_back(base_index + 1);
    indices_.push_back(base_index + 2);
    indices_.push_back(base_index + 2);
    indices_.push_back(base_index + 3);
    indices_.push_back(base_index + 0);

    // 将绘制命令添加到绘制命令缓冲区
    // 如果绘制命令缓冲区不为空，并且最后一个绘制命令的纹理和使用纹理状态与当前精灵相同，则增加索引计数
    // 否则创建新的绘制命令
    if (!commands_.empty() &&
        commands_.back().texture_ == texture &&
        commands_.back().use_texture_ == use_texture) {
        commands_.back().index_count_ += 6;
    } else {
        Command cmd;
        cmd.texture_ = texture;
        cmd.index_offset_ = index_offset;
        cmd.index_count_ = 6;
        cmd.use_texture_ = use_texture;
        commands_.push_back(cmd);
    }
    return true;
}

bool SpriteBatch::flush(const FlushParams& params) {
    last_draw_calls_ = 0;
    last_vertex_count_ = 0;
    last_index_count_ = 0;
    last_sprite_count_ = 0;

    // 没有可绘制内容：清空 CPU 缓冲区，以便下一帧重新开始。
    if (indices_.empty()) {
        reset();
        return true;
    }

    // 如果批处理未正确初始化或调用者未提供着色器程序，则提前返回。
    if (vao_ == 0 || vbo_ == 0 || ebo_ == 0 || params.program_ == 0) {
        reset();
        spdlog::error("SpriteBatch::flush: invalid parameters");
        return false;
    }
    if (default_texture_ == 0 && !ensureDefaultTexture()) {
        reset();
        spdlog::error("SpriteBatch::flush: failed to prepare default texture");
        return false;
    }

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);

    // 确保 GPU 缓冲区足够大。如果超出当前容量，则分配更大的缓冲区（ensureCapacity 将大小翻倍），
    // 然后重新绑定并上传新的顶点数据。
    size_t vertex_bytes = vertices_.size() * sizeof(Vertex);
    if (vertex_bytes > vertex_buffer_capacity_bytes_) {
        size_t required_sprites = vertices_.size() / 4 + 1;
        if (!ensureCapacity(required_sprites)) {
            glBindVertexArray(0);
            reset();
            spdlog::error("SpriteBatch::flush: ensureCapacity failed");
            return false;
        }
        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    }
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertex_bytes, vertices_.data());

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    size_t index_bytes = indices_.size() * sizeof(uint32_t);
    // 索引缓冲区同样遵循相同的增长逻辑：如果当前绘制批次不再适合，则重新分配更大的缓冲区（ensureCapacity 将大小翻倍），
    // 然后重新绑定并上传新的索引数据。
    if (index_bytes > index_buffer_capacity_bytes_) {
        size_t required_sprites = vertices_.size() / 4 + 1;
        if (!ensureCapacity(required_sprites)) {
            glBindVertexArray(0);
            reset();
            spdlog::error("SpriteBatch::flush: ensureCapacity failed");
            return false;
        }
        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferSubData(GL_ARRAY_BUFFER, 0, vertex_bytes, vertices_.data());
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    }
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, index_bytes, indices_.data());

    // 使用传入的着色器程序，设置视图投影矩阵和取样器等
    glUseProgram(params.program_);
    if (params.u_tex_ >= 0) {
        glUniform1i(params.u_tex_, 0);
    }
    if (params.u_view_proj_ >= 0 && params.apply_view_projection_) {
        params.apply_view_projection_(params.u_view_proj_);
    }
    if (params.u_use_texture_ >= 0) {
        glUniform1i(params.u_use_texture_, 0);
    }

    // 按纹理分组绘制，依次执行绘制命令
    glActiveTexture(GL_TEXTURE0);
    uint32_t draw_calls = 0;
    const uint32_t sprite_count = static_cast<uint32_t>(vertices_.size() / 4);
    const uint32_t vertex_count = static_cast<uint32_t>(vertices_.size());
    const uint32_t index_count = static_cast<uint32_t>(indices_.size());
    for (const Command& cmd : commands_) {
        GLuint tex = cmd.texture_ ? cmd.texture_ : default_texture_;
        glBindTexture(GL_TEXTURE_2D, tex);
        if (params.u_use_texture_ >= 0) {
            glUniform1i(params.u_use_texture_, cmd.use_texture_ ? 1 : 0);
        }
        const void* offset = reinterpret_cast<const void*>(static_cast<uintptr_t>(cmd.index_offset_ * sizeof(uint32_t)));
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(cmd.index_count_), GL_UNSIGNED_INT, offset);
        ++draw_calls;
    }

    // 解除绑定
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
    glUseProgram(0);

    last_draw_calls_ = draw_calls;
    last_vertex_count_ = vertex_count;
    last_index_count_ = index_count;
    last_sprite_count_ = sprite_count;

    reset();
    return logGlErrors("SpriteBatch::flush");
}

void SpriteBatch::reset() {
    vertices_.clear();
    indices_.clear();
    commands_.clear();
    if (capacity_ > 0) {
        vertices_.reserve(capacity_ * 4);
        indices_.reserve(capacity_ * 6);
        commands_.reserve(capacity_);
    }
}

bool SpriteBatch::ensureCapacity(size_t required_sprites) {
    if (vao_ == 0 || vbo_ == 0 || ebo_ == 0) {
        return false;
    }

    // 计算所需精灵数量（至少64个）
    size_t desired_sprites = std::max(required_sprites, capacity_ == 0 ? required_sprites : capacity_ * 2);
    desired_sprites = std::max(desired_sprites, MIN_SPRITE_CAPACITY);

    // 计算所需顶点和索引字节数
    size_t vertex_bytes = desired_sprites * 4 * sizeof(Vertex);
    size_t index_bytes = desired_sprites * 6 * sizeof(uint32_t);

    // 绑定顶点数组对象和缓冲区
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, vertex_bytes, nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_bytes, nullptr, GL_DYNAMIC_DRAW);
    glBindVertexArray(0);

    capacity_ = desired_sprites;
    vertex_buffer_capacity_bytes_ = vertex_bytes;
    index_buffer_capacity_bytes_ = index_bytes;
    return logGlErrors("SpriteBatch::ensureCapacity");
}

void SpriteBatch::destroyBuffers() {
    if (vbo_) { glDeleteBuffers(1, &vbo_); vbo_ = 0; }
    if (ebo_) { glDeleteBuffers(1, &ebo_); ebo_ = 0; }
    if (vao_) { glDeleteVertexArrays(1, &vao_); vao_ = 0; }
}

bool SpriteBatch::ensureDefaultTexture() {
    if (default_texture_ != 0) {
        return true;
    }
    glGenTextures(1, &default_texture_);
    if (default_texture_ == 0) {
        spdlog::error("SpriteBatch::ensureDefaultTexture: glGenTextures failed");
        return false;
    }
    glBindTexture(GL_TEXTURE_2D, default_texture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    const uint32_t white_pixel = 0xFFFFFFFFu;
    const ScopedGLUnpackAlignment scoped_unpack_alignment(4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &white_pixel);
    glBindTexture(GL_TEXTURE_2D, 0);
    return logGlErrors("SpriteBatch::ensureDefaultTexture");
}

void SpriteBatch::destroyDefaultTexture() {
    if (default_texture_ != 0) {
        glDeleteTextures(1, &default_texture_);
        default_texture_ = 0;
    }
}

} // namespace engine::render::opengl
