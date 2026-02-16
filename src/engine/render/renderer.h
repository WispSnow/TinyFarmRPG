#pragma once
#include "image.h"
#include "nine_slice.h"
#include "engine/component/sprite_component.h"
#include "engine/utils/math.h"
#include "engine/utils/defs.h"
#include <optional>
#include <memory>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

struct SDL_Renderer;
struct SDL_FRect;

namespace engine::resource {
    class ResourceManager;
}

namespace engine::render::opengl {
    class GLRenderer;
}

namespace engine::render {
class Camera;

/**
 * @brief 渲染外观层（Facade）
 *
 * Renderer 提供给上层系统/Scene 使用的高层渲染 API：
 * - 负责资源获取（通过 ResourceManager 获取纹理等）
 * - 负责少量与项目约定相关的逻辑（例如 viewport clipping / 默认绘制参数）
 * - 将具体绘制提交给底层 OpenGL 后端（GLRenderer）
 *
 * 设计目的：上层不直接依赖 OpenGL/Pass/FBO 等细节，便于渲染管线演进（L08–L10）。
 */
class Renderer final{
private:
    engine::render::opengl::GLRenderer* gl_renderer_ = nullptr;     ///< @brief 指向 GLRenderer 的非拥有指针
    engine::resource::ResourceManager* resource_manager_ = nullptr; ///< @brief 指向 ResourceManager 的非拥有指针

    engine::utils::FColor clear_color_{0.0f, 0.0f, 0.0f, 1.0f};///< @brief 清除屏幕的颜色（默认黑色），可调用setClearColorFloat设置
    const Camera* current_camera_{nullptr};                         ///< @brief 当前帧使用的相机指针
public:
    /**
     * @brief 创建并初始化渲染器。
     * @param gl_renderer 指向有效的 GLRenderer 的指针（不能为空）。
     * @param resource_manager 指向有效的 ResourceManager 的指针（不能为空）。
     * @return 创建成功返回实例；失败返回 nullptr。
     */
    [[nodiscard]] static std::unique_ptr<Renderer> create(engine::render::opengl::GLRenderer* gl_renderer,
                                                          engine::resource::ResourceManager* resource_manager);

    /**
     * @brief 开始渲染帧
     * 
     * @param camera 游戏相机，用于坐标转换。
     */
    void beginFrame(const Camera& camera);
    void setViewportClippingEnabled(bool enabled);
    [[nodiscard]] bool isViewportClippingEnabled() const;
    [[nodiscard]] std::optional<engine::utils::Rect> getCurrentViewRect() const;

    /**
     * @brief 绘制一个精灵
     * 
     * @param sprite 包含纹理ID、源矩形和翻转状态的 Sprite 对象。
     * @param position 世界坐标中的左上角位置。
     * @param size 精灵的大小。
     * @param rotation 旋转角度（度）。
     * @param color 调整颜色。(原始颜色*调整颜色，默认为白色，即不调整)
     */
    void drawSprite(const component::Sprite& sprite,
                    const glm::vec2& position,
                    const glm::vec2& size,
                    const engine::utils::ColorOptions* color_options = nullptr,
                    const engine::utils::TransformOptions* transform_options = nullptr);

    /**
     * @brief 绘制一条世界坐标系下的直线
     *
     * @param start 起点坐标
     * @param end 终点坐标
     * @param thickness 线宽(像素)
     * @param color_options 可选颜色/渐变设置
     */
    void drawLine(const glm::vec2& start,
                  const glm::vec2& end,
                  float thickness = 1.0f,
                  const engine::utils::ColorOptions* color_options = nullptr);

    /**
     * @brief 绘制圆形边框
     *
     * @param center 圆心坐标
     * @param radius 半径
     * @param thickness 线宽
     * @param color_options 可选颜色/渐变设置
     */
    void drawCircleOutline(const glm::vec2& center,
                           float radius,
                           float thickness = 1.0f,
                           const engine::utils::ColorOptions* color_options = nullptr);

    /**
     * @brief 绘制填充圆形
     * @note 必须存在默认圆形纹理"assets/textures/UI/circle.png"
     * 
     * @param position 圆形中心位置
     * @param radius 圆形半径
     * @param params 纹理绘制参数（旋转、颜色、渐变等）
     */
    void drawFilledCircle(const glm::vec2& position,
                          float radius,
                          const engine::utils::ColorOptions* color_options = nullptr,
                          const engine::utils::TransformOptions* transform_options = nullptr);

    /**
     * @brief 绘制填充矩形
     * 
     * @param rect 矩形区域
     * @param params 纹理绘制参数（颜色、渐变、旋转等）
     */
    void drawFilledRect(const utils::Rect& rect,
                        const engine::utils::ColorOptions* color_options = nullptr,
                        const engine::utils::TransformOptions* transform_options = nullptr);

