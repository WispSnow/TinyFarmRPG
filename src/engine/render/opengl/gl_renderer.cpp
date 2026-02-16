#include "engine/render/opengl/gl_renderer.h"
#include "engine/render/opengl/gl_helper.h"
#include "engine/render/opengl/render_context.h"
#ifdef TF_ENABLE_DEBUG_UI
#include "engine/render/opengl/imgui_layer.h"
#endif
#include "engine/render/opengl/viewport_manager.h"
#include "engine/render/opengl/shader_library.h"
#include "engine/render/opengl/composite_pass.h"
#include "engine/render/opengl/lighting_pass.h"
#include "engine/render/opengl/emissive_pass.h"
#include "engine/render/opengl/bloom_pass.h"
#include "engine/render/opengl/scene_pass.h"
#include "engine/render/opengl/ui_pass.h"
#ifdef TF_ENABLE_DEBUG_UI
#include "engine/debug/debug_ui_manager.h"
#endif
#include "engine/render/camera.h"
#include <spdlog/spdlog.h>
#include <entt/core/hashed_string.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/epsilon.hpp> 
#include <glm/gtc/constants.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <glm/geometric.hpp>

using namespace entt::literals;

namespace engine::render::opengl {

using engine::utils::FColor;
using engine::utils::ColorOptions;

std::unique_ptr<GLRenderer> GLRenderer::create(SDL_Window* window, 
                                               const glm::vec2& logical_size, 
                                               std::string_view params_json_path) {
    auto renderer = std::unique_ptr<GLRenderer>(new GLRenderer());
    if (!renderer->init(window, logical_size, params_json_path)) {
        return nullptr;
    }
    return renderer;
}

GLRenderer::~GLRenderer() {
    clean();
}

bool GLRenderer::init(SDL_Window* window, 
                      const glm::vec2& logical_size,
                      std::string_view params_json_path) {
    logical_size_ = logical_size;
    if (render_context_ = RenderContext::create(window, params_json_path); !render_context_) {
        spdlog::error("创建 RenderContext 失败。");
        return false;
    }
#ifdef TF_ENABLE_DEBUG_UI
    if (!initImGuiLayer()) {
        spdlog::error("创建 ImGuiLayer 失败。");
        return false;
    }
#endif
    if (!initViewportManager()) {
        spdlog::error("初始化 ViewportManager 失败。");
        return false;
    }
    shader_library_ = std::make_unique<ShaderLibrary>();

    // 初始化各个通道：场景、光照、自发光、泛光、合成、UI
    if (!initScenePass()) {
        spdlog::error("创建 ScenePass 失败。");
        return false;
    }
    if (!initLightingPass()) {
        spdlog::error("创建 LightingPass 失败。");
        return false;
    }
    if (!initEmissivePass()) {
        spdlog::error("创建 EmissivePass 失败。");
        return false;
    }
    if (!initBloomPass()) {
        spdlog::error("创建 BloomPass 失败。");
        return false;
    }
    if (!initCompositePass()) {
        spdlog::error("创建 CompositePass 失败。");
        return false;
    }
    if (!initUIPass()) {
        spdlog::error("创建 UIPass 失败。");
        return false;
    }

    // 启用 sRGB 默认帧缓冲输出
    glEnable(GL_FRAMEBUFFER_SRGB);
    // 启用透明混合
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    return logGlErrors("GLRenderer::init");
}

namespace {
glm::vec4 computeUVFromPixels(const glm::vec2& texture_size, const engine::utils::Rect& src_rect) {
    if (texture_size.x <= 0.0f || texture_size.y <= 0.0f) {
        return {0.0f, 0.0f, 1.0f, 1.0f};
    }
    const float inv_w = 1.0f / texture_size.x;
    const float inv_h = 1.0f / texture_size.y;
    return {
        src_rect.pos.x * inv_w,
        src_rect.pos.y * inv_h,
        (src_rect.pos.x + src_rect.size.x) * inv_w,
        (src_rect.pos.y + src_rect.size.y) * inv_h
    };
}

inline FColor toFColor(const glm::vec4& value) {
    return {value.r, value.g, value.b, value.a};
}

inline float rotationAbsAroundZero(float radians) {
    const float two_pi = glm::two_pi<float>();
    float normalized = std::fmod(radians, two_pi);
    if (normalized < 0.0f) {
        normalized += two_pi;
    }
    return std::min(normalized, two_pi - normalized);
}

} // namespace

void GLRenderer::drawRect(const glm::vec4& rect,
                          const ColorOptions* color,
                          const engine::utils::TransformOptions* transform) {
    if (scene_pass_) {
        scene_pass_->queueSprite(0, false, rect, {0.0f, 0.0f, 1.0f, 1.0f},
                                 color, transform);
    }
}

void GLRenderer::drawLine(const glm::vec2& start,
                          const glm::vec2& end,
                          float thickness,
                          const ColorOptions* color) {
    if (!scene_pass_ || thickness <= 0.0f) {
        return;
    }

    const glm::vec2 delta = end - start;
    const float length = glm::length(delta);
    const float half_thickness = thickness * 0.5f;

    if (length <= std::numeric_limits<float>::epsilon()) {
        const glm::vec4 rect{start.x - half_thickness,
                             start.y - half_thickness,
                             thickness,
                             thickness};
        scene_pass_->queueSprite(0, false, rect, {0.0f, 0.0f, 1.0f, 1.0f},
                                 color, nullptr);
        return;
    }

    engine::utils::TransformOptions transform = scene_pass_->getDefaultTransformOptions();
    transform.rotation_radians = std::atan2(delta.y, delta.x);
    transform.pivot = {0.0f, 0.5f};

    const glm::vec4 rect{start.x,
                         start.y - half_thickness,
                         length,
                         thickness};

    scene_pass_->queueSprite(0, false, rect, {0.0f, 0.0f, 1.0f, 1.0f},
                             color, &transform);
}

void GLRenderer::drawRectGradient(const glm::vec4& rect,
                                  const glm::vec4& start_color,
                                  const glm::vec4& end_color,
                                  float gradient_angle_radians,
                                  const engine::utils::TransformOptions* transform) {
    ColorOptions color{};
    color.use_gradient = true;
    color.start_color = toFColor(start_color);
    color.end_color = toFColor(end_color);
    color.angle_radians = gradient_angle_radians;
    drawRect(rect, &color, transform);
}

void GLRenderer::drawTexture(GLuint texture, const glm::vec4& dst_rect, const glm::vec4& uv_rect,
                             const ColorOptions* color,
                             const engine::utils::TransformOptions* transform) {
    if (scene_pass_) {
        scene_pass_->queueSprite(texture, true, dst_rect, uv_rect,
                                 color, transform);
    }
}

void GLRenderer::drawTexture(GLuint texture, const glm::vec4& dst_rect,
                             const glm::vec2& texture_size_pixels,
                             const engine::utils::Rect& src_rect_pixels,
                             const ColorOptions* color,
                             const engine::utils::TransformOptions* transform) {
    const glm::vec4 uv_rect = computeUVFromPixels(texture_size_pixels, src_rect_pixels);
    drawTexture(texture, dst_rect, uv_rect, color, transform);
}

void GLRenderer::drawTextureGradient(GLuint texture, const glm::vec4& dst_rect, const glm::vec4& uv_rect,
                                     const glm::vec4& start_color,
                                     const glm::vec4& end_color,
                                     float gradient_angle_radians,
                                     const engine::utils::TransformOptions* transform) {
    ColorOptions color{};
    color.use_gradient = true;
    color.start_color = toFColor(start_color);
    color.end_color = toFColor(end_color);
    color.angle_radians = gradient_angle_radians;
    drawTexture(texture, dst_rect, uv_rect, &color, transform);
}

void GLRenderer::addPointLight(const glm::vec2& pos, float radius,
                               const engine::utils::PointLightOptions* options) {
    if (!point_lights_enabled_) return;
    engine::utils::PointLightOptions defaults{};
    const auto* resolved = options ? options : &defaults;
    lighting_pass_->addPointLight(pos, radius, *resolved);
}

void GLRenderer::addSpotLight(const glm::vec2& pos, float radius,
                      const glm::vec2& dir,
                      const engine::utils::SpotLightOptions* options) {
    if (!spot_lights_enabled_) return;
    engine::utils::SpotLightOptions defaults{};
    const auto* resolved = options ? options : &defaults;
    lighting_pass_->addSpotLight(pos, radius, dir, *resolved);
}

void GLRenderer::addDirectionalLight(const glm::vec2& dir,
                             const engine::utils::DirectionalLightOptions* options) {
    if (!directional_lights_enabled_) return;
    engine::utils::DirectionalLightOptions defaults{};
    const auto* resolved = options ? options : &defaults;
    lighting_pass_->addDirectionalLight(dir, *resolved);
}

void GLRenderer::addEmissiveRect(const glm::vec4& rect,
                                 const engine::utils::EmissiveParams* params) {
    if (!emissive_enabled_ || !emissive_pass_) return;
    engine::utils::EmissiveParams defaults{};
    defaults.color = emissive_pass_->getDefaultColorOptions();
    defaults.transform = emissive_pass_->getDefaultTransformOptions();
    const auto* resolved = params ? params : &defaults;
    emissive_pass_->addRect(rect,
                            resolved->intensity,
                            &resolved->color,
                            &resolved->transform);
}

void GLRenderer::addEmissiveTexture(GLuint texture, const glm::vec4& dst_rect, const glm::vec4& uv_rect,
                                    bool flip_horizontal,
                                    const engine::utils::EmissiveParams* params) {
    if (!emissive_enabled_ || texture == 0 || !emissive_pass_) return;
    engine::utils::EmissiveParams defaults{};
    defaults.color = emissive_pass_->getDefaultColorOptions();
    defaults.transform = emissive_pass_->getDefaultTransformOptions();
    const auto* resolved = params ? params : &defaults;
    engine::utils::TransformOptions transform = resolved->transform;
    if (flip_horizontal) {
        transform.flip_horizontal = !transform.flip_horizontal;
    }
    emissive_pass_->addTexture(texture, dst_rect, uv_rect,
                               resolved->intensity,
                               &resolved->color,
                               &transform);
}

void GLRenderer::drawUIRect(const glm::vec4& rect,
                            const ColorOptions* color,
                            const engine::utils::TransformOptions* transform) {
    if (ui_pass_) {
        ui_pass_->queueSprite(0, false, rect, {0,0,1,1}, color, transform);
    }
}

void GLRenderer::drawUIRectGradient(const glm::vec4& rect,
                                    const glm::vec4& start_color,
                                    const glm::vec4& end_color,
                                    float gradient_angle_radians,
                                    const engine::utils::TransformOptions* transform) {
    ColorOptions color{};
    color.use_gradient = true;
    color.start_color = toFColor(start_color);
    color.end_color = toFColor(end_color);
    color.angle_radians = gradient_angle_radians;
    drawUIRect(rect, &color, transform);
}

void GLRenderer::drawUITexture(GLuint texture, const glm::vec4& dst_rect, const glm::vec4& uv_rect,
                               const ColorOptions* color,
                               const engine::utils::TransformOptions* transform) {
    if (ui_pass_) {
        ui_pass_->queueSprite(texture, true, dst_rect, uv_rect,
                              color, transform);
    }
}

void GLRenderer::drawUITexture(GLuint texture, const glm::vec4& dst_rect,
                               const glm::vec2& texture_size_pixels,
                               const engine::utils::Rect& src_rect_pixels,
                               const ColorOptions* color,
                               const engine::utils::TransformOptions* transform) {
    const glm::vec4 uv_rect = computeUVFromPixels(texture_size_pixels, src_rect_pixels);
    drawUITexture(texture, dst_rect, uv_rect, color, transform);
}

void GLRenderer::drawUITextureGradient(GLuint texture, const glm::vec4& dst_rect, const glm::vec4& uv_rect,
                                       const glm::vec4& start_color,
                                       const glm::vec4& end_color,
                                       float gradient_angle_radians,
                                       const engine::utils::TransformOptions* transform) {
    ColorOptions color{};
    color.use_gradient = true;
    color.start_color = toFColor(start_color);
    color.end_color = toFColor(end_color);
    color.angle_radians = gradient_angle_radians;
    drawUITexture(texture, dst_rect, uv_rect, &color, transform);
}

void GLRenderer::beginFrame(const Camera& camera) {
    current_view_proj_ = computeViewProjection(camera);
    if (viewport_clipping_enabled_) {
        current_view_rect_ = computeCameraViewRect(camera);
    } else {
        current_view_rect_.reset();
    }
}

void GLRenderer::beginDebugUI() {
#ifdef TF_ENABLE_DEBUG_UI
    if (!debug_ui_enabled_) {
        return;
    }
    imgui_layer_->newFrame();
#endif
}

void GLRenderer::endDebugUI() {
#ifdef TF_ENABLE_DEBUG_UI
    if (!debug_ui_enabled_) {
        return;
    }
    if (debug_ui_manager_) {
        debug_ui_manager_->draw();
    }
#endif
}

void GLRenderer::handleSDLEvent(const SDL_Event& event) {
#ifdef TF_ENABLE_DEBUG_UI
    if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat && debug_ui_manager_) {
        const size_t category_count = engine::debug::DebugUIManager::getCategoryCount();
        // F5开始，根据面板类别数量自动扩展按键: F5 + category_index。
        // 例如：Engine(0)->F5，Game(1)->F6。
        // 注意：该映射依赖 PanelCategory 的枚举顺序，新增类别时应尽量末尾追加。
        for (size_t i = 0; i < category_count; ++i) {
            if (event.key.scancode == SDL_SCANCODE_F5 + static_cast<int>(i)) {
                debug_ui_manager_->toggleVisible(static_cast<engine::debug::PanelCategory>(i));
                break;
            }
        }
    }
    imgui_layer_->processEvent(event);
#else
    (void)event;
#endif
}

