#pragma once

#include "engine/scene/scene.h"
#include "engine/ui/rmlui/rml_document_controller.h"

#include <RmlUi/Core/Types.h>
#include <string>
#include <string_view>

namespace game::data {
struct GameTime;
}

namespace game::scene {

class TitleScene final : public engine::scene::Scene {
    std::shared_ptr<game::data::GameTime> title_game_time_{};
    std::string error_message_{};

    engine::ui::rmlui::RmlDocumentController document_controller_{};
    bool context_pushed_{false};

    Rml::String error_text_{};
    bool show_error_{false};

public:
    TitleScene(std::string_view name, engine::core::Context& context, std::string error_message = {});
    ~TitleScene() override;

    bool init() override;
    void clean() override;

private:
    [[nodiscard]] bool initUI();
    void shutdownUI();

    void onStartClicked();
    void onLoadClicked();
    void onMenuClicked();
    void onExitClicked();
};

} // namespace game::scene
