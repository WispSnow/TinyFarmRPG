#include "title_scene.h"

#include "game_scene.h"
#include "pause_menu_scene.h"
#include "save_slot_select_scene.h"

#include "game/defs/audio_ids.h"
#include "game/data/game_time.h"

#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/audio/audio_player.h"
#include "engine/render/text_renderer.h"
#include "engine/resource/resource_manager.h"
#include "engine/ui/layout/ui_stack_layout.h"
#include "engine/ui/ui_button.h"
#include "engine/ui/ui_image.h"
#include "engine/ui/ui_label.h"
#include "engine/ui/ui_manager.h"

#include <entt/core/hashed_string.hpp>
#include <spdlog/spdlog.h>
#include <cmath>
#include <memory>
#include <utility>

namespace {

constexpr entt::hashed_string TITLE_BG_ID{"title-bg"};
constexpr entt::hashed_string TITLE_LOGO_ID{"title-logo"};
constexpr int MUSIC_FADE_IN_MS = 200;

constexpr float LOGO_FLOAT_AMPLITUDE = 5.0f;
constexpr float LOGO_FLOAT_SPEED = 2.0f;

constexpr float BUTTON_WIDTH = 96.0f;
constexpr float BUTTON_HEIGHT = 32.0f;
constexpr float BUTTON_SPACING = 16.0f;

} // namespace

