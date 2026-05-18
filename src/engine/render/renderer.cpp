#include "renderer.h"
#include "opengl/gl_renderer.h"
#include "engine/resource/default_resource_ids.h"
#include "engine/resource/resource_manager.h"
#include "camera.h"
#include "image.h"
#include "nine_slice.h"
#include <SDL3/SDL.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace engine::render {

std::unique_ptr<Renderer> Renderer::create(engine::render::opengl::GLRenderer* gl_renderer,
                                          engine::resource::ResourceManager* resource_manager) {
    if (gl_renderer == nullptr) {
        spdlog::error("创建 Renderer 失败：提供的 GLRenderer 指针为空。");
        return nullptr;
    }
    if (resource_manager == nullptr) {
        spdlog::error("创建 Renderer 失败：提供的 ResourceManager 指针为空。");
        return nullptr;
    }
    return std::unique_ptr<Renderer>(new Renderer(gl_renderer, resource_manager));
}

// 构造函数: 执行初始化，增加 ResourceManager
Renderer::Renderer(engine::render::opengl::GLRenderer* gl_renderer, engine::resource::ResourceManager* resource_manager)
    : gl_renderer_(gl_renderer), resource_manager_(resource_manager) 
{
    spdlog::trace("构造 Renderer...");
    setClearColorFloat(engine::utils::FColor::black());
    spdlog::trace("Renderer 构造成功。");
}

void Renderer::beginFrame(const Camera& camera) {
    current_camera_ = &camera;
    gl_renderer_->beginFrame(camera);
}

void Renderer::setViewportClippingEnabled(bool enabled) {
    if (gl_renderer_) {
        gl_renderer_->setViewportClippingEnabled(enabled);
    }
}

bool Renderer::isViewportClippingEnabled() const {
    if (!gl_renderer_) {
        return false;
    }
    return gl_renderer_->isViewportClippingEnabled();
}

std::optional<engine::utils::Rect> Renderer::getCurrentViewRect() const {
    if (!gl_renderer_) {
        return std::nullopt;
    }
    return gl_renderer_->getCurrentViewRect();
}

void Renderer::drawSprite(const component::Sprite& sprite,
                          const glm::vec2& position,
                          const glm::vec2& size,
                          const engine::utils::ColorOptions* color_options,
                          const engine::utils::TransformOptions* transform_options) {
    if (shouldCullRect(engine::utils::Rect(position, size))) {
        return;
    }

    auto texture_handle = resource_manager_->getTexture(sprite.texture_id_);
    if (!texture_handle) {
        spdlog::error("无法为 ID {} 获取纹理。", sprite.texture_id_);
        return;
    }

    glm::vec4 dest_rect = {position.x, position.y, size.x, size.y};
    glm::vec4 uv_rect = getSrcRectUV(*texture_handle, sprite.src_rect_);

    engine::utils::TransformOptions resolved_transform = transform_options
        ? *transform_options
        : gl_renderer_->getSceneDefaultTransformOptions();
    resolved_transform.flip_horizontal = sprite.is_flipped_ ^ resolved_transform.flip_horizontal;

    gl_renderer_->drawTexture(texture_handle->texture,
                              dest_rect,
                              uv_rect,
                              color_options,
                              &resolved_transform);
}

void Renderer::drawLine(const glm::vec2& start,
                        const glm::vec2& end,
                        float thickness,
                        const engine::utils::ColorOptions* color_options) {
    if (!gl_renderer_ || thickness <= 0.0f) {
        return;
    }

    const glm::vec2 delta = end - start;
    const float length = glm::length(delta);
    const float half_thickness = thickness * 0.5f;

    // 使用轴对齐包围盒进行视锥裁剪检测
    const glm::vec2 min_point{std::min(start.x, end.x), std::min(start.y, end.y)};
    const glm::vec2 max_point{std::max(start.x, end.x), std::max(start.y, end.y)};
    const engine::utils::Rect cull_rect{
        {min_point.x - half_thickness, min_point.y - half_thickness},
        {std::max(max_point.x - min_point.x, 0.0f) + thickness,
         std::max(max_point.y - min_point.y, 0.0f) + thickness}
    };

    if (shouldCullRect(cull_rect)) {
        return;
    }

    if (length <= std::numeric_limits<float>::epsilon()) {
        // 长度近似为零时退化为小矩形
        const glm::vec4 rect{start.x - half_thickness,
                             start.y - half_thickness,
                             thickness,
                             thickness};
        gl_renderer_->drawRect(rect, color_options, nullptr);
        return;
    }

    gl_renderer_->drawLine(start, end, thickness, color_options);
}