void GLRenderer::setDebugUIEnabled(bool enabled) {
#ifdef TF_ENABLE_DEBUG_UI
    debug_ui_enabled_ = enabled;
    if (!debug_ui_enabled_ && debug_ui_manager_) {
        const size_t category_count = engine::debug::DebugUIManager::getCategoryCount();
        for (size_t i = 0; i < category_count; ++i) {
            debug_ui_manager_->setVisible(false, static_cast<engine::debug::PanelCategory>(i));
        }
    }
#else
    (void)enabled;
#endif
}

void GLRenderer::setDebugUIManager(engine::debug::DebugUIManager* manager) {
#ifdef TF_ENABLE_DEBUG_UI
    debug_ui_manager_ = manager;
#else
    (void)manager;
#endif
}

void GLRenderer::setVSyncEnabled(bool enabled) {
    if (!render_context_) {
        return;
    }
    (void)render_context_->setSwapInterval(enabled ? 1 : 0);
}

int GLRenderer::getSwapInterval() const {
    if (!render_context_) {
        return -1;
    }
    return render_context_->getSwapInterval();
}

void GLRenderer::clear() {
    // 清空物理窗口（包括信箱区域）以及用于延迟管线的离屏渲染目标。
    // 这样可以确保在开始本帧渲染任务前，每个缓冲区都处于已知的初始状态。
    if (viewport_manager_->dirty()) [[unlikely]] {
        viewport_manager_->update();
    }
    // 先清空默认帧缓冲以填充信箱区域(viewport大小为整个窗口大小)
    const auto& window_size = viewport_manager_->getWindowSize();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, static_cast<int>(std::round(window_size.x)), static_cast<int>(std::round(window_size.y)));
    glClearColor(clear_color_.r, clear_color_.g, clear_color_.b, clear_color_.a);
    glClear(GL_COLOR_BUFFER_BIT);
    // 清空场景FBO中的颜色附件
    scene_pass_->clear(clear_color_);
    // 同时清空光照/发光缓冲
    lighting_pass_->clear();
    emissive_pass_->clear();
    logGlErrors("GLRenderer::clear");
}

