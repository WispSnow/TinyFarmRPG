#include "game_overlay.h"

#include "engine/ui/rmlui/rml_ui_runtime.h"

#include <spdlog/spdlog.h>

#include <string_view>
#include <utility>

namespace {

constexpr std::string_view DOCUMENT_PATH = "ui/rmlui/hud/game_overlay.rml";
constexpr std::string_view MODEL_NAME = "game_overlay";

} // namespace

namespace game::ui {

GameOverlay::GameOverlay(engine::ui::rmlui::RmlUiRuntime& runtime,
                         uint64_t owner_scene_id,
                         MenuRequestHandler on_menu_requested) {
    document_controller_.attach(&runtime, owner_scene_id);
    auto constructor = document_controller_.createModel(MODEL_NAME);
    if (!constructor) {
        spdlog::error("GameOverlay: failed to create data model '{}'.", MODEL_NAME);
        return;
    }

    if (!document_controller_.bindSimpleEvent(constructor, "menu", [handler = std::move(on_menu_requested)] {
            if (handler) {
                handler();
            }
        })) {
        spdlog::error("GameOverlay: failed to bind menu event.");
        document_controller_.unload();
        return;
    }

    if (!document_controller_.load(DOCUMENT_PATH)) {
        spdlog::error("GameOverlay: failed to load '{}'.", DOCUMENT_PATH);
        document_controller_.unload();
        return;
    }
}

GameOverlay::~GameOverlay() {
    document_controller_.unload();
}

} // namespace game::ui
