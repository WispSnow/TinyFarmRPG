// 拖拽预览层实现：半透明图标+数量文本
#include "ui_drag_preview.h"
#include "engine/core/context.h"
#include "engine/render/renderer.h"
#include "engine/utils/defs.h"
#include <algorithm>
#include <limits>
#include <memory>
#include <spdlog/spdlog.h>
#include <glm/geometric.hpp>

namespace engine::ui {
namespace {
constexpr float DEFAULT_ALPHA = 0.6f;
constexpr glm::vec2 COUNT_PADDING{2.0f, 2.0f};
} // namespace

UIDragPreview::UIDragPreview(engine::core::Context& context,
                             std::string_view font_path,
                             int font_size,
                             glm::vec2 size)
    : UIElement({0.0f, 0.0f}, size),
      context_(context),
      alpha_(DEFAULT_ALPHA) {
    auto label = std::make_unique<UILabel>(context_.getTextRenderer(), "", font_path, font_size);
    label->setVisible(false);
    count_label_ = label.get();
    addChild(std::move(label));

    // 默认居中对齐鼠标
    setAnchor({0.0f, 0.0f}, {0.0f, 0.0f});
    setPivot({0.5f, 0.5f});
    setOrderIndex(std::numeric_limits<int>::max() / 2); // 高优先级渲染
}

void UIDragPreview::setContent(const engine::render::Image& image, int count, glm::vec2 slot_size) {
    image_ = image;
    setSize(slot_size);

    if (count_label_) {
        if (count > 1) {
            count_label_->setText(std::to_string(count));
            count_label_->setVisible(true);
        } else {
            count_label_->setVisible(false);
        }
    }
    invalidateLayout();
}

void UIDragPreview::setAlpha(float alpha) {
    alpha_ = std::clamp(alpha, 0.0f, 1.0f);
}

void UIDragPreview::setFontPath(std::string_view font_path) {
    if (count_label_) {
        count_label_->setFontPath(font_path);
    }
}

void UIDragPreview::renderSelf(engine::core::Context& context) {
    if (image_.getTextureId() == entt::null) {
        spdlog::warn("UIDragPreview: image 无效，跳过绘制。");
        return;
    }
    const auto size = getLayoutSize();
    if (size.x <= 0.0f || size.y <= 0.0f) {
        spdlog::warn("UIDragPreview: 尺寸无效 ({}, {})，跳过绘制。", size.x, size.y);
        return;
    }

    engine::utils::ColorOptions color{};
    color.start_color = engine::utils::FColor{1.0f, 1.0f, 1.0f, alpha_};
    color.end_color = color.start_color;
    color.use_gradient = false;

    context.getRenderer().drawUIImage(image_,
                                      getScreenPosition(),
                                      size,
                                      &color,
                                      nullptr);
}

void UIDragPreview::onLayout() {
    if (!count_label_ || !count_label_->isVisible()) {
        return;
    }

    const glm::vec2 lbl_size = count_label_->getSize();
    glm::vec2 pos = getSize();
    pos -= (lbl_size + COUNT_PADDING);

    if (glm::distance(count_label_->getPosition(), pos) > 0.001f) {
        count_label_->setPosition(pos);
    }
}

} // namespace engine::ui
