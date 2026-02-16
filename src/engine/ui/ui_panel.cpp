#include "ui_panel.h"
#include "engine/core/context.h"
#include "engine/render/renderer.h"
#include "engine/utils/defs.h"
#include <spdlog/spdlog.h>

namespace engine::ui {

UIPanel::UIPanel(glm::vec2 position, glm::vec2 size,
                 std::optional<engine::utils::FColor> background_color,
                 std::optional<engine::render::Image> skin_image,
                 std::optional<engine::render::NineSliceMargins> skin_margins)
    : UIElement(std::move(position), std::move(size)),
      background_color_(std::move(background_color)),
      skin_image_(std::move(skin_image))
{
    spdlog::trace("UIPanel 构造完成。");
    if (skin_image_ && skin_margins) {
        skin_image_->setNineSliceMargins(std::move(skin_margins));
    }
}

void UIPanel::setSkinImage(engine::render::Image image) {
    skin_image_ = std::move(image);
}

void UIPanel::clearSkinImage() {
    skin_image_.reset();
}

void UIPanel::setNineSliceMargins(std::optional<engine::render::NineSliceMargins> margins) {
    if (skin_image_) {
        skin_image_->setNineSliceMargins(std::move(margins));
    }
}

void UIPanel::renderSelf(engine::core::Context& context) {
    if (background_color_) {
        engine::utils::ColorOptions params{};
        params.start_color = *background_color_;
        context.getRenderer().drawUIFilledRect(getBounds(), &params);
    }

    if (skin_image_) {
        const auto size = getLayoutSize();
        if (size.x > 0.0f && size.y > 0.0f) {
            context.getRenderer().drawUIImage(*skin_image_, getScreenPosition(), size);
        }
    }
}

bool UIPanel::hasNineSliceSkin() const {
    return skin_image_.has_value() && skin_image_->hasNineSlice();
}

const engine::render::Image* UIPanel::getSkinImage() const {
    return skin_image_ ? &*skin_image_ : nullptr;
}

const engine::render::NineSliceMargins* UIPanel::getNineSliceMargins() const {
    if (skin_image_ && skin_image_->getNineSliceMargins()) {
        return &*skin_image_->getNineSliceMargins();
    }
    return nullptr;
}

void UIPanel::markNineSliceDirty() {
    if (skin_image_) {
        skin_image_->markNineSliceDirty();
    }
}

} // namespace engine::ui 
