#pragma once

/**
 * @file composite_pass.h
 * @brief 负责将场景、光照、自发光及泛光等缓冲区混合输出至最终的颜色缓冲。
 */
#include "engine/utils/math.h"
#include <memory>
#include <glad/glad.h>
#include <glm/vec3.hpp>

namespace engine::render::opengl {

class ShaderLibrary;
class ShaderProgram;

class CompositePass {
    ShaderProgram* program_ = nullptr;
    bool bloom_enabled_{true};
    // 缓存的 uniform 位置
    GLint u_scene_tex_{-1};
    GLint u_light_tex_{-1};
    GLint u_ambient_{-1};
    GLint u_emissive_tex_{-1};
    GLint u_bloom_tex_{-1};
    GLint u_bloom_strength_{-1};

    GLuint scene_tex_{0};
    GLuint light_tex_{0};
    GLuint emissive_tex_{0};
    GLuint bloom_tex_{0};
    glm::vec3 ambient_{glm::vec3(0.5f)};
    float bloom_strength_{1.0f};
    
    GLuint vao_{0};
    GLuint vbo_{0};
    GLsizei vertex_count_{0};

    // 默认纹理：white_tex_ 用于光照，black_tex_ 用于自发光和泛光
    GLuint white_tex_{0};
    GLuint black_tex_{0};

    bool createBuffers();

public:
    [[nodiscard]] static std::unique_ptr<CompositePass> create(ShaderLibrary& library);
    ~CompositePass();

    CompositePass(const CompositePass&) = delete;
    CompositePass& operator=(const CompositePass&) = delete;
    CompositePass(CompositePass&&) = delete;
    CompositePass& operator=(CompositePass&&) = delete;

    void setAmbient(const glm::vec3& ambient) { ambient_ = ambient; }
    void setBloomStrength(float strength) { bloom_strength_ = strength; }
    void setSceneTexture(GLuint tex) { scene_tex_ = tex; }
    void setLightTexture(GLuint tex) { light_tex_ = tex; }
    void setEmissiveTexture(GLuint tex) { emissive_tex_ = tex; }
    void setBloomTexture(GLuint tex) { bloom_tex_ = tex; }

    void setBloomEnabled(bool enabled) { bloom_enabled_ = enabled; }

    [[nodiscard]] const glm::vec3& getAmbient() const { return ambient_; }
    [[nodiscard]] float getBloomStrength() const { return bloom_strength_; }
    bool render(const utils::Rect& viewport);
    void clean();

private:
    CompositePass() = default;

    [[nodiscard]] bool init(ShaderLibrary& library);
};

} // namespace engine::render::opengl