void GLRenderer::present() {
    if (viewport_manager_->dirty()) [[unlikely]] {
        viewport_manager_->update();
    }
    auto viewport = viewport_manager_->getViewport();
    engine::utils::Rect logical_vp{};
    logical_vp.pos = {0.0f, 0.0f};
    logical_vp.size = logical_size_;

    for (auto& stats : pass_stats_) {
        stats = {};
    }

    // 渲染管线（坐标空间与帧缓冲）：
    // - Window Pixels：实际 drawable 像素尺寸（glViewport 单位），letterbox viewport 定义在此空间。
    // - Logical：固定逻辑分辨率（离屏渲染目标尺寸，也是 UI 的设计坐标空间）。
    //
    // - Scene/Lighting/Emissive：渲染到各自离屏 FBO（@Logical）
    // - Composite：把离屏结果合成到默认帧缓冲的 letterbox viewport（@Window Pixels）
    // - UI：绘制到默认帧缓冲的 letterbox viewport（@Window Pixels；UI 坐标按 logical 设计映射到 viewport）
    // - ImGui：最后覆盖绘制到默认帧缓冲的整窗区域（@Window Pixels）

    // 1) 刷新场景/光照/发光到各自的离屏 FBO（@Logical）
    scene_pass_->flush(logical_vp);
    pass_stats_[static_cast<size_t>(PassType::Scene)] = {
        scene_pass_->getLastDrawCallCount(),
        scene_pass_->getLastSpriteCount(),
        scene_pass_->getLastVertexCount(),
        scene_pass_->getLastIndexCount()
    };
    // 2) 刷新光照到光照 FBO（@Logical）
    lighting_pass_->flush(
        logical_vp,
        [this](GLint location) { applyViewProjection(location); });
    pass_stats_[static_cast<size_t>(PassType::Lighting)] = {
        lighting_pass_->getLastDrawCallCount(),
        0u,
        lighting_pass_->getLastVertexCount(),
        0u
    };
    // 3) 刷新发光到发光 FBO（@Logical）
    emissive_pass_->flush(logical_vp);
    pass_stats_[static_cast<size_t>(PassType::Emissive)] = {
        emissive_pass_->getLastDrawCallCount(),
        emissive_pass_->getLastSpriteCount(),
        emissive_pass_->getLastVertexCount(),
        emissive_pass_->getLastIndexCount()
    };
    // 4) 执行泛光（@Logical）
    if (bloom_enabled_) {
        const bool has_emissive = emissive_pass_->getLastSpriteCount() > 0;
        if (has_emissive) {
            if (!bloom_pass_->process(emissive_color_tex_)) {
                bloom_pass_->clear();
            }
        } else {
            bloom_pass_->clear();
        }

        const uint32_t bloom_draw_calls = bloom_pass_->getLastDrawCallCount();
        pass_stats_[static_cast<size_t>(PassType::Bloom)] = {
            bloom_draw_calls,
            0u,
            bloom_draw_calls * 6u,
            0u
        };
    }
    // 5) 合成各个通道到默认帧缓冲（@Window Pixels / viewport）
    if (!composite_pass_->render(viewport)) {
        // 如果合成失败，则直接 blit 场景内容
        glBindFramebuffer(GL_READ_FRAMEBUFFER, scene_pass_->getFBO());  // 将 Scene 绑定为“读取”帧缓冲。
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);          // 将 0 (默认帧缓冲) 绑定为“绘制”帧缓冲。
        // 执行像素块传输（Block Image Transfer, or "Blit"）。
        glBlitFramebuffer(
            0, 0,
            static_cast<GLint>(std::round(logical_size_.x)),
            static_cast<GLint>(std::round(logical_size_.y)),
            static_cast<GLint>(std::round(viewport.pos.x)), 
            static_cast<GLint>(std::round(viewport.pos.y)), 
            static_cast<GLint>(std::round(viewport.pos.x + viewport.size.x)), 
            static_cast<GLint>(std::round(viewport.pos.y + viewport.size.y)),
            GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }

    // 6) 合成之后，绘制 UI（@Window Pixels / viewport）
    ui_pass_->flush(viewport);
    pass_stats_[static_cast<size_t>(PassType::UI)] = {
        ui_pass_->getLastDrawCallCount(),
        ui_pass_->getLastSpriteCount(),
        ui_pass_->getLastVertexCount(),
        ui_pass_->getLastIndexCount()
    };

#ifdef TF_ENABLE_DEBUG_UI
    // 7) 如果启用 Debug UI，则在最后渲染 ImGui 界面（@Window Pixels / full window）
    if (debug_ui_enabled_) {
        auto window_size = viewport_manager_->getWindowSize();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(
            0,
            0,
            static_cast<GLsizei>(std::round(window_size.x)),
            static_cast<GLsizei>(std::round(window_size.y))
        );
        imgui_layer_->endFrame();
    }
#endif

    render_context_->swapWindow();
}

