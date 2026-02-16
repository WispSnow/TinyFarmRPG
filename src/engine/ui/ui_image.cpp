#include "ui_image.h"
#include "engine/render/renderer.h"
#include "engine/render/image.h"
#include "engine/core/context.h"
#include <spdlog/spdlog.h>
#include <entt/core/hashed_string.hpp>

namespace engine::ui {

UIImage::UIImage(std::string_view texture_path,
                 glm::vec2 position,
                 glm::vec2 size,
                 engine::utils::Rect source_rect,
                 bool is_flipped)
    : UIElement(std::move(position), std::move(size)),
      image_(texture_path, source_rect, is_flipped)
{
    if (image_.getTextureId() == entt::null) {
        spdlog::warn("创建了一个空纹理ID的UIImage。 (by texture_path)");
    }
    spdlog::trace("UIImage 构造完成");
}

UIImage::UIImage(entt::id_type texture_id,
                 glm::vec2 position,
                 glm::vec2 size,
                 engine::utils::Rect source_rect,
                 bool is_flipped)
    : UIElement(std::move(position), std::move(size)),
      image_(texture_id, source_rect, is_flipped)
{
    if (image_.getTextureId() == entt::null) {
        spdlog::warn("创建了一个空纹理ID的UIImage。 (by texture_id)");
    }
    spdlog::trace("UIImage 构造完成");
}

UIImage::UIImage(engine::render::Image image,
                 glm::vec2 position,
                 glm::vec2 size)
    : UIElement(std::move(position), std::move(size)),
      image_(std::move(image))
{
    spdlog::trace("UIImage 构造完成");
}

void UIImage::renderSelf(engine::core::Context& context) {
    if (image_.getTextureId() == entt::null) {
        return; // 如果没有分配纹理则不渲染
    }

    auto size = getLayoutSize();
    if (size.x <= 0.0f || size.y <= 0.0f) {
        spdlog::warn("UIImage 尺寸无效 ({}, {})，不渲染。", size.x, size.y);
        return;
    }

    context.getRenderer().drawUIImage(image_, getScreenPosition(), size);
}

} // namespace engine::ui 
