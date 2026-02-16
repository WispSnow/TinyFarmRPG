#include "ui_button.h"

#include "state/ui_normal_state.h"
#include "ui_preset_manager.h"

#include "engine/core/context.h"
#include "engine/render/renderer.h"
#include "engine/render/text_renderer.h"
#include "engine/resource/resource_manager.h"
#include "engine/utils/math.h"

#include <entt/core/hashed_string.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <utility>

namespace engine::ui {

namespace {

[[nodiscard]] bool isValidPresetImage(const engine::render::Image& image) {
    return image.getTextureId() != entt::null || !image.getTexturePath().empty();
}

[[nodiscard]] const engine::render::Image* resolvePresetImageForState(const UIButtonSkin& skin, UIButtonVisualState state) {
    auto getImage = [](const std::optional<engine::render::Image>& image) -> const engine::render::Image* {
        if (image && isValidPresetImage(*image)) {
            return &*image;
        }
        return nullptr;
    };

    const engine::render::Image* resolved = nullptr;
    switch (state) {
        case UIButtonVisualState::Normal:
            resolved = getImage(skin.normal_image);
            break;
        case UIButtonVisualState::Hover:
            resolved = getImage(skin.hover_image);
            break;
        case UIButtonVisualState::Pressed:
            resolved = getImage(skin.pressed_image);
            break;
        case UIButtonVisualState::Disabled:
            resolved = getImage(skin.disabled_image);
            break;
        default:
            resolved = nullptr;
            break;
    }

    if (!resolved && state != UIButtonVisualState::Normal) {
        resolved = getImage(skin.normal_image);
    }
    return resolved;
}

struct ResolvedLabelVisual {
    engine::utils::FColor color{engine::utils::FColor::white()};
    glm::vec2 offset{0.0f, 0.0f};
};

[[nodiscard]] ResolvedLabelVisual resolvePresetLabelVisual(const UIButtonSkin& skin, UIButtonVisualState state) {
    ResolvedLabelVisual result{};
    if (!skin.normal_label) {
        return result;
    }

    result.color = skin.normal_label->color;
    result.offset = skin.normal_label->offset;

    auto applyOverrides = [&result](const std::optional<UIButtonLabelOverrides>& overrides) -> bool {
        if (!overrides) {
            return false;
        }
        if (overrides->color) {
            result.color = *overrides->color;
        }
        if (overrides->offset) {
            result.offset = *overrides->offset;
        }
        return true;
    };

    if (state == UIButtonVisualState::Hover) {
        if (applyOverrides(skin.hover_label)) {
            return result;
        }
        result.color = engine::utils::brightenColor(result.color, 1.15f);
        return result;
    }

    if (state == UIButtonVisualState::Pressed) {
        if (applyOverrides(skin.pressed_label)) {
            return result;
        }
        result.offset = glm::vec2{0.0f, 2.0f};
        return result;
    }

    if (state == UIButtonVisualState::Disabled) {
        if (applyOverrides(skin.disabled_label)) {
            return result;
        }
        result.color = engine::utils::greyscaleColor(result.color);
        return result;
    }

    return result;
}

} // namespace

std::unique_ptr<UIButton> UIButton::create(engine::core::Context& context,
                                          entt::id_type preset_id,
                                          glm::vec2 position,
                                          glm::vec2 size,
                                          std::function<void()> click_callback,
                                          std::function<void()> hover_enter_callback,
                                          std::function<void()> hover_leave_callback) {
    auto button = std::unique_ptr<UIButton>(new UIButton(context,
                                                         position,
                                                         size,
                                                         std::move(click_callback),
                                                         std::move(hover_enter_callback),
                                                         std::move(hover_leave_callback)));
    if (!button->initFromPreset(preset_id)) {
        return nullptr;
    }
    return button;
}

std::unique_ptr<UIButton> UIButton::create(engine::core::Context& context,
                                          std::string_view preset_key,
                                          glm::vec2 position,
                                          glm::vec2 size,
                                          std::function<void()> click_callback,
                                          std::function<void()> hover_enter_callback,
                                          std::function<void()> hover_leave_callback) {
    if (preset_key.empty()) {
        spdlog::error("创建 UIButton 失败：preset_key 不能为空。");
        return nullptr;
    }

    const entt::id_type preset_id = entt::hashed_string{preset_key.data(), preset_key.size()}.value();
    return create(context,
                  preset_id,
                  position,
                  size,
                  std::move(click_callback),
                  std::move(hover_enter_callback),
                  std::move(hover_leave_callback));
}

UIButton::UIButton(engine::core::Context& context,
                   glm::vec2 position,
                   glm::vec2 size,
                   std::function<void()> click_callback,
                   std::function<void()> hover_enter_callback,
                   std::function<void()> hover_leave_callback)
    : UIInteractive(context, position, size),
      click_callback_(std::move(click_callback)),
      hover_enter_callback_(std::move(hover_enter_callback)),
      hover_leave_callback_(std::move(hover_leave_callback)) {}

const UIButtonSkin* UIButton::getPreset() const {
    return context_.getResourceManager().getUIPresetManager().getButtonPreset(preset_id_);
}

bool UIButton::initFromPreset(entt::id_type preset_id) {
    preset_id_ = preset_id;

    const auto* preset = getPreset();
    if (!preset) {
        spdlog::error("创建 UIButton 失败：未找到 UI 预设 (id={})。", preset_id_);
        return false;
    }
    if (!preset->normal_image || !isValidPresetImage(*preset->normal_image)) {
        spdlog::error("创建 UIButton 失败：预设缺少可用的 normal image (id={})。", preset_id_);
        return false;
    }

    clearSoundOverrides();
    for (const auto& [event_id, path] : preset->sound_events) {
        setSoundEvent(event_id, path);
    }

    if (label_text_.empty() && preset->normal_label && !preset->normal_label->text.empty()) {
        label_text_ = preset->normal_label->text;
    }

    if (size_.x == 0.0f && size_.y == 0.0f) {
        glm::vec2 image_size = preset->normal_image->getSourceRect().size;
        if (image_size.x <= 0.0f || image_size.y <= 0.0f) {
            image_size = context_.getResourceManager().getTextureSize(
                preset->normal_image->getTextureId(),
                preset->normal_image->getTexturePath());
        }
        if (image_size.x > 0.0f && image_size.y > 0.0f) {
            setSizeInternal(image_size);
        }
    }

    refreshBaseTextSize();
    setState(std::make_unique<engine::ui::state::UINormalState>(this));
    return true;
}

void UIButton::update(float delta_time, engine::core::Context& context) {
    UIInteractive::update(delta_time, context);
    refreshBaseTextSizeIfNeeded();
}

void UIButton::applyStateVisual(entt::id_type state_id) {
    if (const auto state = fromStateId(state_id)) {
        current_visual_state_ = *state;
    }
}

void UIButton::setLabelText(std::string text) {
    label_text_ = std::move(text);
    refreshBaseTextSize();
}

void UIButton::setTextLayoutFixed() {
    text_layout_mode_ = TextLayoutMode::Fixed;
}

void UIButton::setTextLayoutScaleToFit(const Thickness& padding) {
    text_layout_mode_ = TextLayoutMode::ScaleToFit;
    text_padding_ = padding;
}

void UIButton::refreshBaseTextSizeIfNeeded() {
    if (label_text_.empty()) {
        return;
    }

    auto& text_renderer = context_.getTextRenderer();
    const auto revision = text_renderer.getLayoutRevision();
    if (revision != last_label_layout_revision_) {
        refreshBaseTextSize();
        return;
    }

    const auto* preset = getPreset();
    if (!preset || !preset->normal_label) {
        return;
    }

    const auto& label = *preset->normal_label;
    const entt::id_type font_id = entt::hashed_string{label.font_path.c_str(), label.font_path.size()}.value();
    if (font_id != label_font_id_ || label.font_size != label_font_size_) {
        refreshBaseTextSize();
    }
}

void UIButton::refreshBaseTextSize() {
    auto& text_renderer = context_.getTextRenderer();
    last_label_layout_revision_ = text_renderer.getLayoutRevision();

    base_text_size_ = {0.0f, 0.0f};
    label_font_id_ = entt::null;
    label_font_size_ = 0;

    if (label_text_.empty()) {
        return;
    }

    const auto* preset = getPreset();
    if (!preset || !preset->normal_label) {
        return;
    }

    const auto& label = *preset->normal_label;
    if (label.font_path.empty() || label.font_size <= 0) {
        return;
    }

    label_font_id_ = entt::hashed_string{label.font_path.c_str(), label.font_path.size()}.value();
    if (label_font_id_ == entt::null) {
        return;
    }
    label_font_size_ = label.font_size;

    const auto default_style = text_renderer.getDefaultUIStyleId();
    const auto layout = text_renderer.getTextStyle(default_style).layout;
    base_text_size_ = text_renderer.getTextSize(label_text_, label_font_id_, label_font_size_, label.font_path, &layout);
}

std::optional<UIButtonVisualState> UIButton::fromStateId(entt::id_type state_id) {
    if (state_id == UI_IMAGE_NORMAL_ID) return UIButtonVisualState::Normal;
    if (state_id == UI_IMAGE_HOVER_ID) return UIButtonVisualState::Hover;
    if (state_id == UI_IMAGE_PRESSED_ID) return UIButtonVisualState::Pressed;
    if (state_id == UI_IMAGE_DISABLED_ID) return UIButtonVisualState::Disabled;
    return std::nullopt;
}

void UIButton::renderSelf(engine::core::Context& context) {
    const glm::vec2 size = getLayoutSize();
    if (size.x <= 0.0f || size.y <= 0.0f) {
        return;
    }

    const auto* skin = getPreset();
    if (!skin) {
        return;
    }

    const auto* image_to_draw = resolvePresetImageForState(*skin, current_visual_state_);
    if (!image_to_draw) {
        return;
    }

    const glm::vec2 position = getScreenPosition();
    context.getRenderer().drawUIImage(*image_to_draw, position, size);

    renderLabel(context, *skin, position, size);
}

void UIButton::renderLabel(engine::core::Context& context, const UIButtonSkin& skin,
                           const glm::vec2& position, const glm::vec2& size) {
    if (label_text_.empty() || !skin.normal_label || label_font_id_ == entt::null || label_font_size_ <= 0) {
        return;
    }

    auto& text_renderer = context.getTextRenderer();
    const auto default_style = text_renderer.getDefaultUIStyleId();
    const auto base_glyph_scale = text_renderer.getTextStyle(default_style).layout.glyph_scale;

    const auto visual = resolvePresetLabelVisual(skin, current_visual_state_);
    engine::utils::TextRenderOverrides overrides{};
    overrides.color = visual.color;

    glm::vec2 draw_position{0.0f, 0.0f};
    glm::vec2 text_size = base_text_size_;
    bool can_draw_text = base_text_size_.x > 0.0f && base_text_size_.y > 0.0f;

    if (text_layout_mode_ == TextLayoutMode::ScaleToFit) {
        glm::vec2 available{
            std::max(0.0f, size.x - text_padding_.width()),
            std::max(0.0f, size.y - text_padding_.height())
        };
        if (available.x <= 0.0f || available.y <= 0.0f || !can_draw_text) {
            can_draw_text = false;
        } else {
            const float scale = std::min(available.x / base_text_size_.x, available.y / base_text_size_.y);
            if (scale <= 0.0f) {
                can_draw_text = false;
            } else {
                overrides.glyph_scale = glm::vec2{base_glyph_scale.x * scale, base_glyph_scale.y * scale};
                text_size = base_text_size_ * scale;
                glm::vec2 content_origin = position + glm::vec2{text_padding_.left, text_padding_.top};
                glm::vec2 centered_offset{
                    std::max(0.0f, (available.x - text_size.x) * 0.5f),
                    std::max(0.0f, (available.y - text_size.y) * 0.5f)
                };
                draw_position = content_origin + centered_offset + visual.offset;
            }
        }
    } else {
        if (can_draw_text) {
            glm::vec2 centered_offset{
                std::max(0.0f, (size.x - text_size.x) * 0.5f),
                std::max(0.0f, (size.y - text_size.y) * 0.5f)
            };
            draw_position = position + centered_offset + visual.offset;
        }
    }

    if (!can_draw_text) {
        return;
    }

    text_renderer.drawUIText(label_text_, label_font_id_, label_font_size_, draw_position, default_style, &overrides);
}

} // namespace engine::ui

