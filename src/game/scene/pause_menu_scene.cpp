#include "pause_menu_scene.h"

#include "save_slot_select_scene.h"
#include "title_scene.h"

#include "game/data/game_time.h"
#include "game/save/save_service.h"

#include "engine/audio/audio_player.h"
#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/input/input_manager.h"
#include "engine/render/text_renderer.h"
#include "engine/ui/layout/ui_stack_layout.h"
#include "engine/ui/ui_button.h"
#include "engine/ui/ui_input_blocker.h"
#include "engine/ui/ui_label.h"
#include "engine/ui/ui_manager.h"
#include "engine/ui/ui_panel.h"

#include <entt/core/hashed_string.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <memory>
#include <string>
#include <utility>

using namespace entt::literals;

namespace {

constexpr glm::vec2 BUTTON_SIZE{160.0f, 32.0f};
constexpr float BUTTON_SPACING = 6.0f;

constexpr glm::vec2 ICON_BUTTON_SIZE{32.0f, 32.0f};
constexpr float SECTION_SPACING = 20.0f;
constexpr float VOLUME_ROW_SPACING = 6.0f;

constexpr engine::ui::Thickness PANEL_PADDING{24.0f, 24.0f, 24.0f, 24.0f};

constexpr float MESSAGE_TOP_MARGIN = 4.0f;
constexpr float MESSAGE_AREA_HEIGHT = 22.0f;

constexpr float VOLUME_STEP = 0.10f;
constexpr float TIME_SCALE_MIN = 0.01f;
constexpr float TIME_SCALE_MAX = 100.0f;
constexpr float TIME_SCALE_STEP_RATIO_EXP = 0.2f;

float clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

std::string toPercentLabel(std::string_view prefix, float value) {
    const int percent = static_cast<int>(std::lround(clamp01(value) * 100.0f));
    return std::string(prefix) + " " + std::to_string(percent) + "%";
}

std::string toMultiplierLabel(std::string_view prefix, float value) {
    const float clamped = std::clamp(value, TIME_SCALE_MIN, TIME_SCALE_MAX);
    char buffer[64]{};
    std::snprintf(buffer, sizeof(buffer), "%.2fx", clamped);
    return std::string(prefix) + " " + buffer;
}

} // namespace

