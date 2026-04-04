#pragma once

#include "engine/ui/rmlui/rml_document_controller.h"

#include <cstdint>
#include <functional>

namespace engine::ui::rmlui {
class RmlUiRuntime;
}

namespace game::ui {

/**
 * @brief 游戏场景常驻 overlay
 *
 * 负责右上角的菜单按钮，以及未来扩展的场景级按钮/信息区域。
 */
class GameOverlay final {
public:
    using MenuRequestHandler = std::function<void()>;

    GameOverlay(engine::ui::rmlui::RmlUiRuntime& runtime,
                uint64_t owner_scene_id,
                MenuRequestHandler on_menu_requested);
    ~GameOverlay();

    GameOverlay(const GameOverlay&) = delete;
    GameOverlay& operator=(const GameOverlay&) = delete;
    GameOverlay(GameOverlay&&) = delete;
    GameOverlay& operator=(GameOverlay&&) = delete;

    [[nodiscard]] bool isReady() const { return document_controller_.isModelValid(); }

private:
    engine::ui::rmlui::RmlDocumentController document_controller_{};
};

} // namespace game::ui
