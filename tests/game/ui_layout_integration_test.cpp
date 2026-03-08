#include <gtest/gtest.h>

#include <SDL3/SDL.h>
#include <entt/core/hashed_string.hpp>
#include <entt/signal/dispatcher.hpp>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
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
#include "engine/ui/rmlui/rml_ui_layer.h"
#include "engine/render/renderer.h"
#include "engine/render/text_renderer.h"
#include "engine/resource/auto_tile_library.h"
#include "engine/resource/resource_manager.h"
#include "engine/spatial/spatial_index_manager.h"
#include "engine/ui/ui_button.h"
#include "engine/ui/ui_element.h"
#include "engine/ui/ui_item_slot.h"
#include "engine/ui/ui_label.h"
#include "engine/ui/ui_preset_manager.h"
#include "game/component/hotbar_component.h"
#include "game/ui/hotbar_ui.h"
#include "game/ui/inventory_ui.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace game::ui {
namespace {

[[nodiscard]] bool initSdlVideoWithDummyFallback(Uint32 flags) {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    return SDL_Init(flags);
}

engine::render::Image makeTestPresetImage(std::string_view key,
                                          const glm::vec2& size,
                                          std::optional<engine::render::NineSliceMargins> margins = std::nullopt) {
    engine::render::Image image(
        std::string{"test://"} + std::string(key),
        engine::utils::Rect{glm::vec2{0.0F, 0.0F}, size}
    );
    if (margins.has_value()) {
        image.setNineSliceMargins(*margins);
    }
    return image;
}

constexpr float kEpsilon = 0.001F;

float centerX(const engine::utils::Rect& bounds) {
    return bounds.pos.x + bounds.size.x * 0.5F;
}

float centerY(const engine::utils::Rect& bounds) {
    return bounds.pos.y + bounds.size.y * 0.5F;
}

Rml::ElementDocument* findDocumentByElementId(Rml::Context& context, std::string_view element_id) {
    const Rml::String id{element_id.data(), element_id.size()};
    for (int index = 0; index < context.GetNumDocuments(); ++index) {
        auto* document = context.GetDocument(index);
        if (document && document->GetElementById(id)) {
            return document;
        }
    }
    return nullptr;
}

void forceLayoutTree(engine::ui::UIElement& node) {
    (void)node.getLayoutSize();
    for (const auto& child : node.getChildren()) {
        if (child) {
            forceLayoutTree(*child);
        }
    }
}

template <typename T>
void collectDescendants(engine::ui::UIElement& node, std::vector<T*>& out) {
    if (auto* typed = dynamic_cast<T*>(&node)) {
        out.push_back(typed);
    }
    for (const auto& child : node.getChildren()) {
        if (child) {
            collectDescendants<T>(*child, out);
        }
    }
}

class UILayoutIntegrationTest : public ::testing::Test {
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
    std::unique_ptr<engine::ui::UIPresetManager> ui_preset_manager_{};
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
            GTEST_SKIP() << "Unable to locate runtime assets directory for layout integration test.";
        }
        {
            std::error_code ec;
            std::filesystem::current_path(runtime_root_, ec);
            if (ec) {
                GTEST_SKIP() << "Failed to switch working directory to runtime root.";
            }
        }

        window_ = SDL_CreateWindow("UILayoutIntegrationTest", 640, 360, SDL_WINDOW_HIDDEN);
        if (!window_) {
            GTEST_SKIP() << "Failed to create SDL window.";
        }

        game_state_ = engine::core::GameState::create(window_);
        if (!game_state_) {
            GTEST_SKIP() << "Failed to create GameState.";
        }
        game_state_->setWindowSize({640.0F, 360.0F});
        game_state_->setLogicalSize({640.0F, 360.0F});

        const auto timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        input_config_path_ =
            std::filesystem::temp_directory_path() / ("ui_layout_integration_input_" + std::to_string(timestamp) + ".json");
        std::ofstream input_config(input_config_path_);
        ASSERT_TRUE(input_config.is_open());
        input_config << R"({"input_mappings":{"mouse_left":["MouseLeft"],"mouse_right":["MouseRight"]}})";
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
        auto& preset_manager = *ui_preset_manager_;

