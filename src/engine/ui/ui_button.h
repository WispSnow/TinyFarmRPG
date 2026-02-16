#pragma once

#include "ui_interactive.h"
#include "engine/render/nine_slice.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace engine::ui {

enum class UIButtonVisualState : std::uint8_t {
    Normal = 0,
    Hover,
    Pressed,
    Disabled,
    Count
};

struct UIButtonLabelStyle {
    std::string text;
    std::string font_path;
    int font_size{16};
    engine::utils::FColor color{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec2 offset{0.0f, 0.0f};
};

struct UIButtonLabelOverrides {
    std::optional<engine::utils::FColor> color{};
    std::optional<glm::vec2> offset{};
};

struct UIButtonSkin {
    std::optional<engine::render::Image> normal_image{};
    std::optional<engine::render::Image> hover_image{};
    std::optional<engine::render::Image> pressed_image{};
    std::optional<engine::render::Image> disabled_image{};
    std::optional<engine::render::NineSliceMargins> nine_slice_margins{};

    std::optional<UIButtonLabelStyle> normal_label{};
    std::optional<UIButtonLabelOverrides> hover_label{};
    std::optional<UIButtonLabelOverrides> pressed_label{};
    std::optional<UIButtonLabelOverrides> disabled_label{};

    std::unordered_map<entt::id_type, std::string> sound_events{};
};

class UIButton final : public UIInteractive {
private:
    enum class TextLayoutMode : std::uint8_t {
        Fixed = 0,
        ScaleToFit
    };
    static std::optional<UIButtonVisualState> fromStateId(entt::id_type state_id);

    std::function<void()> click_callback_{};
    std::function<void()> hover_enter_callback_{};
    std::function<void()> hover_leave_callback_{};

    entt::id_type preset_id_{entt::null};
    UIButtonVisualState current_visual_state_{UIButtonVisualState::Normal};

    TextLayoutMode text_layout_mode_{TextLayoutMode::Fixed};
    Thickness text_padding_{};

    std::string label_text_{};
    glm::vec2 base_text_size_{0.0f, 0.0f};
    std::uint64_t last_label_layout_revision_{0};
    entt::id_type label_font_id_{entt::null};
    int label_font_size_{0};

public:
    [[nodiscard]] static std::unique_ptr<UIButton> create(engine::core::Context& context,
                                                          entt::id_type preset_id,
                                                          glm::vec2 position = {0.0f, 0.0f},
                                                          glm::vec2 size = {0.0f, 0.0f},
                                                          std::function<void()> click_callback = nullptr,
                                                          std::function<void()> hover_enter_callback = nullptr,
                                                          std::function<void()> hover_leave_callback = nullptr);

    [[nodiscard]] static std::unique_ptr<UIButton> create(engine::core::Context& context,
                                                          std::string_view preset_key,
                                                          glm::vec2 position = {0.0f, 0.0f},
                                                          glm::vec2 size = {0.0f, 0.0f},
                                                          std::function<void()> click_callback = nullptr,
                                                          std::function<void()> hover_enter_callback = nullptr,
                                                          std::function<void()> hover_leave_callback = nullptr);

    ~UIButton() override = default;

    void update(float delta_time, engine::core::Context& context) override;
    void applyStateVisual(entt::id_type state_id) override;

    void clicked() override { if (click_callback_) click_callback_(); }
    void hover_enter() override { if (hover_enter_callback_) hover_enter_callback_(); }
    void hover_leave() override { if (hover_leave_callback_) hover_leave_callback_(); }

    void setLabelText(std::string text);
    [[nodiscard]] std::string_view getLabelText() const { return label_text_; }

    void setTextLayoutFixed();
    void setTextLayoutScaleToFit(const Thickness& padding = {});

private:
    void renderSelf(engine::core::Context& context) override;
    void renderLabel(engine::core::Context& context, const UIButtonSkin& skin,
                     const glm::vec2& position, const glm::vec2& size);

    [[nodiscard]] const UIButtonSkin* getPreset() const;

    UIButton(engine::core::Context& context,
             glm::vec2 position,
             glm::vec2 size,
             std::function<void()> click_callback,
             std::function<void()> hover_enter_callback,
             std::function<void()> hover_leave_callback);

    [[nodiscard]] bool initFromPreset(entt::id_type preset_id);

    void refreshBaseTextSize();
    void refreshBaseTextSizeIfNeeded();
};

} // namespace engine::ui

