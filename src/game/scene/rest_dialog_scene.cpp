#include "rest_dialog_scene.h"

#include "game/defs/events.h"

#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/render/text_renderer.h"
#include "engine/ui/layout/ui_stack_layout.h"
#include "engine/ui/ui_button.h"
#include "engine/ui/ui_input_blocker.h"
#include "engine/ui/ui_label.h"
#include "engine/ui/ui_manager.h"
#include "engine/ui/ui_panel.h"

#include <spdlog/spdlog.h>

#include <entt/signal/dispatcher.hpp>

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

namespace {

constexpr glm::vec2 ICON_BUTTON_SIZE{32.0f, 32.0f};
constexpr glm::vec2 ACTION_BUTTON_SIZE{128.0f, 32.0f};

constexpr float SECTION_SPACING = 16.0f;
constexpr float TITLE_HEIGHT = 20.0f;

constexpr engine::ui::Thickness PANEL_PADDING{24.0f, 24.0f, 24.0f, 24.0f};

} // namespace

namespace game::scene {

RestDialogScene::RestDialogScene(std::string_view name, engine::core::Context& context)
    : engine::scene::Scene(name, context),
      previous_state_(context.getGameState().getCurrentState()) {
}

bool RestDialogScene::init() {
    previous_state_ = context_.getGameState().getCurrentState();
    context_.getGameState().setState(engine::core::State::Paused);

    if (!initUI()) {
        return false;
    }

    if (!Scene::init()) {
        return false;
    }
    updateHoursLabel();
    return true;
}

void RestDialogScene::clean() {
    context_.getGameState().setState(previous_state_);
    Scene::clean();
}

bool RestDialogScene::initUI() {
    const auto logical_size = context_.getGameState().getLogicalSize();
    ui_manager_ = std::make_unique<engine::ui::UIManager>(context_, logical_size);

    buildLayout();
    return true;
}

void RestDialogScene::buildLayout() {
    const auto logical_size = context_.getGameState().getLogicalSize();
    (void)logical_size;

    auto dim = std::make_unique<engine::ui::UIPanel>(
        glm::vec2{0.0f, 0.0f}, glm::vec2{0.0f, 0.0f}, engine::utils::FColor{0.0f, 0.0f, 0.0f, 0.6f});
    dim->setAnchor({0.0f, 0.0f}, {1.0f, 1.0f});
    dim->setPivot({0.0f, 0.0f});
    dim->setOrderIndex(0);
    dim_ = dim.get();
    ui_manager_->addElement(std::move(dim));

    auto blocker = std::make_unique<engine::ui::UIInputBlocker>(context_, glm::vec2{0.0f, 0.0f}, glm::vec2{0.0f, 0.0f});
    blocker->setAnchor({0.0f, 0.0f}, {1.0f, 1.0f});
    blocker->setPivot({0.0f, 0.0f});
    blocker->setOrderIndex(1);
    input_blocker_ = blocker.get();
    ui_manager_->addElement(std::move(blocker));

    const float content_height = TITLE_HEIGHT + SECTION_SPACING + ICON_BUTTON_SIZE.y + SECTION_SPACING + ACTION_BUTTON_SIZE.y;
    const glm::vec2 panel_size{320.0f, content_height + PANEL_PADDING.height()};

    auto panel =
        std::make_unique<engine::ui::UIPanel>(glm::vec2{0.0f, 0.0f}, panel_size, engine::utils::FColor{0.0f, 0.0f, 0.0f, 0.75f});
    panel->setAnchor({0.5f, 0.5f}, {0.5f, 0.5f});
    panel->setPivot({0.5f, 0.5f});
    panel->setPadding(PANEL_PADDING);
    panel->setOrderIndex(2);
    panel_ = panel.get();

    auto& text_renderer = context_.getTextRenderer();
    auto title = std::make_unique<engine::ui::UILabel>(text_renderer,
                                                       "Rest",
                                                       engine::ui::DEFAULT_UI_FONT_PATH,
                                                       engine::ui::DEFAULT_UI_FONT_SIZE_PX,
                                                       glm::vec2{0.0f, 0.0f},
                                                       engine::utils::FColor{1.0f, 1.0f, 1.0f, 1.0f});
    title->setAnchor({0.5f, 0.0f}, {0.5f, 0.0f});
    title->setPivot({0.5f, 0.0f});
    panel_->addChild(std::move(title));

    const glm::vec2 selector_size{240.0f, ICON_BUTTON_SIZE.y};
    const float selector_y = TITLE_HEIGHT + SECTION_SPACING;

    auto selector = std::make_unique<engine::ui::UIPanel>(glm::vec2{0.0f, selector_y}, selector_size);
    selector->setAnchor({0.5f, 0.0f}, {0.5f, 0.0f});
    selector->setPivot({0.5f, 0.0f});

    auto minus = engine::ui::UIButton::create(
        context_, "page_left", glm::vec2{0.0f, 0.0f}, ICON_BUTTON_SIZE, [this]() { adjustHours(-1); });
    if (minus) {
        minus_button_ = minus.get();
        selector->addChild(std::move(minus));
    } else {
        spdlog::error("RestDialogScene: 创建 minus_button 失败。");
    }

    auto plus = engine::ui::UIButton::create(
        context_,
        "page_right",
        glm::vec2{selector_size.x - ICON_BUTTON_SIZE.x, 0.0f},
        ICON_BUTTON_SIZE,
        [this]() { adjustHours(1); });
    if (plus) {
        plus_button_ = plus.get();
        selector->addChild(std::move(plus));
    } else {
        spdlog::error("RestDialogScene: 创建 plus_button 失败。");
    }

    auto hours_label = std::make_unique<engine::ui::UILabel>(text_renderer,
                                                             "24h",
                                                             engine::ui::DEFAULT_UI_FONT_PATH,
                                                             engine::ui::DEFAULT_UI_FONT_SIZE_PX,
                                                             glm::vec2{selector_size.x * 0.5f, selector_size.y * 0.5f},
                                                             engine::utils::FColor{1.0f, 1.0f, 1.0f, 1.0f});
    hours_label_ = hours_label.get();
    hours_label_->setPivot({0.5f, 0.5f});

    selector->addChild(std::move(hours_label));

    panel_->addChild(std::move(selector));

    const float buttons_y = selector_y + selector_size.y + SECTION_SPACING;
    auto buttons =
        std::make_unique<engine::ui::UIStackLayout>(glm::vec2{0.0f, buttons_y}, glm::vec2{selector_size.x, ACTION_BUTTON_SIZE.y});
    buttons->setOrientation(engine::ui::Orientation::Horizontal);
    buttons->setSpacing(12.0f);
    buttons->setContentAlignment(engine::ui::Alignment::Center);
    buttons->setAnchor({0.5f, 0.0f}, {0.5f, 0.0f});
    buttons->setPivot({0.5f, 0.0f});

    auto confirm = engine::ui::UIButton::create(
        context_, "primary", glm::vec2{0.0f, 0.0f}, ACTION_BUTTON_SIZE, [this]() { onConfirm(); });
    if (confirm) {
        confirm->setLabelText("Confirm");
        confirm_button_ = confirm.get();
        buttons->addChild(std::move(confirm));
    } else {
        spdlog::error("RestDialogScene: 创建 confirm_button 失败。");
    }

    auto cancel = engine::ui::UIButton::create(
        context_, "secondary", glm::vec2{0.0f, 0.0f}, ACTION_BUTTON_SIZE, [this]() { onCancel(); });
    if (cancel) {
        cancel->setLabelText("Cancel");
        cancel_button_ = cancel.get();
        buttons->addChild(std::move(cancel));
    } else {
        spdlog::error("RestDialogScene: 创建 cancel_button 失败。");
    }
    panel_->addChild(std::move(buttons));

    ui_manager_->addElement(std::move(panel));
}

void RestDialogScene::updateHoursLabel() {
    if (!hours_label_) {
        return;
    }
    hours_label_->setText(std::to_string(selected_hours_) + "h");
}

void RestDialogScene::adjustHours(int delta) {
    const int next = std::clamp(selected_hours_ + delta, 1, 24);
    if (next == selected_hours_) {
        return;
    }
    selected_hours_ = next;
    updateHoursLabel();
}

void RestDialogScene::onConfirm() {
    context_.getDispatcher().enqueue(game::defs::AdvanceTimeRequest{selected_hours_});
    // TODO: Dispatch a recovery event (health/stamina) once related systems are implemented.
    requestPopScene();
}

void RestDialogScene::onCancel() {
    requestPopScene();
}

} // namespace game::scene
