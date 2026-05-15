#include "title_scene.h"

#include "game_scene.h"
#include "pause_menu_scene.h"
#include "save_slot_select_scene.h"

#include "game/data/game_time.h"
#include "game/defs/audio_ids.h"

#include "engine/audio/audio_player.h"
#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/input/input_manager.h"

#include <spdlog/spdlog.h>

#include <memory>
#include <utility>

namespace {

constexpr int MUSIC_FADE_IN_MS = 200;
constexpr std::string_view DOCUMENT_PATH = "ui/rmlui/scenes/title.rml";
constexpr std::string_view MODEL_NAME = "title_scene";

} // namespace

namespace game::scene {

TitleScene::TitleScene(std::string_view name, engine::core::Context& context, std::string error_message)
    : engine::scene::Scene(name, context),
      error_message_(std::move(error_message)) {
}

TitleScene::~TitleScene() {
    shutdownUI();
}

bool TitleScene::init() {
    context_.getGameState().setState(engine::core::State::Title);
    context_.getInputManager().pushContext(engine::input::InputContextId::Menu);
    context_pushed_ = true;

    title_game_time_ = game::data::GameTime::loadFromConfig("assets/data/game_time_config.json");
    if (!title_game_time_) {
        spdlog::warn("TitleScene: GameTime 加载失败，使用默认配置。");
        title_game_time_ = std::make_shared<game::data::GameTime>();
    }

    if (!initUI()) {
        return false;
    }
    if (!Scene::init()) {
        return false;
    }

    context_.getAudioPlayer().playMusic(game::defs::audio::TITLE_BG_MUSIC_ID.value(), true, MUSIC_FADE_IN_MS);
    return true;
}

void TitleScene::clean() {
    shutdownUI();
    if (context_pushed_) {
        context_.getInputManager().popContext();
        context_pushed_ = false;
    }
    Scene::clean();
}

bool TitleScene::initUI() {
    auto* runtime = context_.getRmlUi();
    if (!runtime) {
        spdlog::error("TitleScene: RmlUiRuntime 不可用。");
        return false;
    }

    document_controller_.attach(runtime, instanceId());
    auto constructor = document_controller_.createModel(MODEL_NAME);
    if (!constructor) {
        spdlog::error("TitleScene: 创建 data model 失败。");
        return false;
    }

    constructor.Bind("error_text", &error_text_);
    constructor.Bind("show_error", &show_error_);

    if (!document_controller_.bindSimpleEvent(constructor, "start", [this] { onStartClicked(); }) ||
        !document_controller_.bindSimpleEvent(constructor, "load", [this] { onLoadClicked(); }) ||
        !document_controller_.bindSimpleEvent(constructor, "menu", [this] { onMenuClicked(); }) ||
        !document_controller_.bindSimpleEvent(constructor, "exit", [this] { onExitClicked(); })) {
        spdlog::error("TitleScene: 绑定 data event 回调失败。");
        document_controller_.unload();
        return false;
    }

    if (!document_controller_.load(DOCUMENT_PATH)) {
        spdlog::error("TitleScene: 加载 RML 文档失败。");
        document_controller_.unload();
        return false;
    }

    error_text_ = Rml::String{error_message_.data(), error_message_.size()};
    show_error_ = !error_message_.empty();
    document_controller_.markAllDirty();
    return true;
}

void TitleScene::shutdownUI() {
    document_controller_.unload();
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
    auto menu = std::make_unique<game::scene::PauseMenuScene>(
        "PauseMenu", context_, nullptr, title_game_time_.get(), nullptr);
    requestPushScene(std::move(menu));
}

void TitleScene::onExitClicked() {
    quit();
}

} // namespace game::scene
