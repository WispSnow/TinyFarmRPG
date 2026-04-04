// NOLINTBEGIN
#include <gtest/gtest.h>

#include <SDL3/SDL.h>
#include <entt/core/hashed_string.hpp>
#include <entt/signal/dispatcher.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "engine/audio/audio_player.h"
#include "engine/async/main_thread_command_queue.h"
#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/core/time.h"
#ifdef TF_ENABLE_DEBUG_UI
#include "engine/debug/debug_ui_manager.h"
#endif
#include "engine/input/input_manager.h"
#include "engine/render/camera.h"
#include "engine/render/opengl/gl_renderer.h"
#include "engine/render/renderer.h"
#include "engine/render/text_renderer.h"
#include "engine/resource/auto_tile_library.h"
#include "engine/resource/resource_manager.h"
#include "engine/scene/scene.h"
#include "engine/scene/scene_manager.h"
#include "engine/spatial/spatial_index_manager.h"
#include "engine/utils/events.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace {

using namespace entt::literals;

[[nodiscard]] bool initSdlVideoWithDummyFallback(Uint32 flags) {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    return SDL_Init(flags);
}

[[nodiscard]] std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    EXPECT_TRUE(file.is_open()) << path;
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

struct SceneCallbackProbe {
    int calls{0};
    bool consume{true};
    std::vector<std::string> order{};
};

class TestMenuScene final : public engine::scene::Scene {
public:
    TestMenuScene(std::string_view name, engine::core::Context& context, SceneCallbackProbe& probe)
        : Scene(name, context), probe_(probe) {}

    [[nodiscard]] bool init() override {
        context_.getInputManager().pushContext(engine::input::InputContextId::Menu);
        context_pushed_ = true;
        context_.getInputManager().onAction("menu_cancel"_hs).connect<&TestMenuScene::onMenuCancel>(this);

        return Scene::init();
    }

    void clean() override {
        context_.getInputManager().onAction("menu_cancel"_hs).disconnect<&TestMenuScene::onMenuCancel>(this);
        if (context_pushed_) {
            context_.getInputManager().popContext();
            context_pushed_ = false;
        }
        Scene::clean();
    }

private:
    bool onMenuCancel() {
        ++probe_.calls;
        probe_.order.emplace_back(getName());
        return probe_.consume;
    }

    SceneCallbackProbe& probe_;
    bool context_pushed_{false};
};

void expectSceneContextHooks(std::string_view relative_path, std::string_view context_name) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / relative_path).lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("context_pushed_ = true;"), std::string::npos) << source_path;
    EXPECT_NE(source.find("if (context_pushed_)"), std::string::npos) << source_path;
    EXPECT_NE(source.find("context_.getInputManager().popContext();"), std::string::npos) << source_path;
    EXPECT_NE(source.find("context_pushed_ = false;"), std::string::npos) << source_path;
    EXPECT_NE(source.find("context_.getInputManager().pushContext(engine::input::InputContextId::" + std::string(context_name) + ");"),
              std::string::npos)
        << source_path;
}

} // namespace

namespace game::scene {
namespace {

class InputContextSceneStackTest : public ::testing::Test {
protected:
    static inline bool sdl_ready_{false};

    SDL_Window* window_{nullptr};
    std::filesystem::path input_config_path_{};
    std::filesystem::path original_working_dir_{};
    std::filesystem::path runtime_root_{};

    entt::dispatcher dispatcher_{};
    std::unique_ptr<engine::core::GameState> game_state_{};
    std::unique_ptr<engine::input::InputManager> input_manager_{};
    std::unique_ptr<engine::resource::ResourceManager> resource_manager_{};
    engine::resource::AutoTileLibrary auto_tile_library_{};
    std::unique_ptr<engine::audio::AudioPlayer> audio_player_{};
    std::unique_ptr<engine::render::opengl::GLRenderer> gl_renderer_{};
    std::unique_ptr<engine::render::Renderer> renderer_{};
    std::unique_ptr<engine::render::Camera> camera_{};
    std::unique_ptr<engine::render::TextRenderer> text_renderer_{};
    engine::spatial::SpatialIndexManager spatial_index_manager_{};
    std::unique_ptr<engine::core::Time> time_{};
    std::unique_ptr<engine::async::MainThreadCommandQueue> main_thread_command_queue_{};
#ifdef TF_ENABLE_DEBUG_UI
    std::unique_ptr<engine::debug::DebugUIManager> debug_ui_manager_{};
#endif
    std::unique_ptr<engine::core::Context> context_{};
    std::unique_ptr<engine::scene::SceneManager> scene_manager_{};

