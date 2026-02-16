#include "dialogue_bubble.h"
#include "engine/core/context.h"
#include "engine/resource/resource_manager.h"
#include "engine/ui/ui_manager.h"
#include "engine/ui/ui_preset_manager.h"
#include "engine/render/text_renderer.h"
#include "engine/render/camera.h"
#include <entt/core/hashed_string.hpp>
#include <algorithm>

using namespace entt::literals;

namespace {
constexpr entt::id_type DIALOGUE_PRESET_ID = "dialogue_bubble"_hs;
constexpr float DEFAULT_WIDTH = 160.0f;
constexpr float DEFAULT_HEIGHT = 48.0f;
}

namespace game::ui {

DialogueBubble::DialogueBubble(engine::core::Context& context,
                               entt::dispatcher& dispatcher,
                               engine::render::TextRenderer& text_renderer,
                               std::string_view font_path,
                               int font_size,
                               std::uint8_t channel)
    : UIElement(glm::vec2{0.0f}, glm::vec2{DEFAULT_WIDTH, DEFAULT_HEIGHT}),
      text_renderer_(text_renderer),
      dispatcher_(dispatcher),
      font_path_(font_path),
      font_size_(font_size),
      channel_(channel) {
    setAnchor({0.0f, 0.0f}, {0.0f, 0.0f});
    setPivot({0.5f, 1.0f});
    buildSkin(context);
    buildLayout();
    subscribeEvents();
    setVisible(false);
}

DialogueBubble::~DialogueBubble() {
    dispatcher_.sink<game::defs::DialogueShowEvent>().disconnect<&DialogueBubble::onShowEvent>(this);
    dispatcher_.sink<game::defs::DialogueMoveEvent>().disconnect<&DialogueBubble::onMoveEvent>(this);
    dispatcher_.sink<game::defs::DialogueHideEvent>().disconnect<&DialogueBubble::onHideEvent>(this);
}

void DialogueBubble::buildSkin(engine::core::Context& context) {
    auto& preset_mgr = context.getResourceManager().getUIPresetManager();
    if (const auto* preset = preset_mgr.getImagePreset(DIALOGUE_PRESET_ID)) {
        bubble_image_ = *preset;
    } else {
        // fallback: simple transparent panel
        bubble_image_ = engine::render::Image{};
    }
}

void DialogueBubble::buildLayout() {
    auto panel = std::make_unique<engine::ui::UIPanel>(glm::vec2{0.0f}, getRequestedSize(), std::nullopt, bubble_image_);
    panel->setPivot({0.0f, 0.0f});
    panel_ = panel.get();

    auto label = std::make_unique<engine::ui::UILabel>(
        text_renderer_, "", font_path_, font_size_, glm::vec2{padding_, padding_}, engine::utils::FColor::black());
    label->setPivot({0.0f, 0.0f});
    label_ = label.get();
    label_->setShadowEnabled(false);
    panel_->addChild(std::move(label));
    addChild(std::move(panel));
}

void DialogueBubble::subscribeEvents() {
    dispatcher_.sink<game::defs::DialogueShowEvent>().connect<&DialogueBubble::onShowEvent>(this);
    dispatcher_.sink<game::defs::DialogueMoveEvent>().connect<&DialogueBubble::onMoveEvent>(this);
    dispatcher_.sink<game::defs::DialogueHideEvent>().connect<&DialogueBubble::onHideEvent>(this);
}

void DialogueBubble::setText(std::string_view text) {
    if (label_) {
        label_->setText(text);
        refreshLayoutFromText();
    }
}

void DialogueBubble::refreshLayoutFromText() {
    if (!panel_ || !label_) return;

    const auto label_size = label_->getSize();
    const glm::vec2 desired_size{
        std::max(DEFAULT_WIDTH, label_size.x + padding_ * 2.0f),
        std::max(DEFAULT_HEIGHT, label_size.y + padding_ * 2.0f)
    };
    setSize(desired_size);
    panel_->setSize(desired_size);
}

void DialogueBubble::setWorldPosition(glm::vec2 world_position) {
    world_position_ = world_position;
}

void DialogueBubble::setOffset(glm::vec2 offset) {
    offset_ = offset;
}

void DialogueBubble::update(float delta_time, engine::core::Context& context) {
    UIElement::update(delta_time, context);
    const auto screen_pos = context.getCamera().worldToScreen(world_position_);
    setPosition(screen_pos + offset_);
}

void DialogueBubble::onShowEvent(const game::defs::DialogueShowEvent& evt) {
    if (evt.channel != channel_) return;
    setWorldPosition(evt.world_position);
    // 组合显示：第一行角色名，后续为内容（自动换行）
    std::string wrapped = evt.text;
    // 已知局限：按字符数换行仅适用于等宽 ASCII；CJK/混排文本可能溢出，
    // 需升级为按像素宽度测量（参见课后作业）。
    constexpr std::size_t MAX_CHARS_PER_LINE = 28; // 适配 160px 宽度和当前字体大小
    std::string output;
    output.reserve(wrapped.size() + evt.speaker.size() + 4);
    if (!evt.speaker.empty()) {
        output.append(evt.speaker + ": ");
        output.push_back('\n');
    }
    std::size_t line_len = 0;
    for (std::size_t i = 0; i < wrapped.size(); ++i) {
        char c = wrapped[i];
        output.push_back(c);
        if (c == '\n') {
            line_len = 0;
            continue;
        }
        line_len++;
        if (line_len >= MAX_CHARS_PER_LINE) {
            // 尝试在空格处换行，否则强制换行
            std::size_t back = output.size();
            while (back > 0 && output[back - 1] != ' ' && output[back - 1] != '\n' && (output.size() - back) < MAX_CHARS_PER_LINE) {
                --back;
            }
            if (back > 0 && output[back - 1] == ' ') {
                output[back - 1] = '\n';
            } else {
                output.push_back('\n');
            }
            line_len = 0;
        }
    }
    setText(output);
    setVisible(true);
}

void DialogueBubble::onMoveEvent(const game::defs::DialogueMoveEvent& evt) {
    if (evt.channel != channel_) return;
    setWorldPosition(evt.world_position);
}

void DialogueBubble::onHideEvent(const game::defs::DialogueHideEvent& evt) {
    if (evt.channel != channel_) return;
    setVisible(false);
}

} // namespace game::ui
