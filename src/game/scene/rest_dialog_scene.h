#pragma once

#include "engine/scene/scene.h"
#include "engine/ui/rmlui/rml_document_controller.h"

#include <RmlUi/Core/Types.h>

#include <string_view>

namespace engine::core {
enum class State;
}

namespace game::scene {

class RestDialogScene final : public engine::scene::Scene {
private:
    engine::core::State previous_state_{};
    int selected_hours_{24};
    bool context_pushed_{false};

    engine::ui::rmlui::RmlDocumentController document_controller_{};

    Rml::String hours_text_{"24h"};

public:
    RestDialogScene(std::string_view name, engine::core::Context& context);
    ~RestDialogScene() override;

    bool init() override;
    void clean() override;

private:
    [[nodiscard]] bool initUI();
    void shutdownUI();
    void disconnectRuntimeListeners();
    void updateHoursLabel();
    void adjustHours(int delta);
    bool onMenuCancelPressed();
    void onConfirm();
    void onCancel();
};

} // namespace game::scene
