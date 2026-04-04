#include <gtest/gtest.h>

#include <SDL3/SDL.h>
#include <entt/signal/dispatcher.hpp>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Property.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
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
#include "engine/ui/rmlui/rml_ui_runtime.h"
#include "engine/render/renderer.h"
#include "engine/render/text_renderer.h"
#include "engine/resource/auto_tile_library.h"
#include "engine/resource/resource_manager.h"
#include "engine/spatial/spatial_index_manager.h"
#include "game/component/hotbar_component.h"
#include "game/component/inventory_component.h"
#include "game/scene/inventory_menu_scene.h"
#include "game/ui/hotbar_ui.h"

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
    auto* runtime = context_->getRmlUi();
    if (!runtime) {
        GTEST_SKIP() << "RmlUiRuntime not available in headless layout test environment.";
    }
    auto* rml_context = runtime->getContext();
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
    runtime->update();

    auto* target = target_document->GetElementById("probe-target");
    ASSERT_NE(target, nullptr);

    CountingEventListener dragdrop_listener{};
    target->AddEventListener(Rml::EventId::Dragdrop, &dragdrop_listener);

    EXPECT_TRUE(rml_context->ProcessMouseMove(16, 16, 0));
    runtime->update();
    EXPECT_TRUE(rml_context->ProcessMouseButtonDown(0, 0));
    runtime->update();
    EXPECT_TRUE(rml_context->ProcessMouseMove(176, 16, 0));
    runtime->update();
    EXPECT_TRUE(rml_context->ProcessMouseButtonUp(0, 0));
    runtime->update();

    EXPECT_EQ(dragdrop_listener.count, 1);

    target->RemoveEventListener(Rml::EventId::Dragdrop, &dragdrop_listener);
    source_document->Close();
    target_document->Close();
    runtime->update();
}

TEST_F(UILayoutIntegrationTest, InventoryMenuSceneRmlDocumentKeepsGridAndToolbarLayout) {
    auto* runtime = context_->getRmlUi();
    if (!runtime) {
        GTEST_SKIP() << "RmlUiRuntime not available in headless layout test environment.";
    }
    auto* rml_context = runtime->getContext();
    if (!rml_context) {
        GTEST_SKIP() << "RmlUi context not available in headless layout test environment.";
    }

    const int initial_document_count = rml_context->GetNumDocuments();

    {
        entt::registry registry;
        const entt::entity player = registry.create();
        registry.emplace<game::component::InventoryComponent>(player);
        registry.emplace<game::component::HotbarComponent>(player);

        game::scene::InventoryMenuScene menu("InventoryMenu", *context_, registry, player, nullptr);
        ASSERT_TRUE(menu.init());

        runtime->update();

        EXPECT_EQ(rml_context->GetNumDocuments(), initial_document_count + 1);

        auto* document = findDocumentByElementId(*rml_context, "menu-panel");
        ASSERT_NE(document, nullptr);

        auto* panel = document->GetElementById("menu-panel");
        auto* hotbar_grid = document->GetElementById("hotbar-grid");
        auto* backpack_grid = document->GetElementById("backpack-grid");
        auto* sort_button = document->GetElementById("sort-btn");
        auto* trash_button = document->GetElementById("trash-btn");
        ASSERT_NE(panel, nullptr);
        ASSERT_NE(hotbar_grid, nullptr);
        ASSERT_NE(backpack_grid, nullptr);
        ASSERT_NE(sort_button, nullptr);
        ASSERT_NE(trash_button, nullptr);

        ASSERT_EQ(hotbar_grid->GetNumChildren(), game::component::HotbarComponent::SLOT_COUNT);
        ASSERT_EQ(backpack_grid->GetNumChildren(), game::component::InventoryComponent::TOTAL_SLOTS);

        auto* hotbar_slot0 = hotbar_grid->GetChild(0);
        auto* hotbar_slot1 = hotbar_grid->GetChild(1);
        auto* backpack_slot0 = backpack_grid->GetChild(0);
        auto* backpack_slot1 = backpack_grid->GetChild(1);
        auto* backpack_slot10 = backpack_grid->GetChild(10);
        ASSERT_NE(hotbar_slot0, nullptr);
        ASSERT_NE(hotbar_slot1, nullptr);
        ASSERT_NE(backpack_slot0, nullptr);
        ASSERT_NE(backpack_slot1, nullptr);
        ASSERT_NE(backpack_slot10, nullptr);

        EXPECT_NEAR(hotbar_slot0->GetOffsetWidth(), 20.0F, 0.1F);
        EXPECT_NEAR(hotbar_slot0->GetOffsetHeight(), 20.0F, 0.1F);
        EXPECT_NEAR(hotbar_slot1->GetAbsoluteLeft() - hotbar_slot0->GetAbsoluteLeft(), 22.0F, 0.2F);

        EXPECT_NEAR(backpack_slot0->GetOffsetWidth(), 20.0F, 0.1F);
        EXPECT_NEAR(backpack_slot0->GetOffsetHeight(), 20.0F, 0.1F);
        EXPECT_NEAR(backpack_slot1->GetAbsoluteLeft() - backpack_slot0->GetAbsoluteLeft(), 22.0F, 0.2F);
        EXPECT_NEAR(backpack_slot10->GetAbsoluteLeft(), backpack_slot0->GetAbsoluteLeft(), 0.2F);
        EXPECT_NEAR(backpack_slot10->GetAbsoluteTop() - backpack_slot0->GetAbsoluteTop(), 22.0F, 0.2F);

        EXPECT_GT(sort_button->GetAbsoluteLeft(), centerX(hotbar_slot1));
        EXPECT_GT(trash_button->GetAbsoluteLeft(), sort_button->GetAbsoluteLeft());

        menu.clean();
    }

    runtime->update();
    EXPECT_EQ(rml_context->GetNumDocuments(), initial_document_count);
}

