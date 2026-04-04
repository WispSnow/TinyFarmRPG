#include "game_input_prompt_overlay.h"

#include "engine/input/input_manager.h"
#include "engine/ui/rmlui/rml_bind_helpers.h"
#include "engine/ui/rmlui/rml_ui_runtime.h"

#include <entt/core/hashed_string.hpp>
#include <spdlog/spdlog.h>

#include <string>
#include <string_view>

using namespace entt::literals;

namespace {

constexpr std::string_view DOCUMENT_PATH = "ui/rmlui/hud/game_input_prompt_overlay.rml";
constexpr std::string_view MODEL_NAME = "input_prompt_overlay";

using engine::ui::rmlui::updateBoundString;

[[nodiscard]] std::string promptTextForAction(engine::input::InputManager& input_manager, entt::id_type action_id) {
    if (const auto prompt = input_manager.getActionPrompt(action_id); prompt.has_value()) {
        return prompt->fallback_text;
    }

    return "-";
}

} // namespace

namespace game::ui {

GameInputPromptOverlay::GameInputPromptOverlay(engine::ui::rmlui::RmlUiRuntime& runtime,
                                               engine::input::InputManager& input_manager,
                                               uint64_t owner_scene_id)
    : input_manager_(input_manager) {
    document_controller_.attach(&runtime, owner_scene_id);
    auto constructor = document_controller_.createModel(MODEL_NAME);
    if (!constructor) {
        spdlog::error("GameInputPromptOverlay: failed to create data model '{}'.", MODEL_NAME);
        return;
    }

    constructor.Bind("primary_prompt_text", &primary_prompt_text_);
    constructor.Bind("secondary_prompt_text", &secondary_prompt_text_);
    constructor.Bind("inventory_prompt_text", &inventory_prompt_text_);
    constructor.Bind("pause_prompt_text", &pause_prompt_text_);
    constructor.Bind("toggle_prompt_text", &toggle_prompt_text_);
    constructor.Bind("visible", &visible_);

    if (!document_controller_.load(DOCUMENT_PATH)) {
        spdlog::error("GameInputPromptOverlay: failed to load '{}'.", DOCUMENT_PATH);
        document_controller_.unload();
        return;
    }

    refreshPromptTexts();
    document_controller_.markAllDirty();
    spdlog::debug("GameInputPromptOverlay 初始化完成。");
}

GameInputPromptOverlay::~GameInputPromptOverlay() {
    document_controller_.unload();
}

void GameInputPromptOverlay::update() {
    if (!visible_ || !document_controller_.isModelValid()) {
        return;
    }

    refreshPromptTexts();
}

void GameInputPromptOverlay::setVisible(bool visible) {
    if (visible_ == visible) {
        return;
    }

    visible_ = visible;
    if (!document_controller_.isModelValid()) {
        return;
    }

    if (visible_) {
        refreshPromptTexts();
    }
    document_controller_.markDirty("visible");
}

void GameInputPromptOverlay::toggleVisible() {
    setVisible(!visible_);
}

void GameInputPromptOverlay::refreshPromptTexts() {
    const auto refresh_prompt = [this](std::string_view variable_name, Rml::String& text, entt::id_type action_id) {
        if (updateBoundString(text, promptTextForAction(input_manager_, action_id))) {
            document_controller_.markDirty(variable_name);
        }
    };

    refresh_prompt("primary_prompt_text", primary_prompt_text_, "primary_action"_hs);
    refresh_prompt("secondary_prompt_text", secondary_prompt_text_, "secondary_action"_hs);
    refresh_prompt("inventory_prompt_text", inventory_prompt_text_, "inventory"_hs);
    refresh_prompt("pause_prompt_text", pause_prompt_text_, "pause"_hs);
    refresh_prompt("toggle_prompt_text", toggle_prompt_text_, "toggle_prompt_bar"_hs);
}

} // namespace game::ui
