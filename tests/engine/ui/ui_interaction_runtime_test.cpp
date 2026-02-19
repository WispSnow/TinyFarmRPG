#include <gtest/gtest.h>

#include <SDL3/SDL.h>
#include <entt/signal/dispatcher.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "engine/audio/audio_player.h"
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
#include "engine/spatial/spatial_index_manager.h"
#include "engine/ui/behavior/click_behavior.h"
#include "engine/ui/ui_interactive.h"
#include "engine/ui/ui_manager.h"
#include "engine/ui/ui_preset_manager.h"

namespace engine::ui {
namespace {

[[nodiscard]] bool initSdlVideoWithDummyFallback(Uint32 flags) {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    return SDL_Init(flags);
}

struct RuntimeProbe {
    int destroyed_count{0};
    std::vector<entt::id_type> visual_states{};

    int behavior_hover_enter_count{0};
    int behavior_hover_exit_count{0};
    int behavior_pressed_count{0};
    int behavior_released_count{0};
    int behavior_click_count{0};
    int behavior_drag_begin_count{0};
    int behavior_drag_end_count{0};
    int state_hover_enter_count{0};
    int state_hover_leave_count{0};
    int click_behavior_click_count{0};
    bool last_release_inside{true};
    bool last_drag_end_inside{true};
    std::vector<std::pair<InteractionPhase, InteractionPhase>> phase_transitions{};
};

class ProbeBehavior final : public InteractionBehavior {
public:
    explicit ProbeBehavior(std::shared_ptr<RuntimeProbe> probe) : probe_(std::move(probe)) {}

    void onHoverEnter(UIInteractive&) override {
        ++probe_->behavior_hover_enter_count;
    }

    void onHoverExit(UIInteractive&) override {
        ++probe_->behavior_hover_exit_count;
    }

    void onPressed(UIInteractive&) override {
        ++probe_->behavior_pressed_count;
    }

    void onReleased(UIInteractive&, bool inside) override {
        ++probe_->behavior_released_count;
        probe_->last_release_inside = inside;
    }

    void onClick(UIInteractive&) override {
        ++probe_->behavior_click_count;
    }

    void onDragBegin(UIInteractive&, const glm::vec2&) override {
        ++probe_->behavior_drag_begin_count;
    }

    void onDragEnd(UIInteractive&, const glm::vec2&, bool accepted) override {
        ++probe_->behavior_drag_end_count;
        probe_->last_drag_end_inside = accepted;
    }

    void onStateChanged(UIInteractive&, InteractionPhase old_phase, InteractionPhase new_phase) override {
        if (new_phase == InteractionPhase::Hovered && old_phase != InteractionPhase::Hovered) {
            ++probe_->state_hover_enter_count;
        }
        if (old_phase == InteractionPhase::Hovered && new_phase == InteractionPhase::Normal) {
            ++probe_->state_hover_leave_count;
        }
        probe_->phase_transitions.emplace_back(old_phase, new_phase);
    }

private:
    std::shared_ptr<RuntimeProbe> probe_;
};

class TestInteractive final : public UIInteractive {
public:
    TestInteractive(engine::core::Context& context,
                    std::shared_ptr<RuntimeProbe> probe,
                    glm::vec2 position,
                    glm::vec2 size)
        : UIInteractive(context, position, size), probe_(std::move(probe)) {}

    ~TestInteractive() override {
        if (probe_) {
            ++probe_->destroyed_count;
        }
    }

    void applyStateVisual(entt::id_type state_id) override {
        probe_->visual_states.push_back(state_id);
    }

private:
    std::shared_ptr<RuntimeProbe> probe_;
};

class UIInteractionRuntimeTest : public ::testing::Test {
protected:
    static inline bool sdl_ready_{false};

    SDL_Window* window_{nullptr};
    std::filesystem::path input_config_path_{};