void Renderer::drawCircleOutline(const glm::vec2& center,
                                 float radius,
                                 float thickness,
                                 const engine::utils::ColorOptions* color_options) {
    if (!gl_renderer_ || radius <= 0.0f || thickness <= 0.0f) {
        return;
    }

    const float outer_radius = radius + thickness * 0.5f;
    const engine::utils::Rect bounds{
        {center.x - outer_radius, center.y - outer_radius},
        {outer_radius * 2.0f, outer_radius * 2.0f}
    };
    if (shouldCullRect(bounds)) {
        return;
    }

    constexpr int MIN_SEGMENTS = 24;
    constexpr int MAX_SEGMENTS = 128;
    const float circumference = glm::two_pi<float>() * radius;
    int segment_count = static_cast<int>(circumference / 6.0f); // 约每段 6px
    segment_count = std::clamp(segment_count, MIN_SEGMENTS, MAX_SEGMENTS);

    const float angle_step = glm::two_pi<float>() / static_cast<float>(segment_count);
    glm::vec2 prev_point = center + glm::vec2(radius, 0.0f);

    for (int i = 1; i <= segment_count; ++i) {
        const float angle = angle_step * static_cast<float>(i);
        const glm::vec2 current = center + glm::vec2(std::cos(angle), std::sin(angle)) * radius;
        drawLine(prev_point, current, thickness, color_options);
        prev_point = current;
    }
}

void Renderer::drawFilledCircle(const glm::vec2& position,
                                float radius,
                                const engine::utils::ColorOptions* color_options,
                                const engine::utils::TransformOptions* transform_options) {
    if (shouldCullCircle(position, radius)) {
        return;
    }

    auto circle_texture_handle = resource_manager_->getTexture(engine::resource::defaults::CIRCLE_TEXTURE_ID);
    if (!circle_texture_handle) {
        spdlog::error("无法获取引擎自带的圆形纹理。");
        return;
    }
    glm::vec4 dest_rect = {position.x - radius, position.y - radius, radius * 2.0f, radius * 2.0f};
    glm::vec4 uv_rect = {0.0f, 0.0f, 1.0f, 1.0f};

    engine::utils::TransformOptions resolved_transform = transform_options
        ? *transform_options
        : gl_renderer_->getSceneDefaultTransformOptions();

    gl_renderer_->drawTexture(circle_texture_handle->texture,
                              dest_rect,
                              uv_rect,
                              color_options,
                              &resolved_transform);
}

void Renderer::drawFilledRect(const utils::Rect& rect,
                              const engine::utils::ColorOptions* color_options,
                              const engine::utils::TransformOptions* transform_options) {
    if (shouldCullRect(rect)) {
        return;
    }
    const glm::vec4 rect_vec{rect.pos.x, rect.pos.y, rect.size.x, rect.size.y};
    gl_renderer_->drawRect(rect_vec,
                           color_options,
                           transform_options);
}

void Renderer::drawRect(const glm::vec2& position,
                        const glm::vec2& size,
                        float thickness,
                        const engine::utils::ColorOptions* color_options) {
    if (thickness <= 0 || size.x <= 0.0f || size.y <= 0.0f) {
        return;
    }
    if (shouldCullRect(engine::utils::Rect(position, size))) {
        return;
    }

    const float horizontal_thickness = std::min(thickness, size.y);
    const float vertical_thickness = std::min(thickness, size.x);
    const auto* resolved_color = color_options;

    const float left = position.x;
    const float top = position.y;
    const float right = position.x + size.x;
    const float bottom = position.y + size.y;

    gl_renderer_->drawRect({left, top, size.x, horizontal_thickness}, resolved_color, nullptr);
    if (horizontal_thickness < size.y) {
        gl_renderer_->drawRect({left, bottom - horizontal_thickness, size.x, horizontal_thickness}, resolved_color, nullptr);
    }
    gl_renderer_->drawRect({left, top, vertical_thickness, size.y}, resolved_color, nullptr);
    if (vertical_thickness < size.x) {
        gl_renderer_->drawRect({right - vertical_thickness, top, vertical_thickness, size.y}, resolved_color, nullptr);
    }
}

void Renderer::setClearColorFloat(const engine::utils::FColor& color)
{
    gl_renderer_->setClearColor({color.r, color.g, color.b, color.a});
}

void Renderer::setLightingEnabled(bool enabled) {
    lighting_enabled_ = enabled;
    if (!lighting_enabled_) {
        gl_renderer_->setAmbient({1.0f, 1.0f, 1.0f});
    }
}

