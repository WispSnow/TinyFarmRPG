// 拖拽预览层：用于在拖拽过程中显示半透明图标和数量
#pragma once
#include "ui_element.h"
#include "ui_label.h"
#include "engine/render/image.h"
#include <string_view>

namespace engine::ui {

class UIDragPreview final : public UIElement {
private:
    engine::core::Context& context_;
    engine::render::Image image_{};
    UILabel* count_label_{nullptr};
    float alpha_{0.6f};

public:
    UIDragPreview(engine::core::Context& context,
                  std::string_view font_path,
                  int font_size = 16,
                  glm::vec2 size = {0.0f, 0.0f});

    void setContent(const engine::render::Image& image, int count, glm::vec2 slot_size);
    void setAlpha(float alpha);
    void setFontPath(std::string_view font_path);

private:
    void renderSelf(engine::core::Context& context) override;
    void onLayout() override;
};

} // namespace engine::ui