namespace game::scene {

TitleScene::TitleScene(std::string_view name, engine::core::Context& context, std::string error_message)
    : engine::scene::Scene(name, context),
      error_message_(std::move(error_message)) {
}

bool TitleScene::init() {
    context_.getGameState().setState(engine::core::State::Title);

    title_game_time_ = game::data::GameTime::loadFromConfig("assets/data/game_time_config.json");
    if (!title_game_time_) {
        spdlog::warn("TitleScene: GameTime 加载失败，使用默认配置。");
        title_game_time_ = std::make_shared<game::data::GameTime>();
    }

    if (!initUI()) return false;
    if (!Scene::init()) return false;
    context_.getAudioPlayer().playMusic(game::defs::audio::TITLE_BG_MUSIC_ID.value(), true, MUSIC_FADE_IN_MS);
    return true;
}

void TitleScene::update(float delta_time) {
    logo_elapsed_ += delta_time;
    if (logo_) {
        const float y = logo_base_pos_.y + std::sin(logo_elapsed_ * LOGO_FLOAT_SPEED) * LOGO_FLOAT_AMPLITUDE;
        logo_->setPosition({logo_base_pos_.x, y});
    }
    Scene::update(delta_time);
}

bool TitleScene::initUI() {
    const auto logical_size = context_.getGameState().getLogicalSize();
    ui_manager_ = std::make_unique<engine::ui::UIManager>(context_, logical_size);

    buildLayout();
    return true;
}

void TitleScene::buildLayout() {
    buildBackground();
    buildLogo();
    buildErrorLabel();
    buildButtons();
    buildMenuButton();
}

void TitleScene::buildBackground() {
    const auto src_size = context_.getResourceManager().getTextureSize(TITLE_BG_ID);
    const auto logical_size = context_.getGameState().getLogicalSize();
    auto bg = std::make_unique<engine::ui::UIImage>(TITLE_BG_ID.value(),
                                                    glm::vec2{0.0f, 0.0f},
                                                    logical_size,
                                                    engine::utils::Rect{0.0f, 0.0f, src_size.x, src_size.y});
    bg->setAnchor({0.0f, 0.0f}, {1.0f, 1.0f});
    bg->setPivot({0.0f, 0.0f});
    bg->setOrderIndex(0);
    background_ = bg.get();
    ui_manager_->addElement(std::move(bg));
}

void TitleScene::buildLogo() {
    const auto src_size = context_.getResourceManager().getTextureSize(TITLE_LOGO_ID);
    glm::vec2 logo_size = src_size * 0.5f;
    auto logo = std::make_unique<engine::ui::UIImage>(TITLE_LOGO_ID.value(),
                                                      glm::vec2{0.0f, 0.0f},
                                                      logo_size,
                                                      engine::utils::Rect{0.0f, 0.0f, src_size.x, src_size.y});
    logo->setAnchor({0.5f, 0.32f}, {0.5f, 0.32f});
    logo->setPivot({0.5f, 0.5f});
    logo->setOrderIndex(1);
    logo_base_pos_ = {0.0f, 0.0f};
    logo_ = logo.get();
    ui_manager_->addElement(std::move(logo));
}

void TitleScene::buildButtons() {
    const float total_height = BUTTON_HEIGHT * 3.0f + BUTTON_SPACING * 2.0f;

    auto stack = std::make_unique<engine::ui::UIStackLayout>(glm::vec2{0.0f, 0.0f}, glm::vec2{BUTTON_WIDTH, total_height});
    stack->setOrientation(engine::ui::Orientation::Vertical);
    stack->setSpacing(BUTTON_SPACING);
    stack->setAnchor({0.5f, 0.55f}, {0.5f, 0.55f});
    stack->setPivot({0.5f, 0.0f});
    stack->setOrderIndex(2);

    auto start_button = engine::ui::UIButton::create(
        context_, "secondary", glm::vec2{0.0f, 0.0f}, glm::vec2{BUTTON_WIDTH, BUTTON_HEIGHT}, [this]() { onStartClicked(); });
    if (!start_button) {
        spdlog::error("TitleScene: 创建 start_button 失败。");
        return;
    }
    start_button->setLabelText("Start");

    auto load_button = engine::ui::UIButton::create(
        context_, "secondary", glm::vec2{0.0f, 0.0f}, glm::vec2{BUTTON_WIDTH, BUTTON_HEIGHT}, [this]() { onLoadClicked(); });
    if (!load_button) {
        spdlog::error("TitleScene: 创建 load_button 失败。");
        return;
    }
    load_button->setLabelText("Load");

    auto exit_button = engine::ui::UIButton::create(
        context_, "secondary", glm::vec2{0.0f, 0.0f}, glm::vec2{BUTTON_WIDTH, BUTTON_HEIGHT}, [this]() { onExitClicked(); });
    if (!exit_button) {
        spdlog::error("TitleScene: 创建 exit_button 失败。");
        return;
    }
    exit_button->setLabelText("Exit");

    stack->addChild(std::move(start_button));
    stack->addChild(std::move(load_button));
    stack->addChild(std::move(exit_button));

    button_stack_ = stack.get();
    ui_manager_->addElement(std::move(stack));
}

void TitleScene::buildMenuButton() {
    auto menu_button = engine::ui::UIButton::create(
        context_, "menu", glm::vec2{-10.0f, 10.0f}, glm::vec2{32.0f, 32.0f}, [this]() { onMenuClicked(); });
    if (!menu_button) {
        spdlog::error("TitleScene: 创建菜单按钮失败，UI初始化将继续。");
        return;
    }
    menu_button->setAnchor({1.0f, 0.0f}, {1.0f, 0.0f});
    menu_button->setPivot({1.0f, 0.0f});
    menu_button->setOrderIndex(3);
    ui_manager_->addElement(std::move(menu_button));
}

void TitleScene::buildErrorLabel() {
    auto& text_renderer = context_.getTextRenderer();
    auto label = std::make_unique<engine::ui::UILabel>(text_renderer,
                                                       error_message_,
                                                       engine::ui::DEFAULT_UI_FONT_PATH,
                                                       engine::ui::DEFAULT_UI_FONT_SIZE_PX,
                                                       glm::vec2{0.0f, 0.0f},
                                                       engine::utils::FColor{1.0f, 0.25f, 0.25f, 1.0f});
    label->setAnchor({0.5f, 0.50f}, {0.5f, 0.50f});
    label->setPivot({0.5f, 0.5f});
    label->setOrderIndex(2);
    label->setVisible(!error_message_.empty());

    error_label_ = label.get();
    ui_manager_->addElement(std::move(label));
}

void TitleScene::onStartClicked() {
    auto next = std::make_unique<game::scene::GameScene>("GameScene", context_, title_game_time_);
    requestReplaceScene(std::move(next));
}

void TitleScene::onLoadClicked() {
    auto on_select = [this](int slot) {
        auto next = std::make_unique<game::scene::GameScene>("GameScene", context_, nullptr, slot);
        requestReplaceScene(std::move(next));
    };

    auto menu = std::make_unique<game::scene::SaveSlotSelectScene>("SaveSlotSelect", context_, std::move(on_select));
    requestPushScene(std::move(menu));
}

void TitleScene::onMenuClicked() {
    auto menu = std::make_unique<game::scene::PauseMenuScene>("PauseMenu", context_, nullptr, title_game_time_.get());
    requestPushScene(std::move(menu));
}

void TitleScene::onExitClicked() {
    quit();
}

} // namespace game::scene
