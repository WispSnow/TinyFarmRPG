#include "title_scene.h"

#include "appearance_customize_scene.h"
#include "game_scene.h"
#include "pause_menu_scene.h"
#include "save_slot_select_scene.h"

#include "game/data/game_time.h"
#include "game/defs/audio_ids.h"
#include "game/defs/options_events.h"
#include "game/runtime/game_content_manifest.h"
#include "game/runtime/localization_service.h"
#include "game/runtime/user_settings_service.h"
#include "game/ui/localized_text.h"

#include "engine/audio/audio_player.h"
#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/input/input_manager.h"
#include "engine/platform/web_asset_package_registry.h"

#include <spdlog/spdlog.h>

#include <memory>
#include <utility>

namespace {

constexpr int MUSIC_FADE_IN_MS = 200;
constexpr std::string_view DOCUMENT_PATH = "ui/rmlui/scenes/title.rml";
constexpr std::string_view MODEL_NAME = "title_scene";

[[nodiscard]] bool ensureWebSharedUiPackage() {
#if defined(__EMSCRIPTEN__) && defined(TF_WEB_ENABLE_RUNTIME_PACKAGES)
    return engine::platform::web::loadPackage(engine::platform::web::PACKAGE_SHARED_UI);
#else
    return true;
#endif
}

} // namespace

namespace game::scene {

TitleScene::TitleScene(std::string_view name, engine::core::Context& context, TitleSceneMessage error_message)
    : engine::scene::Scene(name, context),
      error_message_(std::move(error_message)) {
}

TitleScene::~TitleScene() {
    disconnectRuntimeListeners();
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
    initUserSettings();
    connectRuntimeListeners();

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
    flushUserSettings();
    shutdownUI();
    disconnectRuntimeListeners();
    if (context_pushed_) {
        context_.getInputManager().popContext();
        context_pushed_ = false;
    }
    Scene::clean();
}

void TitleScene::initUserSettings() {
    auto* rml_runtime = context_.getRmlUi();
    if (!rml_runtime || !title_game_time_) {
        spdlog::warn("TitleScene: 缺少 RmlUiRuntime 或 GameTime，标题页设置服务跳过初始化。");
        return;
    }

    localization_service_ = std::make_unique<game::runtime::LocalizationService>();
    if (!localization_service_->loadLanguageIndex(game::runtime::GameContentManifest::I18nLanguages)) {
        spdlog::warn("TitleScene: 本地化语言表加载失败，将继续使用默认语言。");
    }

    user_settings_service_ = std::make_unique<game::runtime::UserSettingsService>(
        context_.getDispatcher(),
        context_.getAudioPlayer(),
        *title_game_time_,
        *localization_service_,
        context_.getTextRenderer(),
        *rml_runtime);
    user_settings_service_->loadFromFileOrFallback();
    user_settings_service_->applyAll();
}

void TitleScene::flushUserSettings() {
    if (user_settings_service_) {
        (void)user_settings_service_->flushIfDirty();
    }
}

const game::runtime::LocalizationService* TitleScene::localization() const noexcept {
    return user_settings_service_ ? &user_settings_service_->localization() : nullptr;
}

void TitleScene::connectRuntimeListeners() {
    if (runtime_listeners_connected_) {
        return;
    }
    context_.getDispatcher().sink<game::defs::LanguageChangedEvent>().connect<&TitleScene::onLanguageChanged>(this);
    runtime_listeners_connected_ = true;
}

void TitleScene::disconnectRuntimeListeners() {
    if (!runtime_listeners_connected_) {
        return;
    }
    context_.getDispatcher().sink<game::defs::LanguageChangedEvent>().disconnect<&TitleScene::onLanguageChanged>(this);
    runtime_listeners_connected_ = false;
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

    syncErrorText(false);
    document_controller_.markAllDirty();
    return true;
}

void TitleScene::shutdownUI() {
    document_controller_.unload();
}

void TitleScene::syncErrorText(bool mark_dirty) {
    show_error_ = !error_message_.key.empty();
    if (!show_error_) {
        error_text_ = {};
    } else {
        const std::string text = game::ui::formatTextOrFallback(
            localization(),
            error_message_.key,
            error_message_.args,
            [this] { return "!" + error_message_.key + "!"; });
        error_text_ = Rml::String{text.data(), text.size()};
    }

    if (mark_dirty) {
        document_controller_.markDirty("error_text");
        document_controller_.markDirty("show_error");
    }
}

void TitleScene::onStartClicked() {
    spdlog::info("TitleScene: Start clicked.");
    if (!ensureWebSharedUiPackage()) {
        spdlog::error("TitleScene: Web shared-ui package 加载失败，无法进入外观创建。");
        return;
    }

    auto* context = &context_;
    auto game_time = title_game_time_;
    auto on_confirm = [context, game_time](game::scene::NewGameCharacterSetup setup) mutable
        -> std::unique_ptr<engine::scene::Scene> {
        game::scene::NewGameOptions options{};
        options.initial_appearance = std::move(setup.appearance);
        options.player_name = std::move(setup.player_name);
        return std::make_unique<game::scene::GameScene>(
            "GameScene",
            *context,
            game_time,
            game::scene::GameSceneLaunch{std::move(options)});
    };
    auto next = std::make_unique<game::scene::AppearanceCustomizeScene>(
        "AppearanceCustomize",
        context_,
        std::move(on_confirm),
        localization());
    requestPushScene(std::move(next));
}

void TitleScene::onLoadClicked() {
    spdlog::info("TitleScene: Load clicked.");
    if (!ensureWebSharedUiPackage()) {
        spdlog::error("TitleScene: Web shared-ui package 加载失败，无法打开存档选择。");
        return;
    }

    auto on_select = [this](int slot) {
        spdlog::info("TitleScene: Loading save slot {}.", slot);
        auto next = std::make_unique<game::scene::GameScene>(
            "GameScene",
            context_,
            nullptr,
            game::scene::GameSceneLaunch{game::scene::LoadGameOptions{slot}});
        requestReplaceScene(std::move(next));
    };

    auto menu = std::make_unique<game::scene::SaveSlotSelectScene>(
        "SaveSlotSelect",
        context_,
        std::move(on_select),
        game::scene::SaveSlotSelectScene::Mode::Load,
        localization());
    requestPushScene(std::move(menu));
}

void TitleScene::onMenuClicked() {
    if (!ensureWebSharedUiPackage()) {
        spdlog::error("TitleScene: Web shared-ui package 加载失败，无法打开菜单。");
        return;
    }

    auto menu = std::make_unique<game::scene::PauseMenuScene>(
        "PauseMenu",
        context_,
        nullptr,
        title_game_time_.get(),
        user_settings_service_.get(),
        nullptr);
    requestPushScene(std::move(menu));
}

void TitleScene::onExitClicked() {
    quit();
}

void TitleScene::onLanguageChanged(const game::defs::LanguageChangedEvent&) {
    syncErrorText(true);
}

} // namespace game::scene
