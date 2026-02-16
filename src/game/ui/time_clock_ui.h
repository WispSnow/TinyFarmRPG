#pragma once

#include "engine/ui/ui_element.h"
#include "engine/ui/ui_label.h"
#include "engine/render/image.h"
#include <memory>
#include <array>
#include <string>
#include <entt/entity/fwd.hpp>

namespace game::data {
    struct GameTime;
}

namespace engine::core {
    class Context;
}

namespace engine::render {
    class TextRenderer;
}

namespace engine::ui {
    class UIPanel;
    class UIImage;
    class UIStackLayout;
}

namespace game::ui {

/**
 * @brief 时钟HUD：左侧表盘+指针，右侧双行文字（Day/时间）和背景
 */
class TimeClockUI final : public engine::ui::UIElement {
    entt::registry& registry_;                           ///< ECS注册表引用（用于访问GameTime）
    engine::render::TextRenderer& text_renderer_;        ///< 文本渲染器引用
    engine::ui::UIPanel* background_panel_{nullptr};     ///< 背景板
    engine::ui::UIStackLayout* label_stack_{nullptr};    ///< 右侧垂直布局
    engine::ui::UIPanel* day_label_bg_{nullptr};         ///< 天数标签背景
    engine::ui::UIPanel* time_label_bg_{nullptr};        ///< 时间标签背景
    engine::ui::UILabel* day_label_{nullptr};            ///< 天数字符串
    engine::ui::UILabel* time_label_{nullptr};           ///< 时间字符串
    engine::ui::UIImage* clock_face_{nullptr};           ///< 表盘底图
    engine::ui::UIImage* clock_hand_{nullptr};           ///< 指针图层
    engine::render::Image panel_image_;                  ///< 背景图资源
    engine::render::Image label_image_;                  ///< 标签底图资源
    engine::render::Image clock_face_image_;             ///< 表盘资源
    std::array<engine::render::Image, 8> clock_hand_images_{}; ///< 8向指针资源

    std::string font_path_;                              ///< 字体路径
    int font_size_{engine::ui::DEFAULT_UI_FONT_SIZE_PX};  ///< 字体大小
    engine::utils::FColor text_color_{1.0f, 1.0f, 1.0f, 1.0f};  ///< 文本颜色
    int active_hand_index_{-1};                          ///< 当前指针方向
    std::string last_day_text_;                          ///< 上一次渲染的天数字符串
    std::string last_time_text_;                         ///< 上一次渲染的时间字符串

public:
    /**
     * @brief 构造TimeClockUI
     *
     * @param context 上下文引用（用于访问UI预设/资源）
     * @param registry ECS注册表引用（用于访问GameTime上下文）
     * @param text_renderer 文本渲染器引用
     * @param font_path 字体路径
     * @param font_size 字体大小
     * @param text_color 文本颜色
     * @param position 屏幕位置
     */
    TimeClockUI(engine::core::Context& context,
                entt::registry& registry,
                engine::render::TextRenderer& text_renderer,
                std::string_view font_path = engine::ui::DEFAULT_UI_FONT_PATH,
                int font_size = engine::ui::DEFAULT_UI_FONT_SIZE_PX,
                engine::utils::FColor text_color = {1.0f, 1.0f, 1.0f, 1.0f},
                glm::vec2 position = {10.0f, 10.0f});

    ~TimeClockUI() override = default;

    // --- 核心方法 ---
    void update(float delta_time, engine::core::Context& context) override;

    // --- Setters ---
    void setFontPath(std::string_view font_path);
    void setFontSize(int font_size);
    void setTextColor(engine::utils::FColor text_color);

private:
    void buildImages(engine::core::Context& context);
    void buildLayout();
    void refreshFromGameTime();
    void refreshLabels(const game::data::GameTime& game_time);
    void refreshHand(const game::data::GameTime& game_time);
    void updateLabel(engine::ui::UILabel* label, engine::ui::UIPanel* label_bg, const std::string& text);
    [[nodiscard]] int pickHandIndex(float hour, float minute) const;
    [[nodiscard]] std::string formatDay(const game::data::GameTime& game_time) const;
    [[nodiscard]] std::string formatClock(const game::data::GameTime& game_time) const;
    void setHandImage(int index);
    void applyFallbackText();
};

} // namespace game::ui