    entt::dispatcher dispatcher_{};
    std::unique_ptr<engine::core::GameState> game_state_{};
    std::unique_ptr<engine::input::InputManager> input_manager_{};
    std::unique_ptr<engine::resource::ResourceManager> resource_manager_{};
    engine::resource::AutoTileLibrary auto_tile_library_{};
    std::unique_ptr<engine::ui::UIPresetManager> ui_preset_manager_{};
    std::unique_ptr<engine::audio::AudioPlayer> audio_player_{};
    std::unique_ptr<engine::render::opengl::GLRenderer> gl_renderer_{};
    std::unique_ptr<engine::render::Renderer> renderer_{};
    std::unique_ptr<engine::render::Camera> camera_{};
    std::unique_ptr<engine::render::TextRenderer> text_renderer_{};
    engine::spatial::SpatialIndexManager spatial_index_manager_{};
    std::unique_ptr<engine::core::Time> time_{};
#ifdef TF_ENABLE_DEBUG_UI
    std::unique_ptr<engine::debug::DebugUIManager> debug_ui_manager_{};
#endif
    std::unique_ptr<engine::core::Context> context_{};

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

        window_ = SDL_CreateWindow("UIInteractionRuntimeTest", 160, 120, SDL_WINDOW_HIDDEN);
        if (!window_) {
            GTEST_SKIP() << "Failed to create SDL window.";
        }

        game_state_ = engine::core::GameState::create(window_);
        if (!game_state_) {
            GTEST_SKIP() << "Failed to create GameState.";
        }
        game_state_->setWindowSize({160.0f, 120.0f});
        game_state_->setLogicalSize({160.0f, 120.0f});

        const auto timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        input_config_path_ = std::filesystem::temp_directory_path() /
                             ("ui_interaction_runtime_input_" + std::to_string(timestamp) + ".json");
        std::ofstream input_config(input_config_path_);
        ASSERT_TRUE(input_config.is_open());
        input_config << R"({"input_mappings":{"mouse_left":["MouseLeft"]}})";
        input_config.close();

        input_manager_ = engine::input::InputManager::create(&dispatcher_, game_state_.get(), input_config_path_.string());
        if (!input_manager_) {
            GTEST_SKIP() << "Failed to create InputManager.";
        }

        resource_manager_ = engine::resource::ResourceManager::create(&dispatcher_);
        if (!resource_manager_) {
            GTEST_SKIP() << "Failed to create ResourceManager.";
        }
        ui_preset_manager_ = std::make_unique<engine::ui::UIPresetManager>();

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
        camera_->setPosition(game_state_->getLogicalSize() * 0.5F);
        camera_->setZoom(1.0F);

        text_renderer_ =
            engine::render::TextRenderer::create(gl_renderer_.get(), resource_manager_.get(), &dispatcher_);
        if (!text_renderer_) {
            GTEST_SKIP() << "Failed to create TextRenderer.";
        }

        time_ = std::make_unique<engine::core::Time>();
#ifdef TF_ENABLE_DEBUG_UI
        debug_ui_manager_ = std::make_unique<engine::debug::DebugUIManager>();
#endif
        context_ = engine::core::Context::create(dispatcher_,
                                                 *input_manager_,
                                                 *gl_renderer_,
                                                 *renderer_,
                                                 *camera_,
                                                 *text_renderer_,
                                                 *resource_manager_,
                                                 auto_tile_library_,
                                                 *ui_preset_manager_,
                                                 *audio_player_,
                                                 *game_state_,
                                                 *time_,
#ifdef TF_ENABLE_DEBUG_UI
                                                 *debug_ui_manager_,
#endif
                                                 spatial_index_manager_);
        if (!context_) {
            GTEST_SKIP() << "Failed to create Context.";
        }

        drainPendingEvents();
    }

    void TearDown() override {
        context_.reset();
#ifdef TF_ENABLE_DEBUG_UI
        debug_ui_manager_.reset();
#endif
        ui_preset_manager_.reset();
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
        drainPendingEvents();
    }

    void pushMouseMotion(float x, float y) {
        SDL_Event motion{};
        motion.type = SDL_EVENT_MOUSE_MOTION;
        motion.motion.x = x;
        motion.motion.y = y;
        ASSERT_EQ(SDL_PushEvent(&motion), true);
    }

