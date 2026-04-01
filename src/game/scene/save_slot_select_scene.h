#pragma once

#include "engine/scene/scene.h"
#include "engine/ui/rmlui/rml_document_controller.h"

#include <RmlUi/Core/Types.h>

#include <functional>
#include <optional>
#include <string_view>
#include <vector>

namespace Rml {
class Element;
class Event;
class DataModelHandle;
class DataModelConstructor;
}

namespace game::scene {

class SaveSlotSelectScene final : public engine::scene::Scene {
public:
    using SlotSelectCallback = std::function<void(int slot)>;

    enum class Mode {
        Load,
        Save,
    };

private:
    struct SlotViewModel {
        int slot_index{0};
        Rml::String label{};
        bool enabled{true};
    };

    SlotSelectCallback on_select_{};
    Mode mode_{Mode::Load};
    std::optional<int> pending_overwrite_slot_{};
    bool context_pushed_{false};

    engine::ui::rmlui::RmlDocumentController document_controller_{};
    Rml::Element* focus_before_confirm_{nullptr};

    std::vector<SlotViewModel> slots_{};
    bool confirm_visible_{false};
    Rml::String confirm_text_{"Overwrite?"};

public:
    SaveSlotSelectScene(std::string_view name,
                        engine::core::Context& context,
                        SlotSelectCallback on_select = {},
                        Mode mode = Mode::Load);
    ~SaveSlotSelectScene() override;

    bool init() override;
    void clean() override;

private:
    [[nodiscard]] bool initUI();
    void shutdownUI();
    [[nodiscard]] bool ensureDataTypesRegistered(Rml::DataModelConstructor& constructor);
    void disconnectRuntimeListeners();
    void refreshSlotButtons();
    void queueDefaultFocus();
    [[nodiscard]] bool shouldSyncHoverFocus(Rml::Element* element) const;

    void onSlotClicked(int slot);
    void onBackClicked();
    bool onMenuCancelPressed();

    void showOverwriteConfirm(int slot);
    void hideOverwriteConfirm();
    void onOverwriteConfirmYes();
    void onOverwriteConfirmNo();

    void onSlotSelectEvent(Rml::DataModelHandle model, Rml::Event& event, const Rml::VariantList& arguments);
};

} // namespace game::scene
