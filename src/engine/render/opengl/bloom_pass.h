#pragma once
/**
 * @file bloom_pass.h
 * @brief 负责将自发光纹理进行高斯模糊，生成泛光纹理。
 */

#include <glad/glad.h>
#include <glm/vec2.hpp>
#include <memory>

namespace engine::render::opengl {

class ShaderLibrary;
class ShaderProgram;

class BloomPass {
    static constexpr size_t BLOOM_LEVELS{4};

    int viewport_width_{0};
    int viewport_height_{0};

    // 高斯模糊着色器程序和 uniform 位置
    ShaderProgram* blur_program_{nullptr};
    GLint u_blur_texel_size_{-1};
    GLint u_blur_direction_{-1};
    GLint u_blur_sigma_{-1};

    // 全屏四边形几何体
    GLuint vao_{0};
    GLuint vbo_{0};

    // Ping-pong FBO 和纹理，每个级别一个
    GLuint ping_fbo_[BLOOM_LEVELS]{0};
    GLuint ping_tex_[BLOOM_LEVELS]{0};
    GLuint pong_fbo_[BLOOM_LEVELS]{0};
    GLuint pong_tex_[BLOOM_LEVELS]{0};
    int level_width_[BLOOM_LEVELS]{0};
    int level_height_[BLOOM_LEVELS]{0};

    // 最终的泛光纹理 (pong 级别 0)
    GLuint bloom_tex_{0};

    // 高斯模糊参数
    float sigma_{2.5f};

    // 统计信息（用于 DebugPanel / Pass Stats）
    uint32_t last_draw_calls_{0};
    uint32_t last_levels_{0};

public:
    [[nodiscard]] static std::unique_ptr<BloomPass> create(ShaderLibrary& library, const glm::vec2& viewport_size);
    ~BloomPass();

    BloomPass(const BloomPass&) = delete;
    BloomPass& operator=(const BloomPass&) = delete;

    void clean();

    // 执行泛光，使用给定的自发光纹理作为输入
    bool process(GLuint emissive_tex);
    void clear();

    void setSigma(float s) { sigma_ = s; }
    [[nodiscard]] float getSigma() const { return sigma_; }
    GLuint getBloomTex() const { return bloom_tex_; }
    [[nodiscard]] uint32_t getLastDrawCallCount() const { return last_draw_calls_; }
    [[nodiscard]] uint32_t getLastLevelCount() const { return last_levels_; }

private:
    BloomPass(int viewport_width, int viewport_height) : viewport_width_(viewport_width), viewport_height_(viewport_height) {}

    [[nodiscard]] bool init(ShaderLibrary& library);
    [[nodiscard]] bool createTargets(int width, int height);
    void destroyTargets();

    [[nodiscard]] bool createBuffers();
};

} // namespace engine::render::opengl