    /**
     * @brief 绘制矩形边框
     * 
     * @param position 矩形左上角位置
     * @param size 矩形大小
     * @param thickness 边框厚度
     * @param params 纹理绘制参数（当前主要用于颜色）
     */
    void drawRect(const glm::vec2& position,
                  const glm::vec2& size,
                  float thickness = 1.0f,
                  const engine::utils::ColorOptions* color_options = nullptr);
    
    /**
     * @brief 在屏幕坐标中直接渲染一个用于UI的Image对象。
     *
     * @param image 包含纹理ID、源矩形和翻转状态的Image对象。
     * @param position 屏幕坐标中的左上角位置。
     * @param size 目标矩形的大小。
     */
    void drawUIImage(const Image& image,
                     const glm::vec2& position,
                     const glm::vec2& size,
                     const engine::utils::ColorOptions* color_options = nullptr,
                     const engine::utils::TransformOptions* transform_options = nullptr);

    /**
     * @brief 绘制UI填充矩形
     * 
     * @param rect 矩形区域
     * @param params 纹理绘制参数（颜色、渐变、旋转等）
     */
    void drawUIFilledRect(const engine::utils::Rect& rect,
                          const engine::utils::ColorOptions* color_options = nullptr,
                          const engine::utils::TransformOptions* transform_options = nullptr);

    /**
     * @brief 添加点光源到当前帧
     */
    void addPointLight(const glm::vec2& position, float radius,
                       const engine::utils::PointLightOptions* options = nullptr);

    /**
     * @brief 添加聚光灯到当前帧
     */
    void addSpotLight(const glm::vec2& position, float radius, const glm::vec2& direction,
                      const engine::utils::SpotLightOptions* options = nullptr);

    /**
     * @brief 添加方向光到当前帧
     */
    void addDirectionalLight(const glm::vec2& direction,
                             const engine::utils::DirectionalLightOptions* options = nullptr);

    /**
     * @brief 设置环境光（合成阶段使用）
     */
    void setAmbient(const glm::vec3& ambient);

    /**
     * @brief 添加自发光矩形
     */
    void addEmissiveRect(const engine::utils::Rect& rect,
                         const engine::utils::EmissiveParams* params = nullptr);

    /**
     * @brief 添加自发光精灵
     */
    void addEmissiveSprite(const component::Sprite& sprite, const glm::vec2& position,
                           const glm::vec2& size,
                           const engine::utils::EmissiveParams* params = nullptr);

    void present();                                                     ///< @brief 提交并显示本帧（转发到 GLRenderer::present）
    void clearScreen();                                                 ///< @brief 清空本帧缓冲（转发到 GLRenderer::clear）

    void setClearColorFloat(const engine::utils::FColor& color);  ///< @brief 设置清除屏幕颜色，使用 float 类型

    engine::render::opengl::GLRenderer* getGLRenderer() const { return gl_renderer_; }          ///< @brief 获取底层的 GLRenderer 指针

    void setDefaultWorldColorOptions(const engine::utils::ColorOptions& options);
    void setDefaultWorldTransformOptions(const engine::utils::TransformOptions& options);
    void setDefaultUIColorOptions(const engine::utils::ColorOptions& options);
    void setDefaultUITransformOptions(const engine::utils::TransformOptions& options);

    [[nodiscard]] engine::utils::ColorOptions getDefaultWorldColorOptions() const;
    [[nodiscard]] engine::utils::TransformOptions getDefaultWorldTransformOptions() const;
    [[nodiscard]] engine::utils::ColorOptions getDefaultUIColorOptions() const;
    [[nodiscard]] engine::utils::TransformOptions getDefaultUITransformOptions() const;

    // 禁用拷贝和移动语义
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

private:
    Renderer(engine::render::opengl::GLRenderer* gl_renderer, engine::resource::ResourceManager* resource_manager);

    /**
     * @brief 获取源矩形UV坐标
     * 
     * @param texture 纹理
     * @param src_rect 源矩形
     * @return 源矩形UV坐标
     */
    glm::vec4 getSrcRectUV(const utils::GL_Texture& texture, const utils::Rect& src_rect) const;
    glm::vec4 getSrcRectUV(const utils::GL_Texture& texture, const glm::vec2& position, const glm::vec2& size) const;
    [[nodiscard]] bool shouldCullRect(const engine::utils::Rect& rect) const;
    [[nodiscard]] bool shouldCullCircle(const glm::vec2& center, float radius) const;

    void drawUINineSliceInternal(const Image& image,
                                 const NineSlice& nine_slice,
                                 const glm::vec2& position,
                                 const glm::vec2& size,
                                 const engine::utils::ColorOptions* color_options,
                                 const engine::utils::TransformOptions* transform_options);
};
} // namespace engine::render
