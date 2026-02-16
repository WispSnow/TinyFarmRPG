#include "time_clock_ui.h"
#include "game/data/game_time.h"
#include "engine/core/context.h"
#include "engine/resource/resource_manager.h"
#include "engine/ui/ui_panel.h"
#include "engine/ui/ui_image.h"
#include "engine/ui/layout/ui_stack_layout.h"
#include "engine/ui/ui_preset_manager.h"
#include "engine/render/text_renderer.h"
#include <glm/vec2.hpp>
#include <entt/entity/registry.hpp>
#include <entt/core/hashed_string.hpp>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>
#include <format>

namespace {
// 贴图与布局参数（使用缩放因子放大整体UI）
constexpr float UI_SCALE = 2.0f;
constexpr glm::vec2 BASE_PANEL_SIZE{59.0f, 28.0f};
constexpr glm::vec2 BASE_LABEL_SIZE{33.0f, 10.0f};
constexpr glm::vec2 BASE_CLOCK_SIZE{32.0f, 32.0f};
constexpr float BASE_CLOCK_Y_OFFSET = -2.0f;
constexpr float BASE_CLOCK_PANEL_OVERLAP = 12.0f;               // 背景板向左盖住表盘的像素
constexpr float BASE_LABEL_STACK_X = 14.0f;
constexpr float BASE_LABEL_STACK_Y = 3.0f;
constexpr float BASE_LABEL_STACK_SPACING = 2.0f;

const glm::vec2 PANEL_SIZE = BASE_PANEL_SIZE * UI_SCALE;
const glm::vec2 LABEL_SIZE = BASE_LABEL_SIZE * UI_SCALE;
const glm::vec2 CLOCK_SIZE = BASE_CLOCK_SIZE * UI_SCALE;
const float CLOCK_Y_OFFSET = BASE_CLOCK_Y_OFFSET * UI_SCALE;
const float CLOCK_PANEL_OVERLAP = BASE_CLOCK_PANEL_OVERLAP * UI_SCALE;
const float PANEL_OFFSET_X = CLOCK_SIZE.x - CLOCK_PANEL_OVERLAP;
const float LABEL_STACK_X = BASE_LABEL_STACK_X * UI_SCALE;
const float LABEL_STACK_Y = BASE_LABEL_STACK_Y * UI_SCALE;
const float LABEL_STACK_SPACING = BASE_LABEL_STACK_SPACING * UI_SCALE;

// 指针区间：以 10:30 为起点，每180分钟一档
constexpr float SEGMENT_START_MIN = 630.0f; // 10:30
constexpr float MINUTES_PER_SEGMENT = 180.0f;

constexpr entt::hashed_string TIME_PANEL_BG_PRESET{"time_panel_bg"};
constexpr entt::hashed_string TIME_PANEL_LABEL_PRESET{"time_panel_label"};
constexpr entt::hashed_string CLOCK_IMAGE_ID{"time/clock"};

const std::array<entt::hashed_string, 8> HAND_IDS{
    entt::hashed_string{"time/clock_hand_N"},
    entt::hashed_string{"time/clock_hand_NE"},
    entt::hashed_string{"time/clock_hand_E"},
    entt::hashed_string{"time/clock_hand_SE"},
    entt::hashed_string{"time/clock_hand_S"},
    entt::hashed_string{"time/clock_hand_SW"},
    entt::hashed_string{"time/clock_hand_W"},
    entt::hashed_string{"time/clock_hand_NW"}
};

engine::render::Image makePanelFallback() {
    engine::render::Image image(
        "assets/farm-rpg/UI/Clock/Extras.png",
        engine::utils::Rect{glm::vec2{66.0f, 65.0f}, glm::vec2{59.0f, 28.0f}}
    );
    image.setNineSliceMargins(engine::render::NineSliceMargins{1.0f, 3.0f, 1.0f, 1.0f});
    return image;
}

engine::render::Image makeLabelFallback() {
    engine::render::Image image(
        "assets/farm-rpg/UI/Clock/Extras.png",
        engine::utils::Rect{glm::vec2{71.0f, 99.0f}, glm::vec2{33.0f, 10.0f}}
    );
    image.setNineSliceMargins(engine::render::NineSliceMargins{1.0f, 1.0f, 1.0f, 1.0f});
    return image;
}

engine::render::Image makeClockFace() {
    return engine::render::Image(
        "assets/farm-rpg/UI/Clock/Clock.png",
        CLOCK_IMAGE_ID.value(),
        engine::utils::Rect{glm::vec2{0.0f, 0.0f}, BASE_CLOCK_SIZE}
    );
}

engine::render::Image makeClockHand(int index) {
    const float x = static_cast<float>(index) * BASE_CLOCK_SIZE.x;
    return engine::render::Image(
        "assets/farm-rpg/UI/Clock/clock hand.png",
        HAND_IDS.at(static_cast<std::size_t>(index)).value(),
        engine::utils::Rect{glm::vec2{x, 0.0f}, BASE_CLOCK_SIZE}
    );
}

} // namespace

