#pragma once

/**
 * @brief 管理光照累积通道专用的着色器和OpenGL缓冲区的对象。
 *
 * LightingPass 拥有用于渲染全屏四边形以写入光照累积目标的着色器和GL缓冲区。
 * 从而使本通道负责所有着色器 uniform 设定、加法混合与视口管理等细节。
 *
 * 坐标空间约定（便于理解三类光源的差异）：
 * - 点光/聚光：使用 world-space quad（以光源为中心的矩形），通过相机的 `uViewProj` 投影到屏幕。
 * - 方向光：使用 screen-space quad（覆盖整屏的矩形），绘制前临时把 `uViewProj` 切换为屏幕正交投影，
 *   以确保方向光的渐变遮罩不跟随相机移动；绘制完成后再恢复相机 `uViewProj`，避免影响后续光源。
 */

#include <glad/glad.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <vector>
#include <cstdint>
#include <functional>
#include "engine/utils/math.h"
#include "engine/utils/defs.h"
#include <memory>

namespace engine::render::opengl {

class ShaderLibrary;
class ShaderProgram;

class LightingPass {
    int viewport_width_{0};
    int viewport_height_{0};
    ShaderProgram* program_{nullptr};
    GLuint vao_{0};
    GLuint vbo_{0};
    GLuint fbo_{0};
    GLuint color_tex_{0};
    bool active_{false};

    std::function<void(GLint)> apply_view_projection_{nullptr};

    // 缓存的 uniform 位置，避免每帧重复调用 glGetUniformLocation，以便此通道能高效更新着色器状态。
    GLint u_view_proj_{-1};
    GLint u_light_color_{-1};
    GLint u_light_intensity_{-1};
    GLint u_light_type_{-1};
    GLint u_spot_dir_{-1};
    GLint u_spot_inner_cos_{-1};
    GLint u_spot_outer_cos_{-1};
    GLint u_midday_blend_{-1};
    GLint u_dir_2d_{-1};
    GLint u_dir_offset_{-1};
    GLint u_dir_softness_{-1};

    // 批处理命令缓冲
    enum class LightType : int { Point = 0, Spot = 1, Directional = 2 };
    struct LightCommand {
        LightType type{LightType::Point};
        // 通用
        glm::vec3 color{1.0f};
        float intensity{1.0f};
        // 点/聚光
        glm::vec2 pos{0.0f};
        float radius{0.0f};
        // 聚光参数
        glm::vec2 spot_dir{0.0f, -1.0f};
        float spot_inner_cos{1.0f};
        float spot_outer_cos{1.0f};
        // 方向光参数（屏幕空间）
        glm::vec2 dir2d{0.0f, -1.0f};
        float dir_offset{0.5f};
        float dir_softness{0.1f};
        float midday_blend{0.0f};
        float zoom{1.0f};
    };
    std::vector<LightCommand> commands_{};
    uint32_t last_draw_calls_{0};
    uint32_t last_vertex_count_{0};

public:
    [[nodiscard]] static std::unique_ptr<LightingPass> create(ShaderLibrary& library, const glm::vec2& viewport_size);
    ~LightingPass();

    LightingPass(const LightingPass&) = delete;
    LightingPass& operator=(const LightingPass&) = delete;
    LightingPass(LightingPass&&) = delete;
    LightingPass& operator=(LightingPass&&) = delete;

    void addPointLight(const glm::vec2& pos, float radius,
                       const engine::utils::PointLightOptions& options);
    void addSpotLight(const glm::vec2& pos, float radius, const glm::vec2& dir,
                      const engine::utils::SpotLightOptions& options);
    void addDirectionalLight(const glm::vec2& dir,
                             const engine::utils::DirectionalLightOptions& options);
    void clear();
    void clean();

    ShaderProgram* program() const { return program_; }
    bool isActive() const { return active_; }
    GLuint getColorTex() const { return color_tex_; }
    GLuint getFBO() const { return fbo_; }

    bool flush(const utils::Rect& viewport,
               const std::function<void(GLint)>& apply_view_projection);
    uint32_t getLastDrawCallCount() const { return last_draw_calls_; }
    uint32_t getLastVertexCount() const { return last_vertex_count_; }

private:
    LightingPass(int viewport_width, int viewport_height) : viewport_width_(viewport_width), viewport_height_(viewport_height) {}

    [[nodiscard]] bool init(ShaderLibrary& library);
    [[nodiscard]] bool createFBO(int width, int height);
    void destroyFBO();

    [[nodiscard]] bool createBuffers();


};

} // namespace engine::render::opengl