void GLRenderer::resize(int width, int height) {
    // 仅更新视口管理器（letterbox），离屏缓冲保持逻辑分辨率
    viewport_manager_->setWindowSize(glm::vec2(width, height));
    viewport_manager_->update();
}

void GLRenderer::clean() {
    if (scene_pass_) scene_pass_->clean();
    if (lighting_pass_) lighting_pass_->clean();
    if (emissive_pass_) emissive_pass_->clean();
    if (bloom_pass_) bloom_pass_->clean();
    if (composite_pass_) composite_pass_->clean();
    if (ui_pass_) ui_pass_->clean();
#ifdef TF_ENABLE_DEBUG_UI
    if (imgui_layer_) imgui_layer_->clean();
#endif
    if (render_context_) render_context_->clean();
}

void GLRenderer::setBloomEnabled(bool enabled) {
    bloom_enabled_ = enabled;
    composite_pass_->setBloomEnabled(enabled);
    if (!bloom_enabled_ && bloom_pass_) {
        bloom_pass_->clear();
    }
}

void GLRenderer::setAmbient(const glm::vec3& ambient) {
    composite_pass_->setAmbient(ambient);
}

void GLRenderer::setBloomStrength(float strength) {
    composite_pass_->setBloomStrength(strength);
}

void GLRenderer::setBloomSigma(float sigma) {
    bloom_pass_->setSigma(sigma);
}