namespace game::ui {

TimeClockUI::TimeClockUI(engine::core::Context& context,
                         entt::registry& registry,
                         engine::render::TextRenderer& text_renderer,
                         std::string_view font_path,
                         int font_size,
                         engine::utils::FColor text_color,
                         glm::vec2 position)
    : UIElement(std::move(position)),
      registry_(registry),
      text_renderer_(text_renderer),
      font_path_(font_path),
      font_size_(font_size),
      text_color_(std::move(text_color)) {
    setAnchor({0.0f, 0.0f}, {0.0f, 0.0f});
    setPivot({0.0f, 0.0f});

    buildImages(context);
    buildLayout();
    refreshFromGameTime();

    spdlog::debug("TimeClockUI 创建完成，位置: ({}, {})", getPosition().x, getPosition().y);
}

void TimeClockUI::update(float delta_time, engine::core::Context& context) {
    UIElement::update(delta_time, context);
    refreshFromGameTime();
}

void TimeClockUI::buildImages(engine::core::Context& context) {
    auto& preset_mgr = context.getResourceManager().getUIPresetManager();

    panel_image_ = makePanelFallback();
    label_image_ = makeLabelFallback();

    if (const auto* preset = preset_mgr.getImagePreset(TIME_PANEL_BG_PRESET.value())) {
        panel_image_ = *preset;
    }
    if (const auto* preset = preset_mgr.getImagePreset(TIME_PANEL_LABEL_PRESET.value())) {
        label_image_ = *preset;
    }

    clock_face_image_ = makeClockFace();
    for (std::size_t i = 0; i < clock_hand_images_.size(); ++i) {
        clock_hand_images_[i] = makeClockHand(static_cast<int>(i));
    }
}

void TimeClockUI::buildLayout() {
    const glm::vec2 root_size{PANEL_OFFSET_X + PANEL_SIZE.x, std::max(PANEL_SIZE.y, CLOCK_SIZE.y)};
    setSize(root_size);

    // 左侧表盘
    auto clock_face = std::make_unique<engine::ui::UIImage>(clock_face_image_, glm::vec2{0.0f, CLOCK_Y_OFFSET}, CLOCK_SIZE);
    clock_face_ = clock_face.get();
    clock_face_->setOrderIndex(1);

    auto clock_hand =
        std::make_unique<engine::ui::UIImage>(clock_hand_images_.front(), glm::vec2{0.0f, CLOCK_Y_OFFSET}, CLOCK_SIZE);
    clock_hand_ = clock_hand.get();
    clock_hand_->setOrderIndex(2);

    // 背景板 + 标签区域
    auto panel = std::make_unique<engine::ui::UIPanel>(glm::vec2{PANEL_OFFSET_X, 0.0f}, PANEL_SIZE, std::nullopt, panel_image_);
    panel->setOrderIndex(0);
    background_panel_ = panel.get();

    auto label_stack = std::make_unique<engine::ui::UIStackLayout>(
        glm::vec2{LABEL_STACK_X, LABEL_STACK_Y},
        glm::vec2{LABEL_SIZE.x, LABEL_SIZE.y * 2.0f + LABEL_STACK_SPACING}
    );
    label_stack->setOrientation(engine::ui::Orientation::Vertical);
    label_stack->setSpacing(LABEL_STACK_SPACING);
    label_stack->setContentAlignment(engine::ui::Alignment::Start);
    label_stack_ = label_stack.get();

    // Day 行
    auto day_bg = std::make_unique<engine::ui::UIPanel>(glm::vec2{0.0f, 0.0f}, LABEL_SIZE, std::nullopt, label_image_);
    auto day_label = std::make_unique<engine::ui::UILabel>(text_renderer_,
                                                           "",
                                                           font_path_,
                                                           font_size_,
                                                           glm::vec2{0.0f, 0.0f},
                                                           text_color_);
    day_label_ = day_label.get();
    day_label_->setOrderIndex(1);
    day_bg->addChild(std::move(day_label));
    day_label_bg_ = day_bg.get();
    label_stack_->addChild(std::move(day_bg));

    // Time 行
    auto time_bg = std::make_unique<engine::ui::UIPanel>(glm::vec2{0.0f, 0.0f}, LABEL_SIZE, std::nullopt, label_image_);
    auto time_label = std::make_unique<engine::ui::UILabel>(text_renderer_,
                                                            "",
                                                            font_path_,
                                                            font_size_,
                                                            glm::vec2{0.0f, 0.0f},
                                                            text_color_);
    time_label_ = time_label.get();
    time_label_->setOrderIndex(1);
    time_bg->addChild(std::move(time_label));
    time_label_bg_ = time_bg.get();
    label_stack_->addChild(std::move(time_bg));

    background_panel_->addChild(std::move(label_stack));

    // 挂载到根节点
    addChild(std::move(panel));
    addChild(std::move(clock_face));
    addChild(std::move(clock_hand));
}

void TimeClockUI::refreshFromGameTime() {
    auto* game_time = registry_.ctx().find<game::data::GameTime>();
    if (!game_time) {
        applyFallbackText();
        setHandImage(0);
        return;
    }

    refreshLabels(*game_time);
    refreshHand(*game_time);
}

void TimeClockUI::refreshLabels(const game::data::GameTime& game_time) {
    const std::string day_text = formatDay(game_time);
    const std::string time_text = formatClock(game_time);

    if (day_text != last_day_text_) {
        updateLabel(day_label_, day_label_bg_, day_text);
        last_day_text_ = day_text;
    }
    if (time_text != last_time_text_) {
        updateLabel(time_label_, time_label_bg_, time_text);
        last_time_text_ = time_text;
    }
}

void TimeClockUI::refreshHand(const game::data::GameTime& game_time) {
    const int index = pickHandIndex(game_time.hour_, game_time.minute_);
    if (index != active_hand_index_) {
        setHandImage(index);
    }
}

void TimeClockUI::updateLabel(engine::ui::UILabel* label, engine::ui::UIPanel* label_bg, const std::string& text) {
    if (!label || !label_bg) return;

    label->setText(text);
    const glm::vec2 text_size = label->getRequestedSize();
    const glm::vec2 offset{
        std::max(0.0f, (LABEL_SIZE.x - text_size.x) * 0.5f),
        std::max(0.0f, (LABEL_SIZE.y - text_size.y) * 0.5f)
    };
    label->setPosition(offset);
}

int TimeClockUI::pickHandIndex(float hour, float minute) const {
    float total_minutes = hour * 60.0f + minute;
    total_minutes = std::fmod(total_minutes, 1440.0f);
    if (total_minutes < 0.0f) {
        total_minutes += 1440.0f;
    }
    float normalized = std::fmod(total_minutes - SEGMENT_START_MIN + 1440.0f, 1440.0f);
    int index = static_cast<int>(std::floor(normalized / MINUTES_PER_SEGMENT));
    return std::clamp(index, 0, 7);
}

std::string TimeClockUI::formatDay(const game::data::GameTime& game_time) const {
    return std::format("Day {}", game_time.day_);
}

std::string TimeClockUI::formatClock(const game::data::GameTime& game_time) const {
    int total_minutes = static_cast<int>(std::lround(game_time.hour_ * 60.0f + game_time.minute_));
    if (total_minutes < 0) {
        total_minutes = 0;
    }
    int hour = (total_minutes / 60) % 24;
    int minute = total_minutes % 60;
    return std::format("{:02d}:{:02d}", hour, minute);
}

void TimeClockUI::setHandImage(int index) {
    if (!clock_hand_) return;
    if (index < 0 || index >= static_cast<int>(clock_hand_images_.size())) return;
    clock_hand_->setImage(clock_hand_images_[static_cast<std::size_t>(index)]);
    active_hand_index_ = index;
}

void TimeClockUI::applyFallbackText() {
    updateLabel(day_label_, day_label_bg_, "Day --");
    updateLabel(time_label_, time_label_bg_, "??:??");
}

void TimeClockUI::setFontPath(std::string_view font_path) {
    font_path_ = font_path;
    if (day_label_) {
        day_label_->setFontPath(font_path_);
    }
    if (time_label_) {
        time_label_->setFontPath(font_path_);
    }
    refreshFromGameTime();
}

void TimeClockUI::setFontSize(int font_size) {
    font_size_ = font_size;
    if (day_label_) {
        day_label_->setFontSize(font_size_);
    }
    if (time_label_) {
        time_label_->setFontSize(font_size_);
    }
    refreshFromGameTime();
}

void TimeClockUI::setTextColor(engine::utils::FColor text_color) {
    text_color_ = std::move(text_color);
    if (day_label_) {
        day_label_->setTextColor(text_color_);
    }
    if (time_label_) {
        time_label_->setTextColor(text_color_);
    }
}

} // namespace game::ui
