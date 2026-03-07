#include "title_scene.h"

#include "game_scene.h"
#include "pause_menu_scene.h"
#include "save_slot_select_scene.h"

#include "game/data/game_time.h"
#include "game/defs/audio_ids.h"

#include "engine/audio/audio_player.h"
#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/render/opengl/gl_renderer.h"
#include "engine/ui/rmlui/rml_ui_layer.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
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
    if (document_ || data_bridge_.isValid() || click_listener_registered_) {
        removeEventListeners();
        if (document_) {
            unloadAllRmlDocuments();
            document_ = nullptr;
        }
        data_bridge_.destroy();
    }
}

bool TitleScene::init() {
    context_.getGameState().setState(engine::core::State::Title);

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
    removeEventListeners();
    Scene::clean();
    document_ = nullptr;
    data_bridge_.destroy();
}

bool TitleScene::initUI() {
    auto* layer = context_.getGLRenderer().getRmlUILayer();
    if (!layer) {
        spdlog::error("TitleScene: RmlUILayer 不可用。");
        return false;
    }

    auto* rml_context = layer->getContext();
    if (!rml_context) {
        spdlog::error("TitleScene: RmlUi context 不可用。");
        return false;
    }

    auto constructor = data_bridge_.create(rml_context, MODEL_NAME);
    if (!constructor) {
        spdlog::error("TitleScene: 创建 data model 失败。");
        return false;
    }

    constructor.Bind("error_text", &error_text_);
    constructor.Bind("show_error", &show_error_);

    document_ = loadRmlDocument(DOCUMENT_PATH);
    if (!document_) {
        data_bridge_.destroy();
        spdlog::error("TitleScene: 加载 RML 文档失败。");
        return false;
    }

    event_bridge_.on("start", [this](Rml::Event&) { onStartClicked(); });
    event_bridge_.on("load", [this](Rml::Event&) { onLoadClicked(); });
    event_bridge_.on("menu", [this](Rml::Event&) { onMenuClicked(); });
    event_bridge_.on("exit", [this](Rml::Event&) { onExitClicked(); });
    event_bridge_.registerTo(document_, "click");
    click_listener_registered_ = true;

    error_text_ = Rml::String{error_message_.data(), error_message_.size()};
    show_error_ = !error_message_.empty();
    data_bridge_.markAllDirty();
    return true;
}

void TitleScene::removeEventListeners() {
    if (document_ && click_listener_registered_) {
        document_->RemoveEventListener("click", &event_bridge_);
        click_listener_registered_ = false;
    }
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