TEST_F(UILayoutIntegrationTest, HotbarRmlDocumentKeepsHorizontalSpacingAndPanelAnchor) {
    auto* runtime = context_->getRmlUi();
    if (!runtime) {
        GTEST_SKIP() << "RmlUiRuntime not available in headless layout test environment.";
    }
    auto* rml_context = runtime->getContext();
    if (!rml_context) {
        GTEST_SKIP() << "RmlUi context not available in headless layout test environment.";
    }

    constexpr uint64_t kOwnerSceneId = 4242;
    const int initial_document_count = rml_context->GetNumDocuments();

    {
        game::ui::HotbarUI hotbar(*runtime, *context_, kOwnerSceneId, nullptr);
        ASSERT_TRUE(hotbar.isReady());

        runtime->update();

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

    runtime->update();
    EXPECT_EQ(rml_context->GetNumDocuments(), initial_document_count);
}

TEST_F(UILayoutIntegrationTest, EmptySlotBindingsUseNoneDecoratorInsteadOfEmptyInlineStyle) {
    auto* runtime = context_->getRmlUi();
    if (!runtime) {
        GTEST_SKIP() << "RmlUiRuntime not available in headless layout test environment.";
    }
    auto* rml_context = runtime->getContext();
    if (!rml_context) {
        GTEST_SKIP() << "RmlUi context not available in headless layout test environment.";
    }

    entt::registry registry;
    const entt::entity player = registry.create();
    registry.emplace<game::component::InventoryComponent>(player);
    registry.emplace<game::component::HotbarComponent>(player);

    game::scene::InventoryMenuScene menu("InventoryMenu", *context_, registry, player, nullptr);
    ASSERT_TRUE(menu.init());
    runtime->update();

    auto* menu_document = findDocumentByElementId(*rml_context, "menu-panel");
    ASSERT_NE(menu_document, nullptr);

    auto* backpack_grid = menu_document->GetElementById("backpack-grid");
    auto* hotbar_grid = menu_document->GetElementById("hotbar-grid");
    ASSERT_NE(backpack_grid, nullptr);
    ASSERT_NE(hotbar_grid, nullptr);
    ASSERT_GT(backpack_grid->GetNumChildren(), 0);
    ASSERT_GT(hotbar_grid->GetNumChildren(), 0);

    auto* backpack_slot = backpack_grid->GetChild(0);
    ASSERT_NE(backpack_slot, nullptr);
    ASSERT_GT(backpack_slot->GetNumChildren(), 0);
    auto* backpack_drag_proxy = backpack_slot->GetChild(0);
    ASSERT_NE(backpack_drag_proxy, nullptr);
    ASSERT_GT(backpack_drag_proxy->GetNumChildren(), 0);
    auto* backpack_icon = backpack_drag_proxy->GetChild(0);
    ASSERT_NE(backpack_icon, nullptr);

    auto* hotbar_slot = hotbar_grid->GetChild(0);
    ASSERT_NE(hotbar_slot, nullptr);
    ASSERT_GT(hotbar_slot->GetNumChildren(), 0);
    auto* hotbar_drag_proxy = hotbar_slot->GetChild(0);
    ASSERT_NE(hotbar_drag_proxy, nullptr);
    ASSERT_GT(hotbar_drag_proxy->GetNumChildren(), 0);
    auto* hotbar_icon = hotbar_drag_proxy->GetChild(0);
    ASSERT_NE(hotbar_icon, nullptr);

    const Rml::Property* backpack_decorator = backpack_icon->GetLocalProperty("decorator");
    const Rml::Property* hotbar_decorator = hotbar_icon->GetLocalProperty("decorator");
    ASSERT_NE(backpack_decorator, nullptr);
    ASSERT_NE(hotbar_decorator, nullptr);

    EXPECT_EQ(backpack_decorator->ToString(), "none");
    EXPECT_EQ(hotbar_decorator->ToString(), "none");

    menu.clean();
}

TEST_F(UILayoutIntegrationTest, InventoryActionMenuAnchorsToSlotGeometryAndStaysInsideSlotRegion) {
    auto* runtime = context_->getRmlUi();
    if (!runtime) {
        GTEST_SKIP() << "RmlUiRuntime not available in headless layout test environment.";
    }
    auto* rml_context = runtime->getContext();
    if (!rml_context) {
        GTEST_SKIP() << "RmlUi context not available in headless layout test environment.";
    }

    entt::registry registry;
    const entt::entity player = registry.create();
    auto& inventory = registry.emplace<game::component::InventoryComponent>(player);
    registry.emplace<game::component::HotbarComponent>(player);
    inventory.slot(game::component::InventoryComponent::TOTAL_SLOTS - 1) = game::component::ItemStack{
        .item_id_ = 1,
        .count_ = 3,
    };

    game::scene::InventoryMenuScene menu("InventoryMenu", *context_, registry, player, nullptr);
    ASSERT_TRUE(menu.init());
    runtime->update();

    auto* document = findDocumentByElementId(*rml_context, "menu-panel");
    ASSERT_NE(document, nullptr);

    auto* slot_region = document->GetElementById("slot-region");
    auto* backpack_grid = document->GetElementById("backpack-grid");
    ASSERT_NE(slot_region, nullptr);
    ASSERT_NE(backpack_grid, nullptr);
    ASSERT_EQ(backpack_grid->GetNumChildren(), game::component::InventoryComponent::TOTAL_SLOTS);

    auto* last_slot = backpack_grid->GetChild(game::component::InventoryComponent::TOTAL_SLOTS - 1);
    ASSERT_NE(last_slot, nullptr);

    EXPECT_TRUE(rml_context->ProcessMouseMove(static_cast<int>(centerX(last_slot)), static_cast<int>(centerY(last_slot)), 0));
    runtime->update();
    EXPECT_TRUE(rml_context->ProcessMouseButtonDown(1, 0));
    runtime->update();

    auto* action_menu = document->GetElementById("action-menu");
    ASSERT_NE(action_menu, nullptr);

    const float region_left = slot_region->GetAbsoluteLeft();
    const float region_top = slot_region->GetAbsoluteTop();
    const float region_right = region_left + slot_region->GetOffsetWidth();
    const float region_bottom = region_top + slot_region->GetOffsetHeight();
    const float menu_left = action_menu->GetAbsoluteLeft();
    const float menu_top = action_menu->GetAbsoluteTop();
    const float menu_right = menu_left + action_menu->GetOffsetWidth();
    const float menu_bottom = menu_top + action_menu->GetOffsetHeight();

    EXPECT_GE(menu_left, region_left - 0.1F);
    EXPECT_GE(menu_top, region_top - 0.1F);
    EXPECT_LE(menu_right, region_right + 0.1F);
    EXPECT_LE(menu_bottom, region_bottom + 0.1F);
    EXPECT_LT(menu_left, last_slot->GetAbsoluteLeft())
        << "The menu should flip to the left when the slot is on the right edge of the grid.";

    menu.clean();
}

} // namespace
} // namespace game::ui
