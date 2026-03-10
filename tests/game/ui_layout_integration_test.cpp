#include <gtest/gtest.h>

#include <SDL3/SDL.h>
#include <entt/signal/dispatcher.hpp>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/EventListener.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
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
#include "game/component/hotbar_component.h"
#include "game/component/inventory_component.h"
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

float centerX(Rml::Element* element) {
    return element->GetAbsoluteLeft() + element->GetOffsetWidth() * 0.5F;
}

float centerY(Rml::Element* element) {
    return element->GetAbsoluteTop() + element->GetOffsetHeight() * 0.5F;
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

class CountingEventListener final : public Rml::EventListener {
public:
    void ProcessEvent(Rml::Event&) override {
        ++count;
    }

    int count{0};
};

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
        input_config << R"({"input_mappings":{"primary_action":["MouseLeft"],"secondary_action":["MouseRight"]}})";
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
            *resource_manager_, auto_tile_library_
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

TEST_F(UILayoutIntegrationTest, RmlContextSupportsCrossDocumentDragCloneDrop) {
    auto* layer = gl_renderer_->getRmlUILayer();
    if (!layer) {
        GTEST_SKIP() << "RmlUILayer not available in headless layout test environment.";
    }
    auto* rml_context = layer->getContext();
    if (!rml_context) {
        GTEST_SKIP() << "RmlUi context not available in headless layout test environment.";
    }

    static constexpr std::string_view kSourceDocument = R"(
<rml>
<head>
    <style>
        body, div { display: block; }
        body {
            position: absolute;
            left: 0px;
            top: 0px;
            width: 32px;
            height: 32px;
            margin: 0;
        }
        #probe-source {
            width: 32px;
            height: 32px;
            background-color: #ffffffff;
            drag: clone;
        }
    </style>
</head>
<body>
    <div id="probe-source"></div>
</body>
</rml>
)";

    static constexpr std::string_view kTargetDocument = R"(
<rml>
<head>
    <style>
        body, div { display: block; }
        body {
            position: absolute;
            left: 160px;
            top: 0px;
            width: 32px;
            height: 32px;
            margin: 0;
        }
        #probe-target {
            width: 32px;
            height: 32px;
            background-color: #00ff00ff;
        }
    </style>
</head>
<body>
    <div id="probe-target"></div>
</body>
</rml>
)";

    auto* source_document = rml_context->LoadDocumentFromMemory(kSourceDocument.data(), ".");
    auto* target_document = rml_context->LoadDocumentFromMemory(kTargetDocument.data(), ".");
    ASSERT_NE(source_document, nullptr);
    ASSERT_NE(target_document, nullptr);

    source_document->Show();
    target_document->Show();
    layer->update();

    auto* target = target_document->GetElementById("probe-target");
    ASSERT_NE(target, nullptr);

    CountingEventListener dragdrop_listener{};
    target->AddEventListener(Rml::EventId::Dragdrop, &dragdrop_listener);

    EXPECT_TRUE(rml_context->ProcessMouseMove(16, 16, 0));
    layer->update();
    EXPECT_TRUE(rml_context->ProcessMouseButtonDown(0, 0));
    layer->update();
    EXPECT_TRUE(rml_context->ProcessMouseMove(176, 16, 0));
    layer->update();
    EXPECT_TRUE(rml_context->ProcessMouseButtonUp(0, 0));
    layer->update();

    EXPECT_EQ(dragdrop_listener.count, 1);

    target->RemoveEventListener(Rml::EventId::Dragdrop, &dragdrop_listener);
    source_document->Close();
    target_document->Close();
    layer->update();
}