namespace game::scene {

PauseMenuScene::PauseMenuScene(std::string_view name,
                               engine::core::Context& context,
                               game::save::SaveService* save_service,
                               game::data::GameTime* game_time)
    : engine::scene::Scene(name, context),
      save_service_(save_service),
      game_time_(game_time),
      previous_state_(context.getGameState().getCurrentState()) {
}

PauseMenuScene::~PauseMenuScene() {
    context_.getInputManager().onAction("pause"_hs).disconnect<&PauseMenuScene::onPausePressed>(this);
}

bool PauseMenuScene::init() {
    previous_state_ = context_.getGameState().getCurrentState();
    context_.getGameState().setState(engine::core::State::Paused);

    if (!initUI()) return false;
    context_.getInputManager().onAction("pause"_hs).connect<&PauseMenuScene::onPausePressed>(this);

    if (!Scene::init()) return false;
    return true;
}

void PauseMenuScene::update(float delta_time) {
    if (close_after_load_) {
        close_after_load_ = false;
        requestPopScene();
    }
    Scene::update(delta_time);
}

void PauseMenuScene::clean() {
    context_.getGameState().setState(previous_state_);
    Scene::clean();
}

bool PauseMenuScene::initUI() {
    const auto logical_size = context_.getGameState().getLogicalSize();
    ui_manager_ = std::make_unique<engine::ui::UIManager>(context_, logical_size);

    buildLayout();
    refreshVolumeLabels();
    refreshTimeScaleLabel();
    return true;
}

void PauseMenuScene::buildLayout() {
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

    const float buttons_height = BUTTON_SIZE.y * 4.0f + BUTTON_SPACING * 3.0f;
    const float volume_height = ICON_BUTTON_SIZE.y * 3.0f + VOLUME_ROW_SPACING * 2.0f;
    const float content_height = MESSAGE_AREA_HEIGHT + buttons_height + SECTION_SPACING + volume_height;
    const glm::vec2 panel_size{BUTTON_SIZE.x + PANEL_PADDING.width(), content_height + PANEL_PADDING.height()};

    auto panel =
        std::make_unique<engine::ui::UIPanel>(glm::vec2{0.0f, 0.0f}, panel_size, engine::utils::FColor{0.0f, 0.0f, 0.0f, 0.75f});
    panel->setAnchor({0.5f, 0.5f}, {0.5f, 0.5f});
    panel->setPivot({0.5f, 0.5f});
    panel->setPadding(PANEL_PADDING);
    panel->setOrderIndex(2);

    auto& text_renderer = context_.getTextRenderer();
    auto message = std::make_unique<engine::ui::UILabel>(text_renderer,
                                                         "",
                                                         engine::ui::DEFAULT_UI_FONT_PATH,
                                                         engine::ui::DEFAULT_UI_FONT_SIZE_PX,
                                                         glm::vec2{0.0f, MESSAGE_TOP_MARGIN},
                                                         engine::utils::FColor{1.0f, 0.25f, 0.25f, 1.0f});
    message->setAnchor({0.5f, 0.0f}, {0.5f, 0.0f});
    message->setPivot({0.5f, 0.0f});
    message->setVisible(false);
    message_label_ = message.get();
    panel->addChild(std::move(message));

    auto buttons = std::make_unique<engine::ui::UIStackLayout>(
        glm::vec2{0.0f, MESSAGE_AREA_HEIGHT},
        glm::vec2{BUTTON_SIZE.x, buttons_height});
    buttons->setOrientation(engine::ui::Orientation::Vertical);
    buttons->setSpacing(BUTTON_SPACING);
    buttons->setAnchor({0.0f, 0.0f}, {0.0f, 0.0f});
    buttons->setPivot({0.0f, 0.0f});

    auto resume_button = engine::ui::UIButton::create(
        context_, "primary", glm::vec2{0.0f, 0.0f}, BUTTON_SIZE, [this]() { onResumeClicked(); });
    if (resume_button) {
        resume_button->setLabelText("Resume");
        resume_button_ = resume_button.get();
        buttons->addChild(std::move(resume_button));
    } else {
        spdlog::error("PauseMenuScene: 创建 resume_button 失败。");
    }

    auto save_button = engine::ui::UIButton::create(
        context_, "primary", glm::vec2{0.0f, 0.0f}, BUTTON_SIZE, [this]() { onSaveClicked(); });
    if (save_button) {
        save_button->setLabelText("Save");
        save_button_ = save_button.get();
        buttons->addChild(std::move(save_button));
    } else {
        spdlog::error("PauseMenuScene: 创建 save_button 失败。");
    }

    auto load_button = engine::ui::UIButton::create(
        context_, "primary", glm::vec2{0.0f, 0.0f}, BUTTON_SIZE, [this]() { onLoadClicked(); });
    if (load_button) {
        load_button->setLabelText("Load");
        load_button_ = load_button.get();
        buttons->addChild(std::move(load_button));
    } else {
        spdlog::error("PauseMenuScene: 创建 load_button 失败。");
    }

    auto title_button = engine::ui::UIButton::create(
        context_, "primary", glm::vec2{0.0f, 0.0f}, BUTTON_SIZE, [this]() { onBackToTitleClicked(); });
    if (title_button) {
        title_button->setLabelText("Title");
        back_to_title_button_ = title_button.get();
        buttons->addChild(std::move(title_button));
    } else {
        spdlog::error("PauseMenuScene: 创建 title_button 失败。");
    }
    if (!save_service_) {
        disableButton(save_button_);
        disableButton(load_button_);
    }
    panel->addChild(std::move(buttons));

    auto buildIconRow = [&](std::string_view label_prefix,
                            std::string_view left_preset,
                            std::string_view right_preset,
                              engine::ui::UIButton** out_down,
                              engine::ui::UIButton** out_up,
                              engine::ui::UILabel** out_label,
                              std::function<void()> on_down,
                              std::function<void()> on_up) {
        auto row = std::make_unique<engine::ui::UIPanel>(glm::vec2{0.0f, 0.0f}, glm::vec2{BUTTON_SIZE.x, ICON_BUTTON_SIZE.y});

        auto down = engine::ui::UIButton::create(
            context_, left_preset, glm::vec2{0.0f, 0.0f}, ICON_BUTTON_SIZE, std::move(on_down));
        auto up = engine::ui::UIButton::create(
            context_,
            right_preset,
            glm::vec2{BUTTON_SIZE.x - ICON_BUTTON_SIZE.x, 0.0f},
            ICON_BUTTON_SIZE,
            std::move(on_up));

        auto label = std::make_unique<engine::ui::UILabel>(text_renderer, std::string(label_prefix));
        label->setAnchor({0.5f, 0.5f}, {0.5f, 0.5f});
        label->setPivot({0.5f, 0.5f});

        if (down) {
            if (out_down) *out_down = down.get();
            row->addChild(std::move(down));
        } else {
            spdlog::error("PauseMenuScene: 创建 icon row down button 失败 (preset='{}')。", left_preset);
        }
        if (up) {
            if (out_up) *out_up = up.get();
            row->addChild(std::move(up));
        } else {
            spdlog::error("PauseMenuScene: 创建 icon row up button 失败 (preset='{}')。", right_preset);
        }
        if (out_label) *out_label = label.get();

        row->addChild(std::move(label));
        return row;
    };

    const glm::vec2 volume_pos{0.0f, MESSAGE_AREA_HEIGHT + buttons_height + SECTION_SPACING};
    auto volume_stack = std::make_unique<engine::ui::UIStackLayout>(volume_pos, glm::vec2{BUTTON_SIZE.x, volume_height});
    volume_stack->setOrientation(engine::ui::Orientation::Vertical);
    volume_stack->setSpacing(VOLUME_ROW_SPACING);
    volume_stack->setAnchor({0.0f, 0.0f}, {0.0f, 0.0f});
    volume_stack->setPivot({0.0f, 0.0f});

    auto speed_row = buildIconRow(
        "Speed",
        "arrow_left",
        "arrow_right",
        &time_scale_down_button_,
        &time_scale_up_button_,
        &time_scale_label_,
        [this]() { adjustTimeScale(-1); },
        [this]() { adjustTimeScale(1); });

    auto music_row = buildIconRow(
        "Music",
        "volume_down",
        "volume_up",
        &music_down_button_,
        &music_up_button_,
        &music_label_,
        [this]() { adjustMusicVolume(-1); },
        [this]() { adjustMusicVolume(1); });

    auto sound_row = buildIconRow(
        "SFX",
        "volume_down",
        "volume_up",
        &sound_down_button_,
        &sound_up_button_,
        &sound_label_,
        [this]() { adjustSoundVolume(-1); },
        [this]() { adjustSoundVolume(1); });

    volume_stack->addChild(std::move(speed_row));
    volume_stack->addChild(std::move(music_row));
    volume_stack->addChild(std::move(sound_row));
    panel->addChild(std::move(volume_stack));

    panel_ = panel.get();
    ui_manager_->addElement(std::move(panel));
}

void PauseMenuScene::disableButton(engine::ui::UIButton* button) {
    if (!button) {
        return;
    }
    button->setInteractive(false);
    button->applyStateVisual(engine::ui::UI_IMAGE_DISABLED_ID);
}

void PauseMenuScene::refreshVolumeLabels() {
    auto& audio = context_.getAudioPlayer();
    if (music_label_) {
        music_label_->setText(toPercentLabel("Music", audio.getMusicVolume()));
    }
    if (sound_label_) {
        sound_label_->setText(toPercentLabel("SFX", audio.getSoundVolume()));
    }
}

void PauseMenuScene::refreshTimeScaleLabel() {
    if (!time_scale_label_) {
        return;
    }

    if (!game_time_) {
        time_scale_label_->setText("Speed N/A");
        if (time_scale_down_button_) time_scale_down_button_->setInteractive(false);
        if (time_scale_up_button_) time_scale_up_button_->setInteractive(false);
        return;
    }

    float scale = game_time_->time_scale_;
    if (!std::isfinite(scale) || scale <= 0.0f) {
        scale = 1.0f;
    }
    scale = std::clamp(scale, TIME_SCALE_MIN, TIME_SCALE_MAX);
    game_time_->time_scale_ = scale;

    time_scale_label_->setText(toMultiplierLabel("Speed", scale));
}

void PauseMenuScene::setMessage(std::string message, bool is_error) {
    if (!message_label_) {
        return;
    }

    message_label_->setText(message);
    message_label_->setVisible(!message.empty());
    message_label_->setTextColor(is_error ? engine::utils::FColor{1.0f, 0.25f, 0.25f, 1.0f}
                                          : engine::utils::FColor{0.25f, 1.0f, 0.25f, 1.0f});
}

bool PauseMenuScene::onPausePressed() {
    requestPopScene();
    return true;
}

void PauseMenuScene::onResumeClicked() {
    requestPopScene();
}

void PauseMenuScene::onSaveClicked() {
    setMessage("", false);

    if (!save_service_) {
        setMessage("SaveService unavailable", true);
        return;
    }

    auto on_select = [this](int slot) {
        std::string error;
        if (!save_service_->saveToFile(game::save::SaveService::slotPath(slot), error)) {
            setMessage("Save failed: " + error, true);
        } else {
            setMessage("Saved", false);
        }
        requestPopScene(); // close SaveSlotSelectScene
    };

    auto select = std::make_unique<game::scene::SaveSlotSelectScene>(
        "SaveSlotSelect", context_, std::move(on_select), game::scene::SaveSlotSelectScene::Mode::Save);
    requestPushScene(std::move(select));
}

void PauseMenuScene::onLoadClicked() {
    setMessage("", false);

    if (!save_service_) {
        setMessage("SaveService unavailable", true);
        return;
    }

    auto on_select = [this](int slot) {
        std::string error;
        if (!save_service_->loadFromFile(game::save::SaveService::slotPath(slot), error)) {
            setMessage("Load failed: " + error, true);
            requestPopScene(); // close SaveSlotSelectScene
            return;
        }

        setMessage("Loaded", false);
        requestPopScene(); // close SaveSlotSelectScene
        close_after_load_ = true; // close menu on next frame
    };

    auto select = std::make_unique<game::scene::SaveSlotSelectScene>(
        "SaveSlotSelect", context_, std::move(on_select), game::scene::SaveSlotSelectScene::Mode::Load);
    requestPushScene(std::move(select));
}

void PauseMenuScene::onBackToTitleClicked() {
    requestReplaceScene(std::make_unique<game::scene::TitleScene>("TitleScene", context_));
}

void PauseMenuScene::adjustMusicVolume(int step) {
    auto& audio = context_.getAudioPlayer();
    const float next = clamp01(audio.getMusicVolume() + static_cast<float>(step) * VOLUME_STEP);
    audio.setMusicVolume(next);
    refreshVolumeLabels();
}

void PauseMenuScene::adjustSoundVolume(int step) {
    auto& audio = context_.getAudioPlayer();
    const float next = clamp01(audio.getSoundVolume() + static_cast<float>(step) * VOLUME_STEP);
    audio.setSoundVolume(next);
    refreshVolumeLabels();
}

void PauseMenuScene::adjustTimeScale(int step) {
    if (!game_time_) {
        return;
    }

    float scale = game_time_->time_scale_;
    if (!std::isfinite(scale) || scale <= 0.0f) {
        scale = 1.0f;
    }
    scale = std::clamp(scale, TIME_SCALE_MIN, TIME_SCALE_MAX);

    if (step > 0) {
        auto exp_before = std::log10(scale);
        auto exp_after  = exp_before + TIME_SCALE_STEP_RATIO_EXP;
        scale = std::pow(10.0f, exp_after);
    } else if (step < 0) {
        auto exp_before = std::log10(scale);
        auto exp_after  = exp_before - TIME_SCALE_STEP_RATIO_EXP;
        scale = std::pow(10.0f, exp_after);
    }
    scale = std::clamp(scale, TIME_SCALE_MIN, TIME_SCALE_MAX);

    game_time_->time_scale_ = scale;
    refreshTimeScaleLabel();
}

} // namespace game::scene
