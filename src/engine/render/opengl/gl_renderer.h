#pragma once
#include "engine/utils/defs.h"
#include <memory>
#include <string_view>
#include <array>
#include <cstdint>
#include <optional>
#include <glad/glad.h>
#include <SDL3/SDL.h>
#include <glm/vec2.hpp>
#include <glm/mat4x4.hpp>
#include <entt/core/fwd.hpp>
#include "engine/utils/math.h"

namespace engine::render {
    class Camera;
}

namespace engine::debug {
    class DebugUIManager;
}

namespace engine::render::opengl {
    class RenderContext;
    class ViewportManager;
    class ShaderLibrary;
    class ShaderProgram;
    class CompositePass;
    class LightingPass;
    class EmissivePass;
    class BloomPass;
    class ScenePass;
    class UIPass;
    class ImGuiLayer;

class GLRenderer final {
public:
    enum class PassType : uint8_t {
        Scene = 0,
        Lighting,
        Emissive,
        Bloom,
        UI,
        Count
    };

    struct PassStats {
        uint32_t draw_calls{0};
        uint32_t sprite_count{0};
        uint32_t vertex_count{0};
        uint32_t index_count{0};
    };

private:
    std::unique_ptr<RenderContext> render_context_;
    std::unique_ptr<ViewportManager> viewport_manager_;
    std::unique_ptr<ShaderLibrary> shader_library_;
    std::unique_ptr<CompositePass> composite_pass_;
    std::unique_ptr<ScenePass> scene_pass_;
    std::unique_ptr<LightingPass> lighting_pass_;
    std::unique_ptr<EmissivePass> emissive_pass_;
    std::unique_ptr<BloomPass> bloom_pass_;
    std::unique_ptr<UIPass> ui_pass_;
#ifdef TF_ENABLE_DEBUG_UI
    std::unique_ptr<ImGuiLayer> imgui_layer_;
    engine::debug::DebugUIManager* debug_ui_manager_{nullptr};
#endif

    glm::vec4 clear_color_{0.0f, 0.0f, 0.0f, 1.0f};
    bool viewport_clipping_enabled_{true};

    // 开关
    bool point_lights_enabled_{true};
    bool spot_lights_enabled_{true};
    bool directional_lights_enabled_{true};
    bool bloom_enabled_{true};
    bool emissive_enabled_{true};
#ifdef TF_ENABLE_DEBUG_UI
    bool debug_ui_enabled_{true};
#endif
    bool pixel_snap_enabled_{true};

    glm::vec2 logical_size_{0.0f, 0.0f};
    glm::vec2 window_size_pixels_{0.0f, 0.0f};  // Renderer关注窗口实际像素尺寸
    glm::mat4 current_view_proj_{1.0f};
    std::optional<engine::utils::Rect> current_view_rect_;

    // 保存各个通道绘制的纹理句柄
    GLuint scene_color_tex_{0};
    GLuint light_color_tex_{0};
    GLuint emissive_color_tex_{0};
    GLuint bloom_tex_{0};

    // 保存各个通道绘制的统计信息(DebugPanel需要)
    std::array<PassStats, static_cast<size_t>(PassType::Count)> pass_stats_{};

public:
    [[nodiscard]] static std::unique_ptr<GLRenderer> create(SDL_Window* window,
                                                            const glm::vec2& logical_size,
                                                            std::string_view params_json_path = "");
    ~GLRenderer();

    // 绘制场景中精灵(矩形/纹理)
    void drawRect(const glm::vec4& rect,
                  const engine::utils::ColorOptions* color = nullptr,
                  const engine::utils::TransformOptions* transform = nullptr);
    void drawRectGradient(const glm::vec4& rect,
                          const glm::vec4& start_color,
                          const glm::vec4& end_color,
                          float gradient_angle_radians,
                          const engine::utils::TransformOptions* transform = nullptr);
    void drawLine(const glm::vec2& start,
                  const glm::vec2& end,
                  float thickness,
                  const engine::utils::ColorOptions* color = nullptr);