    static void SetUpTestSuite() {
        sdl_ready_ = initSdlVideoWithDummyFallback(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    }

    static void TearDownTestSuite() {
        if (sdl_ready_) {
            SDL_Quit();
        }
    }

    void SetUp() override {
        if (!sdl_ready_) {
            GTEST_SKIP() << "SDL video subsystem not available in this environment.";
        }

        original_working_dir_ = std::filesystem::current_path();
        runtime_root_ = original_working_dir_;
        if (!std::filesystem::exists(runtime_root_ / "assets") &&
            std::filesystem::exists(runtime_root_.parent_path() / "assets")) {
            runtime_root_ = runtime_root_.parent_path();
        }
        if (!std::filesystem::exists(runtime_root_ / "assets")) {
            GTEST_SKIP() << "Unable to locate runtime assets directory for input context scene stack test.";
        }

        std::error_code ec;
        std::filesystem::current_path(runtime_root_, ec);
        if (ec) {
            GTEST_SKIP() << "Failed to switch working directory to runtime root.";
        }

        window_ = SDL_CreateWindow("InputContextSceneStackTest", 640, 360, SDL_WINDOW_HIDDEN);
        if (!window_) {
            GTEST_SKIP() << "Failed to create SDL window.";
        }

        game_state_ = engine::core::GameState::create(window_);
        if (!game_state_) {
            GTEST_SKIP() << "Failed to create GameState.";
        }
        game_state_->setWindowSize({640.0F, 360.0F});
        game_state_->setLogicalSize({640.0F, 360.0F});

        input_config_path_ = std::filesystem::temp_directory_path() /
                             ("input_context_scene_stack_" +
                              std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".json");
        std::ofstream input_config(input_config_path_);
        ASSERT_TRUE(input_config.is_open());
        input_config << R"({"input_mappings":{"menu_cancel":["Escape"]}})";
        input_config.close();

        input_manager_ = engine::input::InputManager::create(&dispatcher_, game_state_.get(), input_config_path_.string());
        if (!input_manager_) {
            GTEST_SKIP() << "Failed to create InputManager.";
        }

        resource_manager_ = engine::resource::ResourceManager::create(&dispatcher_);
        if (!resource_manager_) {
            GTEST_SKIP() << "Failed to create ResourceManager.";
        }

        audio_player_ = engine::audio::AudioPlayer::create(resource_manager_.get());
        if (!audio_player_) {
            GTEST_SKIP() << "Failed to create AudioPlayer.";
        }

        gl_renderer_ = engine::render::opengl::GLRenderer::createHeadless(game_state_->getLogicalSize());
        if (!gl_renderer_) {
            GTEST_SKIP() << "Failed to create GLRenderer.";
        }

        renderer_ = engine::render::Renderer::create(gl_renderer_.get(), resource_manager_.get());
        if (!renderer_) {
            GTEST_SKIP() << "Failed to create Renderer.";
        }

        camera_ = std::make_unique<engine::render::Camera>(game_state_->getLogicalSize());
        text_renderer_ = engine::render::TextRenderer::create(gl_renderer_.get(), resource_manager_.get(), &dispatcher_);
        if (!text_renderer_) {
            GTEST_SKIP() << "Failed to create TextRenderer.";
        }

        time_ = std::make_unique<engine::core::Time>();
        main_thread_command_queue_ = std::make_unique<engine::async::MainThreadCommandQueue>();
#ifdef TF_ENABLE_DEBUG_UI
        debug_ui_manager_ = std::make_unique<engine::debug::DebugUIManager>();
#endif

        engine::core::CoreServices core_services{
            dispatcher_, *game_state_, *time_, *input_manager_, *main_thread_command_queue_
        };
        engine::core::RenderServices render_services{
            *gl_renderer_, *renderer_, *camera_, *text_renderer_
        };
        engine::core::ResourceServices resource_services{
            *resource_manager_, auto_tile_library_
        };
        engine::core::UiServices ui_services{};
        context_ = engine::core::Context::create(
            core_services, render_services, resource_services, ui_services,
            *audio_player_, spatial_index_manager_
#ifdef TF_ENABLE_DEBUG_UI
            , *debug_ui_manager_
#endif
        );
        if (!context_) {
            GTEST_SKIP() << "Failed to create Context.";
        }

        scene_manager_ = std::make_unique<engine::scene::SceneManager>(*context_);
        drainEvents();
    }

    void TearDown() override {
        scene_manager_.reset();
        context_.reset();
#ifdef TF_ENABLE_DEBUG_UI
        debug_ui_manager_.reset();
#endif
        main_thread_command_queue_.reset();
        time_.reset();
        text_renderer_.reset();
        camera_.reset();
        renderer_.reset();
        gl_renderer_.reset();
        audio_player_.reset();
        resource_manager_.reset();
        input_manager_.reset();
        game_state_.reset();

        if (window_ != nullptr) {
            SDL_DestroyWindow(window_);
            window_ = nullptr;
        }

        std::error_code ec;
        std::filesystem::remove(input_config_path_, ec);
        if (!original_working_dir_.empty()) {
            std::filesystem::current_path(original_working_dir_, ec);
        }
        drainEvents();
    }

    void drainEvents() {
        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
        }
    }

