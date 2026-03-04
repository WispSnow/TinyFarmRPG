#pragma once

#include "engine/scene/scene.h"
#include "engine/ui/rmlui/rml_data_bridge.h"
#include "engine/ui/rmlui/rml_event_bridge.h"

#include <RmlUi/Core/Types.h>

#include <memory>
#include <string>
#include <string_view>

namespace Rml {
class ElementDocument;
}

namespace game::data {
struct GameTime;
} // namespace game::data

namespace game::scene {

class TitleScene final : public engine::scene::Scene {
    std::shared_ptr<game::data::GameTime> title_game_time_{};
    std::string error_message_{};

    engine::ui::rmlui::RmlDataBridge data_bridge_{};
    engine::ui::rmlui::RmlEventBridge event_bridge_{};
    Rml::ElementDocument* document_{nullptr};
    bool click_listener_registered_{false};

    Rml::String error_text_{};
    bool has_error_{false};

public:
    TitleScene(std::string_view name, engine::core::Context& context, std::string error_message = {});
    ~TitleScene() override = default;

    bool init() override;
    void update(float delta_time) override;
    void clean() override;

private:
    [[nodiscard]] bool initUI();
    void bindEvents();
    void removeEventListeners();

    void onStartClicked();
    void onLoadClicked();
    void onMenuClicked();
    void onExitClicked();
};

} // namespace game::scene