    void drawTexture(GLuint texture, const glm::vec4& dst_rect, const glm::vec4& uv_rect,
                     const engine::utils::ColorOptions* color = nullptr,
                     const engine::utils::TransformOptions* transform = nullptr);
    void drawTexture(GLuint texture, const glm::vec4& dst_rect,
                     const glm::vec2& texture_size_pixels,
                     const engine::utils::Rect& src_rect_pixels,
                     const engine::utils::ColorOptions* color = nullptr,
                     const engine::utils::TransformOptions* transform = nullptr);
    void drawTextureGradient(GLuint texture, const glm::vec4& dst_rect, const glm::vec4& uv_rect,
                             const glm::vec4& start_color,
                             const glm::vec4& end_color,
                             float gradient_angle_radians,
                             const engine::utils::TransformOptions* transform = nullptr);

    // 添加光照
    void addPointLight(const glm::vec2& pos, float radius,
                       const engine::utils::PointLightOptions* options = nullptr);
    void addSpotLight(const glm::vec2& pos, float radius,
                      const glm::vec2& dir,
                      const engine::utils::SpotLightOptions* options = nullptr);
    // 方向光：dir 为屏幕空间方向（例如 (0,-1) 自上而下），offset/softness 控制渐变，middayBlend 控制中午均匀度
    void addDirectionalLight(const glm::vec2& dir,
                             const engine::utils::DirectionalLightOptions* options = nullptr);

    // 添加自发光
    void addEmissiveRect(const glm::vec4& rect,
                         const engine::utils::EmissiveParams* params = nullptr);
    void addEmissiveTexture(GLuint texture, const glm::vec4& dst_rect, const glm::vec4& uv_rect,
                            bool flip_horizontal = false,
                            const engine::utils::EmissiveParams* params = nullptr);

    // 绘制UI中精灵(矩形/纹理)
    void drawUIRect(const glm::vec4& rect,
                    const engine::utils::ColorOptions* color = nullptr,
                    const engine::utils::TransformOptions* transform = nullptr);
    void drawUIRectGradient(const glm::vec4& rect,
                            const glm::vec4& start_color,
                            const glm::vec4& end_color,
                            float gradient_angle_radians,
                            const engine::utils::TransformOptions* transform = nullptr);
    void drawUITexture(GLuint texture, const glm::vec4& dst_rect, const glm::vec4& uv_rect,
                       const engine::utils::ColorOptions* color = nullptr,
                       const engine::utils::TransformOptions* transform = nullptr);
    void drawUITexture(GLuint texture, const glm::vec4& dst_rect,
                       const glm::vec2& texture_size_pixels,
                       const engine::utils::Rect& src_rect_pixels,
                       const engine::utils::ColorOptions* color = nullptr,
                       const engine::utils::TransformOptions* transform = nullptr);
    void drawUITextureGradient(GLuint texture, const glm::vec4& dst_rect, const glm::vec4& uv_rect,
                               const glm::vec4& start_color,
                               const glm::vec4& end_color,
                               float gradient_angle_radians,
                               const engine::utils::TransformOptions* transform = nullptr);

    void beginFrame(const Camera& camera);  // 开始渲染帧, 计算本帧需要的视图投影矩阵

    // 调试UI
    void beginDebugUI();
    void endDebugUI();

    void handleSDLEvent(const SDL_Event& event);
    void setDebugUIManager(engine::debug::DebugUIManager* manager);

    void setVSyncEnabled(bool enabled);
    [[nodiscard]] int getSwapInterval() const;

    void setClearColor(const glm::vec4& color) { clear_color_ = color; }
    void clear();
    void present();
    void resize(int width, int height);     // 调整窗口大小
    void clean();