    void pushKey(SDL_Scancode scancode, bool down) {
        SDL_Event event{};
        event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        event.key.windowID = SDL_GetWindowID(window_);
        event.key.scancode = scancode;
        event.key.down = down;
        event.key.repeat = false;
        ASSERT_EQ(SDL_PushEvent(&event), true);
    }

    void pushScene(std::unique_ptr<engine::scene::Scene>&& scene) {
        dispatcher_.trigger<engine::utils::PushSceneEvent>(engine::utils::PushSceneEvent{std::move(scene)});
        scene_manager_->update(0.0f);
    }

    void popScene() {
        dispatcher_.trigger<engine::utils::PopSceneEvent>();
        scene_manager_->update(0.0f);
    }
};

TEST_F(InputContextSceneStackTest, SceneManagerPrefersTopMenuCancelListenerAndRestoresLowerSceneAfterPop) {
    SceneCallbackProbe pause_menu_probe{};
    pause_menu_probe.consume = false;
    SceneCallbackProbe save_slot_probe{};
    save_slot_probe.consume = true;

    pushScene(std::make_unique<TestMenuScene>("PauseMenuScene", *context_, pause_menu_probe));
    ASSERT_EQ(scene_manager_->getSceneStackSize(), 1U);
    EXPECT_EQ(context_->getInputManager().currentContext(),
              std::optional<engine::input::InputContextId>{engine::input::InputContextId::Menu});

    pushScene(std::make_unique<TestMenuScene>("SaveSlotSelectScene", *context_, save_slot_probe));
    ASSERT_EQ(scene_manager_->getSceneStackSize(), 2U);
    EXPECT_EQ(context_->getInputManager().currentContext(),
              std::optional<engine::input::InputContextId>{engine::input::InputContextId::Menu});

    pushKey(SDL_SCANCODE_ESCAPE, true);
    context_->getInputManager().sampleInputEvents();
    context_->getInputManager().dispatchActionCallbacks();

    EXPECT_EQ(save_slot_probe.calls, 1);
    EXPECT_EQ(pause_menu_probe.calls, 0);
    EXPECT_EQ(save_slot_probe.order, std::vector<std::string>{"SaveSlotSelectScene"});

    context_->getInputManager().consumeTick();
    popScene();

    ASSERT_EQ(scene_manager_->getSceneStackSize(), 1U);
    EXPECT_EQ(context_->getInputManager().currentContext(),
              std::optional<engine::input::InputContextId>{engine::input::InputContextId::Menu});

    pushKey(SDL_SCANCODE_ESCAPE, true);
    context_->getInputManager().sampleInputEvents();
    context_->getInputManager().dispatchActionCallbacks();

    EXPECT_EQ(pause_menu_probe.calls, 1);
    EXPECT_EQ(pause_menu_probe.order, std::vector<std::string>{"PauseMenuScene"});
}

TEST(InputContextSceneSourceHookTest, ModalAndGameplayScenesPairPushAndPopHooks) {
    // 运行时行为由上面的 test scene 覆盖；这里额外保证真实 scene 文件没有漏接 hook。
    expectSceneContextHooks("src/game/scene/title_scene.cpp", "Menu");
    expectSceneContextHooks("src/game/scene/game_scene.cpp", "Gameplay");
    expectSceneContextHooks("src/game/scene/pause_menu_scene.cpp", "Menu");
    expectSceneContextHooks("src/game/scene/save_slot_select_scene.cpp", "Menu");
    expectSceneContextHooks("src/game/scene/rest_dialog_scene.cpp", "Dialogue");
    expectSceneContextHooks("src/game/scene/battle_scene.cpp", "Battle");
}

TEST(InputContextSceneSourceHookTest, MenuScenesUseMenuCancelInsteadOfPause) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/pause_menu_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("\"menu_cancel\"_hs"), std::string::npos);
    EXPECT_EQ(source.find("\"pause\"_hs"), std::string::npos);

    const std::filesystem::path save_source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/save_slot_select_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(save_source_path)) << save_source_path;

    const std::string save_source = readTextFile(save_source_path);
    ASSERT_FALSE(save_source.empty());

    EXPECT_NE(save_source.find("\"menu_cancel\"_hs"), std::string::npos);
    EXPECT_EQ(save_source.find("\"pause\"_hs"), std::string::npos);
}

} // namespace
} // namespace game::scene
// NOLINTEND
