#pragma once

#include "engine/scene/scene.h"
#include "engine/ui/rmlui/rml_data_bridge.h"
#include "engine/ui/rmlui/rml_event_bridge.h"

#include <RmlUi/Core/Types.h>

#include <functional>
#include <optional>
#include <string_view>

namespace Rml {
class ElementDocument;
class Event;
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
        bool enabled{false};
    };

    SlotSelectCallback on_select_{};
    Mode mode_{Mode::Load};
    std::optional<int> pending_overwrite_slot_{};

    engine::ui::rmlui::RmlDataBridge data_bridge_{};
    engine::ui::rmlui::RmlEventBridge event_bridge_{};
    Rml::ElementDocument* document_{nullptr};
    bool click_listener_registered_{false};

    Rml::Vector<SlotViewModel> slots_{};
    Rml::String panel_title_{};
    Rml::String back_text_{"Back"};
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
    void bindEvents();
    void removeEventListeners();

    void refreshSlotButtons();
    [[nodiscard]] std::optional<int> extractSlotIndex(Rml::Event& event) const;

    void onSlotClicked(int slot);
    void onBackClicked();
    bool onPausePressed();

    void showOverwriteConfirm(int slot);
    void hideOverwriteConfirm();
    void onOverwriteConfirmYes();
    void onOverwriteConfirmNo();
};

} // namespace game::scene
