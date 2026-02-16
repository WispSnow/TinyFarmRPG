#pragma once
#include "engine/scene/scene.h"
#include "engine/render/nine_slice.h"
#include "engine/ui/ui_button.h"
#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <string_view>
#include <optional>
#include <vector>

namespace engine::debug {
class DebugPanel;
}

namespace engine::ui {
class UIPanel;
class UIButton;
class UIImage;
class UIProgressBar; // Forward declaration
class UIStackLayout;
class UIGridLayout;
class UIItemSlot;
class UIDraggablePanel;
}

namespace tools::ui {

class UITestControlPanel;

class UITestScene final : public engine::scene::Scene {
    friend class UITestControlPanel;
    engine::ui::UIPanel* demo_panel_{nullptr};
    engine::ui::UIButton* start_button_{nullptr};
    engine::ui::UIImage* preview_image_{nullptr};
    engine::ui::UIProgressBar* progress_bar_{nullptr}; 
    engine::ui::UIStackLayout* stack_layout_{nullptr};
    engine::ui::UIGridLayout* grid_layout_{nullptr};
    engine::ui::UIItemSlot* test_slot_{nullptr};
    engine::ui::UIDraggablePanel* layout_drag_panel_{nullptr};
    engine::debug::DebugPanel* debug_panel_handle_{nullptr};
    engine::render::Image panel_skin_image_{};
    std::optional<engine::render::NineSliceMargins> panel_slice_margins_{};

    // Layout acceptance helpers
    float stack_layout_spacing_{10.0f};
    int stack_layout_orientation_{1}; // 0=Horizontal, 1=Vertical
    int stack_layout_alignment_{0};   // 0=Start, 1=Center, 2=End
    bool stack_layout_auto_resize_{false};
    glm::vec2 stack_dynamic_item_size_{180.0f, 26.0f};
    float stack_dynamic_item_alpha_{0.25f};
    std::vector<engine::ui::UIPanel*> stack_dynamic_items_{};
    std::vector<engine::ui::UIPanel*> grid_dynamic_items_{};
    int grid_layout_columns_{4};
    glm::vec2 grid_layout_cell_size_{48.0f, 48.0f};
    glm::vec2 grid_layout_spacing_{5.0f, 5.0f};

    // ItemSlot + drag/drop acceptance helpers
    std::vector<engine::ui::UIItemSlot*> item_slots_{};
    engine::ui::UIItemSlot* drag_source_{nullptr};
    entt::id_type drag_item_id_{entt::null};
    int drag_item_count_{0};
    engine::render::Image drag_item_icon_{};
    bool drag_has_payload_{false};
    float drag_preview_alpha_{0.6f};
    bool drop_outside_discards_{false};
    bool drop_swaps_{false};
    int slot_edit_index_{0};
    bool drag_panel_clamp_enabled_{false};
    float drag_panel_clamp_padding_{0.0f};

    // Debug overlay toggles (UI visualization)
    bool debug_overlay_enabled_{false};
    bool debug_overlay_only_hovered_{true};
    bool debug_overlay_bounds_{true};
    bool debug_overlay_content_bounds_{false};
    bool debug_overlay_hit_boxes_{true};
    bool debug_overlay_anchor_pivot_{false};
    float debug_overlay_alpha_{0.25f};
    float debug_overlay_thickness_{2.0f};

    // UIButton acceptance helpers
    char start_button_label_buffer_[256]{};
    bool start_button_label_initialized_{false};
    int start_button_text_layout_mode_{0}; // 0=Fixed, 1=ScaleToFit
    engine::ui::Thickness start_button_text_padding_{8.0f, 4.0f, 8.0f, 4.0f};
    int hover_sound_mode_{0}; // 0=Preset, 1=Override, 2=Disabled
    int click_sound_mode_{0}; // 0=Preset, 1=Override, 2=Disabled
    int hover_sound_choice_{0};
    int click_sound_choice_{1};

public:
    UITestScene(std::string_view name, engine::core::Context& context);
    ~UITestScene() override;

    bool init() override;
    void update(float delta_time) override;
    void render() override;
    void clean() override;

private:
    void buildUI();
    void resetUI();
    void renderDebugOverlay();
    void clampLayoutPanelToRoot();
    void createDemoPanel(engine::ui::UIPanel& root_panel);
    void createButton(engine::ui::UIPanel& panel);
    void createPreviewImage(engine::ui::UIPanel& panel);
    void applyImagePresets();
    void handleInput();
    void drawControlPanel();
};

} // namespace tools::ui