const glm::vec3& GLRenderer::getAmbient() const {
    return composite_pass_->getAmbient();
}

float GLRenderer::getBloomStrength() const {
    return composite_pass_->getBloomStrength();
}

float GLRenderer::getBloomSigma() const {
    return bloom_pass_->getSigma();
}

uint32_t GLRenderer::getBloomLevelCount() const {
    return bloom_pass_ ? bloom_pass_->getLastLevelCount() : 0u;
}

const GLRenderer::PassStats& GLRenderer::getPassStats(PassType pass) const {
    return pass_stats_[static_cast<size_t>(pass)];
}

glm::vec2 GLRenderer::getWindowSizePixels() const {
    if (!viewport_manager_) {
        return {0.0f, 0.0f};
    }
    return viewport_manager_->getWindowSize();
}

engine::utils::Rect GLRenderer::getViewportPixels() const {
    if (!viewport_manager_) {
        return {};
    }
    return viewport_manager_->getViewport();
}

engine::utils::LetterboxMetrics GLRenderer::getLetterboxMetricsPixels() const {
    if (!viewport_manager_) {
        return {};
    }
    return engine::utils::computeLetterboxMetrics(viewport_manager_->getWindowSize(),
                                                  viewport_manager_->getLogicalSize());
}

void GLRenderer::setViewportClippingEnabled(bool enabled) {
    viewport_clipping_enabled_ = enabled;
    if (!viewport_clipping_enabled_) {
        current_view_rect_.reset();
    }
}