    // 开关控制
    void setDebugUIEnabled(bool enabled);
    void setPointLightsEnabled(bool enabled) { point_lights_enabled_ = enabled; }
    void setSpotLightsEnabled(bool enabled) { spot_lights_enabled_ = enabled; }
    void setDirectionalLightsEnabled(bool enabled) { directional_lights_enabled_ = enabled; }
    void setBloomEnabled(bool enabled);
    void setEmissiveEnabled(bool enabled) { emissive_enabled_ = enabled; }
    void setPixelSnapEnabled(bool enabled) { pixel_snap_enabled_ = enabled; }
    [[nodiscard]] bool isPointLightsEnabled() const { return point_lights_enabled_; }
    [[nodiscard]] bool isSpotLightsEnabled() const { return spot_lights_enabled_; }
    [[nodiscard]] bool isDirectionalLightsEnabled() const { return directional_lights_enabled_; }
    [[nodiscard]] bool isBloomEnabled() const { return bloom_enabled_; }
    [[nodiscard]] bool isEmissiveEnabled() const { return emissive_enabled_; }
    [[nodiscard]] bool isDebugUIEnabled() const {
#ifdef TF_ENABLE_DEBUG_UI
        return debug_ui_enabled_;
#else
        return false;
#endif
    }
    [[nodiscard]] bool isPixelSnapEnabled() const { return pixel_snap_enabled_; }

    // 参数控制
    void setAmbient(const glm::vec3& ambient);
    void setBloomStrength(float strength);
    void setBloomSigma(float sigma);
    [[nodiscard]] const glm::vec3& getAmbient() const;
    [[nodiscard]] float getBloomStrength() const;
    [[nodiscard]] float getBloomSigma() const;
    [[nodiscard]] uint32_t getBloomLevelCount() const;
    [[nodiscard]] const PassStats& getPassStats(PassType pass) const;

    // 调试：渲染中间纹理预览（DebugPanel 使用）
    [[nodiscard]] GLuint getSceneColorTex() const { return scene_color_tex_; }
    [[nodiscard]] GLuint getLightColorTex() const { return light_color_tex_; }
    [[nodiscard]] GLuint getEmissiveColorTex() const { return emissive_color_tex_; }
    [[nodiscard]] GLuint getBloomTex() const { return bloom_tex_; }

    void setViewportClippingEnabled(bool enabled);
    [[nodiscard]] bool isViewportClippingEnabled() const { return viewport_clipping_enabled_; }
    [[nodiscard]] std::optional<engine::utils::Rect> getCurrentViewRect() const { return current_view_rect_; }
    [[nodiscard]] bool shouldCullRect(const engine::utils::Rect& rect) const;
    [[nodiscard]] bool shouldCullCircle(const glm::vec2& center, float radius) const;
    [[nodiscard]] const glm::vec2& getLogicalSize() const { return logical_size_; }
    [[nodiscard]] glm::vec2 getWindowSizePixels() const;
    [[nodiscard]] engine::utils::Rect getViewportPixels() const;
    [[nodiscard]] engine::utils::LetterboxMetrics getLetterboxMetricsPixels() const;

    void setSceneDefaultColorOptions(const engine::utils::ColorOptions& options);
    void setSceneDefaultTransformOptions(const engine::utils::TransformOptions& options);
    [[nodiscard]] engine::utils::ColorOptions getSceneDefaultColorOptions() const;
    [[nodiscard]] engine::utils::TransformOptions getSceneDefaultTransformOptions() const;

    void setUIDefaultColorOptions(const engine::utils::ColorOptions& options);
    void setUIDefaultTransformOptions(const engine::utils::TransformOptions& options);
    [[nodiscard]] engine::utils::ColorOptions getUIDefaultColorOptions() const;
    [[nodiscard]] engine::utils::TransformOptions getUIDefaultTransformOptions() const;

private:
    GLRenderer() = default;

    [[nodiscard]] bool init(SDL_Window* window, const glm::vec2& logical_size, std::string_view params_json_path = "");

    [[nodiscard]] bool initViewportManager();
    [[nodiscard]] bool initImGuiLayer();
    [[nodiscard]] bool initLightingPass();
    [[nodiscard]] bool initEmissivePass();
    [[nodiscard]] bool initBloomPass();
    [[nodiscard]] bool initScenePass();
    [[nodiscard]] bool initUIPass();
    [[nodiscard]] bool initCompositePass();

    glm::mat4 computeViewProjection(const Camera& camera);      ///< @brief 计算视图投影矩阵
    void applyViewProjection(GLint location);                    ///< @brief 应用视图投影矩阵
    [[nodiscard]] std::optional<engine::utils::Rect> computeCameraViewRect(const Camera& camera) const;
};

} // namespace engine::render::opengl
