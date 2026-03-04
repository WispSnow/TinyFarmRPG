#pragma once

#include "engine/scene/scene.h"
#include "engine/ui/rmlui/rml_data_bridge.h"
#include "engine/ui/rmlui/rml_event_bridge.h"

#include <RmlUi/Core/Types.h>

#include <string_view>

namespace engine::core {
enum class State;
}

namespace Rml {
class ElementDocument;
}

namespace game::scene {

class RestDialogScene final : public engine::scene::Scene {
private:
    engine::core::State previous_state_{};
    int selected_hours_{8};

    engine::ui::rmlui::RmlDataBridge data_bridge_{};
    engine::ui::rmlui::RmlEventBridge event_bridge_{};
    Rml::ElementDocument* document_{nullptr};
    bool click_listener_registered_{false};

    Rml::String hours_text_{"8h"};

public:
    RestDialogScene(std::string_view name, engine::core::Context& context);
    ~RestDialogScene() override = default;

    bool init() override;
    void clean() override;

private:
    [[nodiscard]] bool initUI();
    void bindEvents();
    void removeEventListeners();

    void updateHoursLabel();
    void adjustHours(int delta);

    void onConfirm();
    void onCancel();
};

} // namespace game::scene