bool GLRenderer::shouldCullRect(const engine::utils::Rect& rect) const {
    if (!viewport_clipping_enabled_ || !current_view_rect_.has_value()) {
        return false;
    }

    const auto& view = current_view_rect_.value();

    const float rect_left = std::min(rect.pos.x, rect.pos.x + rect.size.x);
    const float rect_right = std::max(rect.pos.x, rect.pos.x + rect.size.x);
    const float rect_top = std::min(rect.pos.y, rect.pos.y + rect.size.y);
    const float rect_bottom = std::max(rect.pos.y, rect.pos.y + rect.size.y);

    const float view_left = std::min(view.pos.x, view.pos.x + view.size.x);
    const float view_right = std::max(view.pos.x, view.pos.x + view.size.x);
    const float view_top = std::min(view.pos.y, view.pos.y + view.size.y);
    const float view_bottom = std::max(view.pos.y, view.pos.y + view.size.y);

    const bool separated = rect_right <= view_left ||
                           rect_left >= view_right ||
                           rect_bottom <= view_top ||
                           rect_top >= view_bottom;
    return separated;
}

bool GLRenderer::shouldCullCircle(const glm::vec2& center, float radius) const {
    if (radius <= 0.0f) {
        return true;
    }
    if (!viewport_clipping_enabled_) {
        return false;
    }
    engine::utils::Rect bounds(
        center - glm::vec2(radius, radius),
        glm::vec2(radius * 2.0f, radius * 2.0f));
    return shouldCullRect(bounds);
}

