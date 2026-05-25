#pragma once

#include "engine/scene/scene.h"
#include "engine/ui/rmlui/rml_document_controller.h"
#include "game/defs/events_dialogue.h"

#include <RmlUi/Core/DataTypeRegister.h>
#include <RmlUi/Core/Types.h>

#include <string_view>
#include <vector>

namespace Rml {
class DataModelConstructor;
class Event;
} // namespace Rml

namespace engine::core {
enum class State;
}

namespace game::scene {

class DialogueChoiceScene final : public engine::scene::Scene {
private:
    struct ChoiceViewModel {
        int option_index{0};
        Rml::String label{};
    };

    game::defs::DialogueChoiceRequestedEvent request_{};
    engine::core::State previous_state_{};
    bool context_pushed_{false};
    bool data_types_registered_{false};

    engine::ui::rmlui::RmlDocumentController document_controller_{};
    Rml::DataTypeRegister type_register_{};

    Rml::String speaker_text_{};
    Rml::String prompt_text_{};
    bool has_speaker_{false};
    bool allow_cancel_{true};
    std::vector<ChoiceViewModel> choices_{};

public:
    DialogueChoiceScene(std::string_view name,
                        engine::core::Context& context,
                        game::defs::DialogueChoiceRequestedEvent request);
    ~DialogueChoiceScene() override;

    bool init() override;
    void clean() override;
    [[nodiscard]] engine::scene::SceneUiCoverage uiCoverage() const override;

private:
    [[nodiscard]] bool initUI();
    [[nodiscard]] bool ensureDataTypesRegistered(Rml::DataModelConstructor& constructor);
    void shutdownUI();
    void connectRuntimeListeners();
    void disconnectRuntimeListeners();
    void refreshBindings();
    void focusDefaultAction();
    [[nodiscard]] int resolveClickedChoiceIndex(const Rml::Event& event) const;
    bool onMenuCancelPressed();
    void onChoose(int option_index);
    void onCancel();
};

} // namespace game::scene