TEST_F(UILayoutIntegrationTest, InventoryRmlDocumentKeepsGridAndPaginationLayout) {
    auto* layer = gl_renderer_->getRmlUILayer();
    if (!layer) {
        GTEST_SKIP() << "RmlUILayer not available in headless layout test environment.";
    }
    auto* rml_context = layer->getContext();
    if (!rml_context) {
        GTEST_SKIP() << "RmlUi context not available in headless layout test environment.";
    }

    constexpr uint64_t kOwnerSceneId = 4343;
    const int initial_document_count = rml_context->GetNumDocuments();

    {
        game::ui::InventoryUI inventory(*layer, *context_, kOwnerSceneId, nullptr);
        ASSERT_TRUE(inventory.isReady());
        inventory.show();

        layer->update();

        EXPECT_EQ(rml_context->GetNumDocuments(), initial_document_count + 1);

        auto* document = findDocumentByElementId(*rml_context, "inventory-panel");
        ASSERT_NE(document, nullptr);

        auto* panel = document->GetElementById("inventory-panel");
        auto* grid = document->GetElementById("inventory-grid");
        auto* pagination = document->GetElementById("inventory-pagination");
        auto* page_left = document->GetElementById("inventory-page-left");
        auto* page_right = document->GetElementById("inventory-page-right");
        auto* page_label = document->GetElementById("inventory-page-label");
        ASSERT_NE(panel, nullptr);
        ASSERT_NE(grid, nullptr);
        ASSERT_NE(pagination, nullptr);
        ASSERT_NE(page_left, nullptr);
        ASSERT_NE(page_right, nullptr);
        ASSERT_NE(page_label, nullptr);

        EXPECT_NEAR(panel->GetOffsetWidth(), 208.0F, 0.1F);
        EXPECT_NEAR(panel->GetOffsetHeight(), 186.0F, 0.1F);
        EXPECT_NEAR(panel->GetAbsoluteLeft(), 412.0F, 0.1F);
        EXPECT_NEAR(panel->GetAbsoluteTop(), 87.0F, 0.1F);

        ASSERT_EQ(grid->GetNumChildren(), game::component::InventoryComponent::SLOTS_PER_PAGE);

        float max_slot_bottom = std::numeric_limits<float>::lowest();
        for (int index = 0; index < grid->GetNumChildren(); ++index) {
            auto* slot = grid->GetChild(index);
            ASSERT_NE(slot, nullptr);
            EXPECT_NEAR(slot->GetOffsetWidth(), 32.0F, 0.1F);
            EXPECT_NEAR(slot->GetOffsetHeight(), 32.0F, 0.1F);
            max_slot_bottom = std::max(max_slot_bottom, slot->GetAbsoluteTop() + slot->GetOffsetHeight());
        }

        auto* slot0 = grid->GetChild(0);
        auto* slot1 = grid->GetChild(1);
        auto* slot5 = grid->GetChild(5);
        ASSERT_NE(slot0, nullptr);
        ASSERT_NE(slot1, nullptr);
        ASSERT_NE(slot5, nullptr);

        EXPECT_NEAR(slot1->GetAbsoluteLeft() - slot0->GetAbsoluteLeft(), 38.0F, 0.1F);
        EXPECT_NEAR(slot1->GetAbsoluteTop(), slot0->GetAbsoluteTop(), 0.1F);
        EXPECT_NEAR(slot5->GetAbsoluteLeft(), slot0->GetAbsoluteLeft(), 0.1F);
        EXPECT_NEAR(slot5->GetAbsoluteTop() - slot0->GetAbsoluteTop(), 38.0F, 0.1F);

        EXPECT_NEAR(page_left->GetOffsetWidth(), 20.0F, 0.1F);
        EXPECT_NEAR(page_left->GetOffsetHeight(), 20.0F, 0.1F);
        EXPECT_NEAR(page_right->GetOffsetWidth(), 20.0F, 0.1F);
        EXPECT_NEAR(page_right->GetOffsetHeight(), 20.0F, 0.1F);

        const float expected_label_center_x = (centerX(page_left) + centerX(page_right)) * 0.5F;
        EXPECT_NEAR(centerX(page_label), expected_label_center_x, 0.2F);
        EXPECT_NEAR(centerY(page_label), centerY(page_left), 0.2F);

        EXPECT_GT(page_left->GetAbsoluteTop(), max_slot_bottom);
        EXPECT_GT(page_right->GetAbsoluteTop(), max_slot_bottom);
        EXPECT_GT(page_label->GetAbsoluteTop(), max_slot_bottom);
        EXPECT_NEAR(pagination->GetAbsoluteTop() - panel->GetAbsoluteTop(), 162.0F, 0.1F);
    }

    layer->update();
    EXPECT_EQ(rml_context->GetNumDocuments(), initial_document_count);
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