        ASSERT_TRUE(preset_manager.registerImagePreset(
            entt::hashed_string{"inventory_panel"}.value(),
            makeTestPresetImage("inventory_panel",
                                {48.0F, 48.0F},
                                engine::render::NineSliceMargins{10.0F, 10.0F, 10.0F, 10.0F})));
        ASSERT_TRUE(preset_manager.registerImagePreset(
            entt::hashed_string{"inventory_slot"}.value(),
            makeTestPresetImage("inventory_slot",
                                {18.0F, 18.0F},
                                engine::render::NineSliceMargins{2.0F, 2.0F, 2.0F, 2.0F})));
        ASSERT_TRUE(preset_manager.registerImagePreset(
            entt::hashed_string{"quick_bar_panel"}.value(),
            makeTestPresetImage("quick_bar_panel",
                                {164.0F, 28.0F},
                                engine::render::NineSliceMargins{7.0F, 7.0F, 7.0F, 6.0F})));
        ASSERT_TRUE(preset_manager.registerImagePreset(
            entt::hashed_string{"quick_bar_slot"}.value(),
            makeTestPresetImage("quick_bar_slot",
                                {18.0F, 18.0F},
                                engine::render::NineSliceMargins{2.0F, 2.0F, 2.0F, 3.0F})));
        ASSERT_TRUE(preset_manager.registerImagePreset(
            entt::hashed_string{"quick_bar_selected"}.value(),
            makeTestPresetImage("quick_bar_selected",
                                {18.0F, 18.0F},
                                engine::render::NineSliceMargins{4.0F, 4.0F, 4.0F, 4.0F})));

        auto make_button_skin = [](std::string_view key, const glm::vec2& size) {
            engine::ui::UIButtonSkin skin{};
            const auto image = makeTestPresetImage(key, size);
            skin.normal_image = image;
            skin.hover_image = image;
            skin.pressed_image = image;
            skin.disabled_image = image;
            return skin;
        };
        ASSERT_TRUE(preset_manager.registerButtonPreset(
            entt::hashed_string{"close"}.value(), make_button_skin("close", {16.0F, 16.0F})));
        ASSERT_TRUE(preset_manager.registerButtonPreset(
            entt::hashed_string{"page_left"}.value(), make_button_skin("page_left", {20.0F, 20.0F})));
        ASSERT_TRUE(preset_manager.registerButtonPreset(
            entt::hashed_string{"page_right"}.value(), make_button_skin("page_right", {20.0F, 20.0F})));

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
            *resource_manager_, auto_tile_library_, *ui_preset_manager_
        };
        context_ = engine::core::Context::create(
            core_services, render_services, resource_services,
            *audio_player_, spatial_index_manager_
#ifdef TF_ENABLE_DEBUG_UI
            , *debug_ui_manager_
#endif
        );
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
        main_thread_command_queue_.reset();
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
        drainPendingEvents();
    }

    static void drainPendingEvents() {
        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
        }
    }
};

TEST_F(UILayoutIntegrationTest, InventoryGridAndPaginationAreaKeepExpectedLayoutRelation) {
    auto root = std::make_unique<engine::ui::UIElement>(glm::vec2{0.0F, 0.0F}, game_state_->getLogicalSize());
    auto inventory = std::make_unique<game::ui::InventoryUI>(*context_, nullptr);
    auto* inventory_ptr = inventory.get();
    inventory_ptr->show();
    root->addChild(std::move(inventory));

    forceLayoutTree(*root);

    std::vector<engine::ui::UIItemSlot*> slots{};
    collectDescendants(*inventory_ptr, slots);
    ASSERT_EQ(slots.size(), 20u);

    float min_x = std::numeric_limits<float>::max();
    float min_y = std::numeric_limits<float>::max();
    float max_bottom = std::numeric_limits<float>::lowest();
    for (auto* slot : slots) {
        const auto bounds = slot->getBounds();
        EXPECT_NEAR(bounds.size.x, 32.0F, kEpsilon);
        EXPECT_NEAR(bounds.size.y, 32.0F, kEpsilon);
        min_x = std::min(min_x, bounds.pos.x);
        min_y = std::min(min_y, bounds.pos.y);
        max_bottom = std::max(max_bottom, bounds.pos.y + bounds.size.y);
    }

    float nearest_right_x = std::numeric_limits<float>::max();
    float nearest_down_y = std::numeric_limits<float>::max();
    for (auto* slot : slots) {
        const auto bounds = slot->getBounds();
        if (std::fabs(bounds.pos.y - min_y) <= kEpsilon && bounds.pos.x > min_x + kEpsilon) {
            nearest_right_x = std::min(nearest_right_x, bounds.pos.x);
        }
        if (std::fabs(bounds.pos.x - min_x) <= kEpsilon && bounds.pos.y > min_y + kEpsilon) {
            nearest_down_y = std::min(nearest_down_y, bounds.pos.y);
        }
    }

    ASSERT_LT(nearest_right_x, std::numeric_limits<float>::max());
    ASSERT_LT(nearest_down_y, std::numeric_limits<float>::max());
    EXPECT_NEAR(nearest_right_x - min_x, 38.0F, 0.05F); // slot 32 + spacing 6
    EXPECT_NEAR(nearest_down_y - min_y, 38.0F, 0.05F);  // slot 32 + spacing 6

    std::vector<engine::ui::UIButton*> page_buttons{};
    collectDescendants(*inventory_ptr, page_buttons);
    page_buttons.erase(std::remove_if(page_buttons.begin(),
                                      page_buttons.end(),
                                      [](engine::ui::UIButton* btn) {
                                          const auto bounds = btn->getBounds();
                                          return std::fabs(bounds.size.x - 20.0F) > 0.1F ||
                                                 std::fabs(bounds.size.y - 20.0F) > 0.1F;
                                      }),
                       page_buttons.end());
    ASSERT_EQ(page_buttons.size(), 2u);

    std::vector<engine::ui::UILabel*> labels{};
    collectDescendants(*inventory_ptr, labels);
    std::vector<engine::ui::UILabel*> page_labels{};
    for (auto* label : labels) {
        const std::string_view text = label->getText();
        if (text.find('/') != std::string_view::npos) {
            page_labels.push_back(label);
        }
    }
    ASSERT_EQ(page_labels.size(), 1u);

    auto left_right = page_buttons;
    std::sort(left_right.begin(), left_right.end(), [](engine::ui::UIButton* lhs, engine::ui::UIButton* rhs) {
        return centerX(lhs->getBounds()) < centerX(rhs->getBounds());
    });

    const auto left_bounds = left_right[0]->getBounds();
    const auto right_bounds = left_right[1]->getBounds();
    const auto label_bounds = page_labels[0]->getBounds();

    const float expected_label_center_x = (centerX(left_bounds) + centerX(right_bounds)) * 0.5F;
    EXPECT_NEAR(centerX(label_bounds), expected_label_center_x, 1.5F);
    EXPECT_NEAR(centerY(label_bounds), centerY(left_bounds), 1.5F);

    EXPECT_GT(left_bounds.pos.y, max_bottom);
    EXPECT_GT(right_bounds.pos.y, max_bottom);
    EXPECT_GT(label_bounds.pos.y, max_bottom);
}