    void pushMouseLeftButton(bool down, float x, float y) {
        SDL_Event button{};
        button.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
        button.button.button = SDL_BUTTON_LEFT;
        button.button.down = down;
        button.button.x = x;
        button.button.y = y;
        ASSERT_EQ(SDL_PushEvent(&button), true);
    }

    void updateInput() const {
        input_manager_->update();
    }

    static void drainPendingEvents() {
        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
        }
    }
};

TEST_F(UIInteractionRuntimeTest, HoverPressReleaseInsideMatrixKeepsCallbacksConsistent) {
    auto probe = std::make_shared<RuntimeProbe>();
    TestInteractive element(*context_, probe, {0.0F, 0.0F}, {80.0F, 40.0F});
    element.addBehavior(std::make_unique<ProbeBehavior>(probe));

    auto click_behavior = std::make_unique<ClickBehavior>();
    click_behavior->setOnClick([probe](UIInteractive&) {
        ++probe->click_behavior_click_count;
    });
    element.addBehavior(std::move(click_behavior));

    EXPECT_EQ(element.getInteractionPhase(), InteractionPhase::Normal);

    element.mouseEnter();
    EXPECT_EQ(element.getInteractionPhase(), InteractionPhase::Hovered);
    EXPECT_EQ(probe->state_hover_enter_count, 1);
    EXPECT_EQ(probe->behavior_hover_enter_count, 1);

    element.mousePressed();
    EXPECT_EQ(element.getInteractionPhase(), InteractionPhase::Pressed);
    EXPECT_EQ(probe->behavior_pressed_count, 1);
    EXPECT_EQ(probe->behavior_drag_begin_count, 1);

    element.mouseReleased(true);
    EXPECT_EQ(element.getInteractionPhase(), InteractionPhase::Hovered);
    EXPECT_EQ(probe->behavior_released_count, 1);
    EXPECT_TRUE(probe->last_release_inside);
    EXPECT_EQ(probe->behavior_click_count, 1);
    EXPECT_EQ(probe->click_behavior_click_count, 1);
    EXPECT_EQ(probe->behavior_drag_end_count, 1);
    EXPECT_TRUE(probe->last_drag_end_inside);
    EXPECT_EQ(probe->state_hover_enter_count, 2);

    element.mouseExit();
    EXPECT_EQ(element.getInteractionPhase(), InteractionPhase::Normal);
    EXPECT_EQ(probe->state_hover_leave_count, 1);
    EXPECT_EQ(probe->behavior_hover_exit_count, 1);

    ASSERT_EQ(probe->phase_transitions.size(), 4);
    EXPECT_EQ(probe->phase_transitions[0].first, InteractionPhase::Normal);
    EXPECT_EQ(probe->phase_transitions[0].second, InteractionPhase::Hovered);
    EXPECT_EQ(probe->phase_transitions[1].first, InteractionPhase::Hovered);
    EXPECT_EQ(probe->phase_transitions[1].second, InteractionPhase::Pressed);
    EXPECT_EQ(probe->phase_transitions[2].first, InteractionPhase::Pressed);
    EXPECT_EQ(probe->phase_transitions[2].second, InteractionPhase::Hovered);
    EXPECT_EQ(probe->phase_transitions[3].first, InteractionPhase::Hovered);
    EXPECT_EQ(probe->phase_transitions[3].second, InteractionPhase::Normal);
}

TEST_F(UIInteractionRuntimeTest, NormalPressPathSkipsHoverEnterUntilReleaseInside) {
    auto probe = std::make_shared<RuntimeProbe>();
    TestInteractive element(*context_, probe, {0.0F, 0.0F}, {80.0F, 40.0F});
    element.addBehavior(std::make_unique<ProbeBehavior>(probe));

    auto click_behavior = std::make_unique<ClickBehavior>();
    click_behavior->setOnClick([probe](UIInteractive&) {
        ++probe->click_behavior_click_count;
    });
    element.addBehavior(std::move(click_behavior));

    element.mousePressed();
    EXPECT_EQ(element.getInteractionPhase(), InteractionPhase::Pressed);
    EXPECT_EQ(probe->state_hover_enter_count, 0);

    // UIR-010 expected behavior:
    // Normal -> Pressed path does not go through Hovered enter callback in the press frame.
    element.mouseReleased(true);
    EXPECT_EQ(element.getInteractionPhase(), InteractionPhase::Hovered);
    EXPECT_EQ(probe->state_hover_enter_count, 1);
    EXPECT_EQ(probe->behavior_click_count, 1);
    EXPECT_EQ(probe->click_behavior_click_count, 1);
}