void GLRenderer::setSceneDefaultColorOptions(const engine::utils::ColorOptions& options) {
    if (scene_pass_) {
        scene_pass_->setDefaultColorOptions(options);
    }
}

void GLRenderer::setSceneDefaultTransformOptions(const engine::utils::TransformOptions& options) {
    if (scene_pass_) {
        scene_pass_->setDefaultTransformOptions(options);
    }
}

engine::utils::ColorOptions GLRenderer::getSceneDefaultColorOptions() const {
    return scene_pass_ ? scene_pass_->getDefaultColorOptions() : engine::utils::ColorOptions{};
}

engine::utils::TransformOptions GLRenderer::getSceneDefaultTransformOptions() const {
    return scene_pass_ ? scene_pass_->getDefaultTransformOptions() : engine::utils::TransformOptions{};
}

void GLRenderer::setUIDefaultColorOptions(const engine::utils::ColorOptions& options) {
    if (ui_pass_) {
        ui_pass_->setDefaultColorOptions(options);
    }
}

void GLRenderer::setUIDefaultTransformOptions(const engine::utils::TransformOptions& options) {
    if (ui_pass_) {
        ui_pass_->setDefaultTransformOptions(options);
    }
}

engine::utils::ColorOptions GLRenderer::getUIDefaultColorOptions() const {
    return ui_pass_ ? ui_pass_->getDefaultColorOptions() : engine::utils::ColorOptions{};
}

engine::utils::TransformOptions GLRenderer::getUIDefaultTransformOptions() const {
    return ui_pass_ ? ui_pass_->getDefaultTransformOptions() : engine::utils::TransformOptions{};
}

// private methods
bool GLRenderer::initViewportManager() {
    
    // 获取窗口实际像素尺寸（高DPI下可能和Config的窗口大小不一致），未来任何窗口变动，都会通过 onResize() 函数更新视口
    int w,h;
    SDL_GetWindowSizeInPixels(render_context_->window(), &w, &h);
    window_size_pixels_ = glm::vec2(w,h);

    // ViewportManager管理窗口大小。其中逻辑分辨率会自动计算带信箱效果的视口（letterboxed viewport）。
    viewport_manager_ = std::make_unique<ViewportManager>(window_size_pixels_, logical_size_);
    return true;
}

bool GLRenderer::initImGuiLayer() {
#ifdef TF_ENABLE_DEBUG_UI
    imgui_layer_ = ImGuiLayer::create(render_context_->window(), render_context_->context());
    if (!imgui_layer_) {
        spdlog::error("创建 ImGuiLayer 失败。");
        return false;
    }
    return true;
#else
    return true;
#endif
}

bool GLRenderer::initLightingPass() {
    lighting_pass_ = LightingPass::create(*shader_library_, logical_size_); 
    if (!lighting_pass_) {
        spdlog::error("创建 LightingPass 失败。");
        return false;
    }
    light_color_tex_ = lighting_pass_->getColorTex();
    if (light_color_tex_ == 0) {
        spdlog::error("获取光照颜色纹理失败。");
        return false;
    }
    return true;
}

bool GLRenderer::initEmissivePass() {
    emissive_pass_ = EmissivePass::create(*shader_library_, logical_size_);
    if (!emissive_pass_) {
        spdlog::error("创建 EmissivePass 失败。");
        return false;
    }
    emissive_color_tex_ = emissive_pass_->getColorTex();
    if (emissive_color_tex_ == 0) {
        spdlog::error("获取发光颜色纹理失败。");
        return false;
    }
    emissive_pass_->setApplyViewProjection([this](GLint location) { applyViewProjection(location); });
    return true;
}

bool GLRenderer::initBloomPass() {
    bloom_pass_ = BloomPass::create(*shader_library_, logical_size_);
    if (!bloom_pass_) {
        spdlog::error("创建 BloomPass 失败。");
        return false;
    }
    bloom_tex_ = bloom_pass_->getBloomTex();
    if (bloom_tex_ == 0) {
        spdlog::error("获取泛光纹理失败。");
        return false;
    }
    return true;
}

