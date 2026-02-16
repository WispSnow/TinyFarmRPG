#pragma once

#include "engine/scene/scene.h"

#include <functional>
#include <optional>
#include <string_view>
#include <vector>

namespace engine::ui {
class UIButton;
class UIGridLayout;
class UIInputBlocker;
class UILabel;
class UIPanel;
class UIStackLayout;
} // namespace engine::ui

namespace game::scene {

class SaveSlotSelectScene final : public engine::scene::Scene {
public:
    using SlotSelectCallback = std::function<void(int slot)>;

    enum class Mode {
        Load,
        Save,
    };

private:
    SlotSelectCallback on_select_{};
    Mode mode_{Mode::Load};
    std::optional<int> pending_overwrite_slot_{};

    engine::ui::UIPanel* dim_{nullptr};
    engine::ui::UIInputBlocker* input_blocker_{nullptr};
    engine::ui::UIPanel* panel_{nullptr};
    engine::ui::UIGridLayout* grid_{nullptr};
    engine::ui::UIButton* back_button_{nullptr};
    std::vector<engine::ui::UIButton*> slot_buttons_{};

    engine::ui::UIInputBlocker* confirm_blocker_{nullptr};
    engine::ui::UIPanel* confirm_panel_{nullptr};
    engine::ui::UILabel* confirm_label_{nullptr};
    engine::ui::UIStackLayout* confirm_button_row_{nullptr};
    engine::ui::UIButton* confirm_yes_button_{nullptr};
    engine::ui::UIButton* confirm_no_button_{nullptr};

public:
    SaveSlotSelectScene(std::string_view name,
                        engine::core::Context& context,
                        SlotSelectCallback on_select = {},
                        Mode mode = Mode::Load);
    ~SaveSlotSelectScene() override;

    bool init() override;

private:
    [[nodiscard]] bool initUI();
    void buildLayout();
    void refreshSlotButtons();

    void onSlotClicked(int slot);
    void onBackClicked();
    bool onPausePressed();

    void showOverwriteConfirm(int slot);
    void hideOverwriteConfirm();
    void onOverwriteConfirmYes();
    void onOverwriteConfirmNo();

};

} // namespace game::scene