TEST_F(UIInteractionRuntimeTest, DisableWhilePressedCancelsReleaseAndBlocksFurtherInput) {
    auto probe = std::make_shared<RuntimeProbe>();
    TestInteractive element(*context_, probe, {0.0F, 0.0F}, {80.0F, 40.0F});
    element.addBehavior(std::make_unique<ProbeBehavior>(probe));

    element.mousePressed();
    EXPECT_EQ(element.getInteractionPhase(), InteractionPhase::Pressed);

    element.setEnabled(false);
    EXPECT_FALSE(element.isInteractive());
    EXPECT_EQ(element.getInteractionPhase(), InteractionPhase::Disabled);
    EXPECT_EQ(probe->behavior_click_count, 0);
    EXPECT_EQ(probe->behavior_released_count, 1);
    EXPECT_FALSE(probe->last_release_inside);
    EXPECT_EQ(probe->behavior_drag_end_count, 1);
    EXPECT_FALSE(probe->last_drag_end_inside);

    element.mouseEnter();
    element.mousePressed();
    element.mouseReleased(true);
    EXPECT_EQ(probe->behavior_pressed_count, 1);
    EXPECT_EQ(probe->behavior_released_count, 1);
    EXPECT_EQ(probe->behavior_click_count, 0);
    EXPECT_EQ(element.getInteractionPhase(), InteractionPhase::Disabled);

    element.setEnabled(true);
    EXPECT_TRUE(element.isInteractive());
    EXPECT_EQ(element.getInteractionPhase(), InteractionPhase::Normal);
}

TEST_F(UIInteractionRuntimeTest, EnableToggleFromHoveredConvergesAndCanHoverAgain) {
    auto probe = std::make_shared<RuntimeProbe>();
    TestInteractive element(*context_, probe, {0.0F, 0.0F}, {80.0F, 40.0F});
    element.addBehavior(std::make_unique<ProbeBehavior>(probe));

    element.mouseEnter();
    EXPECT_EQ(element.getInteractionPhase(), InteractionPhase::Hovered);
    EXPECT_EQ(probe->state_hover_enter_count, 1);

    element.setEnabled(false);
    EXPECT_FALSE(element.isInteractive());
    EXPECT_EQ(element.getInteractionPhase(), InteractionPhase::Disabled);

    element.setEnabled(true);
    EXPECT_TRUE(element.isInteractive());
    EXPECT_EQ(element.getInteractionPhase(), InteractionPhase::Normal);

    element.mouseEnter();
    EXPECT_EQ(element.getInteractionPhase(), InteractionPhase::Hovered);
    EXPECT_EQ(probe->state_hover_enter_count, 2);
}

TEST_F(UIInteractionRuntimeTest, UIManagerClearElementsCancelsPressedCaptureWithoutClick) {
    UIManager manager(*context_, game_state_->getLogicalSize());
    auto probe = std::make_shared<RuntimeProbe>();

    auto element = std::make_unique<TestInteractive>(*context_, probe, glm::vec2{5.0F, 5.0F}, glm::vec2{40.0F, 30.0F});
    element->addBehavior(std::make_unique<ProbeBehavior>(probe));
    manager.addElement(std::move(element));

    pushMouseMotion(10.0F, 10.0F);
    updateInput();
    manager.update(0.0F, *context_);

    pushMouseLeftButton(true, 10.0F, 10.0F);
    updateInput();
    manager.update(0.0F, *context_);
    EXPECT_EQ(probe->behavior_pressed_count, 1);

    manager.clearElements();
    EXPECT_EQ(probe->behavior_released_count, 1);
    EXPECT_FALSE(probe->last_release_inside);
    EXPECT_EQ(probe->behavior_click_count, 0);
    EXPECT_EQ(probe->destroyed_count, 1);
}

} // namespace
} // namespace engine::ui