bool GLRenderer::initScenePass() {
    scene_pass_ = ScenePass::create(*shader_library_, logical_size_);
    if (!scene_pass_) {
        return false;
    }
    scene_color_tex_ = scene_pass_->getColorTex();
    if (scene_color_tex_ == 0) {
        spdlog::error("获取场景颜色纹理失败。");
        return false;
    }
    scene_pass_->setApplyViewProjection([this](GLint location) { applyViewProjection(location); });
    return true;
}

bool GLRenderer::initUIPass() {
    ui_pass_ = UIPass::create(*shader_library_, logical_size_);
    if (!ui_pass_) {
        spdlog::error("创建 UIPass 失败。");
        return false;
    }
    return true;
}

bool GLRenderer::initCompositePass() {
    composite_pass_ = CompositePass::create(*shader_library_);
    if (!composite_pass_) {
        spdlog::error("创建 CompositePass 失败。");
        return false;
    }
    // 设置各个输入纹理和参数
    composite_pass_->setSceneTexture(scene_color_tex_);
    composite_pass_->setLightTexture(light_color_tex_);
    composite_pass_->setEmissiveTexture(emissive_color_tex_);
    composite_pass_->setBloomTexture(bloom_tex_);
    return true;
}

std::optional<engine::utils::Rect> GLRenderer::computeCameraViewRect(const Camera& camera) const {
    const float zoom = camera.getZoom();
    if (zoom <= 0.0f) {
        return std::nullopt;
    }

    const float rotation_abs = rotationAbsAroundZero(camera.getRotation());
    constexpr float ROTATION_THRESHOLD = 1e-1f;
    if (rotation_abs > ROTATION_THRESHOLD) {
        return std::nullopt;
    }

    glm::vec2 size = camera.getLogicalSize();
    if (size.x <= 0.0f || size.y <= 0.0f) {
        return std::nullopt;
    }

    size /= zoom;
    const glm::vec2 half_size = size * 0.5f;

    glm::vec2 position = camera.getPosition();
    constexpr float PIXEL_SNAP_ROTATION_EPSILON = 1e-4f;
    if (pixel_snap_enabled_ && rotation_abs <= PIXEL_SNAP_ROTATION_EPSILON) {
        position = glm::round(position * zoom) / zoom;
    }
    const glm::vec2 top_left = position - half_size;
    return engine::utils::Rect(top_left, size);
}

glm::mat4 GLRenderer::computeViewProjection(const Camera& camera) {
    if (logical_size_.x <= 0 || logical_size_.y <= 0) {
        spdlog::error("逻辑尺寸不能为0");
        return glm::mat4(1.0f);
    }

    float zoom = camera.getZoom();
    float rotation = camera.getRotation();
    const float rotation_abs = rotationAbsAroundZero(rotation);
    glm::vec2 position = camera.getPosition();

    constexpr float PIXEL_SNAP_ROTATION_EPSILON = 1e-4f;
    if (pixel_snap_enabled_ && zoom > 0.0f && rotation_abs <= PIXEL_SNAP_ROTATION_EPSILON) {
        position = glm::round(position * zoom) / zoom;
    }

    // 计算投影矩阵，使相机视图中心在原点，这样缩放、旋转均以中心为基准
    glm::mat4 proj = glm::ortho(-logical_size_.x * 0.5f, logical_size_.x * 0.5f,
                                logical_size_.y * 0.5f, -logical_size_.y * 0.5f,
                                -1.0f, 1.0f);

    glm::mat4 view = glm::mat4(1.0f);
    // 1. 首先应用缩放 (Zoom)
    view = glm::scale(view, glm::vec3(zoom, zoom, 1.0f));
    // 2. 其次应用旋转
    if (!glm::epsilonEqual(rotation, 0.0f, glm::epsilon<float>())) {
        view = glm::rotate(view, -rotation, glm::vec3(0.0f, 0.0f, 1.0f));
    }
    // 3. 最后应用平移
    view = glm::translate(view, glm::vec3(-position, 0.0f));
    /* 由于是后乘，实际作用在物体顶点上的顺序是反过来的：先平移（步骤3）→ 再旋转（步骤2）→ 最后缩放（步骤1）。 */

    return proj * view;
}

void GLRenderer::applyViewProjection(GLint location) {
    if (location >= 0) {
        glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(current_view_proj_));
    }
}

} // namespace engine::render::opengl