void Renderer::clearScreen() {
    gl_renderer_->clear();
}

void Renderer::addPointLight(const glm::vec2& position, float radius,
                             const engine::utils::PointLightOptions* options) {
    if (!lighting_enabled_) {
        return;
    }
    if (shouldCullCircle(position, radius)) {
        return;
    }

    gl_renderer_->addPointLight(position, radius, options);
}

void Renderer::addSpotLight(const glm::vec2& position, float radius, const glm::vec2& direction,
                            const engine::utils::SpotLightOptions* options) {
    if (!lighting_enabled_) {
        return;
    }
    if (shouldCullCircle(position, radius)) {
        return;
    }

    gl_renderer_->addSpotLight(position, radius, direction, options);
}

void Renderer::addDirectionalLight(const glm::vec2& direction,
                                   const engine::utils::DirectionalLightOptions* options) {
    if (!lighting_enabled_) {
        return;
    }
    gl_renderer_->addDirectionalLight(direction, options);
}

void Renderer::setAmbient(const glm::vec3& ambient) {
    if (!lighting_enabled_) {
        return;
    }
    gl_renderer_->setAmbient(ambient);
}

void Renderer::addEmissiveRect(const engine::utils::Rect& rect,
                               const engine::utils::EmissiveParams* params) {
    if (!lighting_enabled_) {
        return;
    }
    if (shouldCullRect(rect)) {
        return;
    }

    glm::vec4 dest_rect{rect.pos.x, rect.pos.y, rect.size.x, rect.size.y};
    gl_renderer_->addEmissiveRect(dest_rect, params);
}

void Renderer::addEmissiveSprite(const component::Sprite& sprite, const glm::vec2& position,
                                 const glm::vec2& size,
                                 const engine::utils::EmissiveParams* params) {
    if (!lighting_enabled_) {
        return;
    }
    if (shouldCullRect(engine::utils::Rect(position, size))) {
        return;
    }

    auto texture_handle = resource_manager_->getTexture(sprite.texture_id_);
    if (!texture_handle) {
        spdlog::error("无法为 ID {} 获取纹理。", sprite.texture_id_);
        return;
    }

    glm::vec4 dest_rect{position.x, position.y, size.x, size.y};
    glm::vec4 uv_rect = getSrcRectUV(*texture_handle, sprite.src_rect_);

    gl_renderer_->addEmissiveTexture(texture_handle->texture, dest_rect, uv_rect,
                                     sprite.is_flipped_, params);
}

void Renderer::present()
{
    gl_renderer_->present();
}

glm::vec4 Renderer::getSrcRectUV(const utils::GL_Texture& texture, const utils::Rect& src_rect) const {
    if (texture.width <= 0.0f || texture.height <= 0.0f) {
        return {0.0f, 0.0f, 1.0f, 1.0f};
    }
    return {
        src_rect.pos.x / texture.width, 
        src_rect.pos.y / texture.height, 
        (src_rect.pos.x + src_rect.size.x) / texture.width, 
        (src_rect.pos.y + src_rect.size.y) / texture.height
    };
}

glm::vec4 Renderer::getSrcRectUV(const utils::GL_Texture& texture, const glm::vec2& position, const glm::vec2& size) const {
    if (texture.width <= 0.0f || texture.height <= 0.0f) {
        return {0.0f, 0.0f, 1.0f, 1.0f};
    }
    return {
        position.x / texture.width, 
        position.y / texture.height, 
        (position.x + size.x) / texture.width, 
        (position.y + size.y) / texture.height,
    };
}

void Renderer::setDefaultWorldColorOptions(const engine::utils::ColorOptions& options) {
    gl_renderer_->setSceneDefaultColorOptions(options);
}

void Renderer::setDefaultWorldTransformOptions(const engine::utils::TransformOptions& options) {
    gl_renderer_->setSceneDefaultTransformOptions(options);
}

engine::utils::ColorOptions Renderer::getDefaultWorldColorOptions() const {
    return gl_renderer_->getSceneDefaultColorOptions();
}

engine::utils::TransformOptions Renderer::getDefaultWorldTransformOptions() const {
    return gl_renderer_->getSceneDefaultTransformOptions();
}

bool Renderer::shouldCullRect(const engine::utils::Rect& rect) const {
    if (!gl_renderer_) {
        return false;
    }
    return gl_renderer_->shouldCullRect(rect);
}

bool Renderer::shouldCullCircle(const glm::vec2& center, float radius) const {
    if (!gl_renderer_) {
        return radius <= 0.0f;
    }
    return gl_renderer_->shouldCullCircle(center, radius);
}

} // namespace engine::render