TEST_F(UILayoutIntegrationTest, HotbarRmlDocumentKeepsHorizontalSpacingAndPanelAnchor) {
    auto* layer = gl_renderer_->getRmlUILayer();
    if (!layer) {
        GTEST_SKIP() << "RmlUILayer not available in headless layout test environment.";
    }
    auto* rml_context = layer->getContext();
    if (!rml_context) {
        GTEST_SKIP() << "RmlUi context not available in headless layout test environment.";
    }

    constexpr uint64_t kOwnerSceneId = 4242;
    const int initial_document_count = rml_context->GetNumDocuments();

    {
        game::ui::HotbarUI hotbar(*layer, *context_, kOwnerSceneId, nullptr);
        ASSERT_TRUE(hotbar.isReady());

        layer->update();

        EXPECT_EQ(rml_context->GetNumDocuments(), initial_document_count + 1);

        auto* document = findDocumentByElementId(*rml_context, "hotbar-panel");
        ASSERT_NE(document, nullptr);

        auto* panel = document->GetElementById("hotbar-panel");
        auto* slots_container = document->GetElementById("hotbar-slots");
        ASSERT_NE(panel, nullptr);
        ASSERT_NE(slots_container, nullptr);

        constexpr float expected_panel_width = 372.0F;
        constexpr float expected_panel_height = 48.0F;
        constexpr float expected_panel_x = 134.0F;
        constexpr float expected_panel_y = 307.0F;

        EXPECT_NEAR(panel->GetOffsetWidth(), expected_panel_width, 0.1F);
        EXPECT_NEAR(panel->GetOffsetHeight(), expected_panel_height, 0.1F);
        EXPECT_NEAR(panel->GetAbsoluteLeft(), expected_panel_x, 0.1F);
        EXPECT_NEAR(panel->GetAbsoluteTop(), expected_panel_y, 0.1F);

        ASSERT_EQ(slots_container->GetNumChildren(), game::component::HotbarComponent::SLOT_COUNT);

        float previous_left = 0.0F;
        float first_top = 0.0F;
        for (int index = 0; index < slots_container->GetNumChildren(); ++index) {
            auto* slot = slots_container->GetChild(index);
            ASSERT_NE(slot, nullptr);
            EXPECT_NEAR(slot->GetOffsetWidth(), 32.0F, 0.1F);
            EXPECT_NEAR(slot->GetOffsetHeight(), 32.0F, 0.1F);

            if (index == 0) {
                first_top = slot->GetAbsoluteTop();
            } else {
                EXPECT_NEAR(slot->GetAbsoluteLeft() - previous_left, 36.0F, 0.1F);
                EXPECT_NEAR(slot->GetAbsoluteTop(), first_top, 0.1F);
            }
            previous_left = slot->GetAbsoluteLeft();
        }
    }

    layer->update();
    EXPECT_EQ(rml_context->GetNumDocuments(), initial_document_count);
}

} // namespace
} // namespace game::ui
