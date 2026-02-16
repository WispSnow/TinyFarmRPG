#include "ui_test_scene.h"
#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/input/input_manager.h"
#include "engine/render/renderer.h"
#include "engine/resource/resource_manager.h"
#include "engine/ui/ui_preset_manager.h"
#include "engine/ui/ui_image.h"
#include "engine/ui/ui_button.h"
#include "engine/ui/ui_manager.h"
#include "engine/ui/ui_panel.h"
#include "engine/ui/ui_element.h"
#include "engine/ui/ui_interactive.h"
#include "engine/ui/layout/ui_stack_layout.h"
#include "engine/ui/layout/ui_grid_layout.h"
#include "engine/ui/ui_progress_bar.h"
#include "engine/ui/ui_item_slot.h"
#include "engine/ui/ui_draggable_panel.h"
#include "engine/utils/math.h"
#include "engine/debug/debug_ui_manager.h"
#include <imgui.h>
#include <entt/core/hashed_string.hpp>
#include <glm/common.hpp>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>
#include <memory>
#include <algorithm>
#include <cstring>
#include <functional>

namespace tools::ui {

class UITestControlPanel final : public engine::debug::DebugPanel {
public:
    explicit UITestControlPanel(UITestScene& scene) : scene_(scene) {}

    std::string_view name() const override { return "UI Test Controls"; }

    void draw(bool& is_open) override {
        if (!ImGui::Begin("UI Test Controls", &is_open, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::End();
            return;
        }
        scene_.drawControlPanel();
        ImGui::End();
    }

private:
    UITestScene& scene_;
};

namespace {
constexpr std::string_view INVENTORY_ATLAS{"assets/farm-rpg/UI/Inventory/inventory.png"};
constexpr std::string_view FONT_PATH{"assets/fonts/VonwaonBitmap-16px.ttf"};
constexpr std::string_view SLOT_ATLAS{"assets/farm-rpg/UI/Inventory/Slots.png"};
constexpr std::string_view BAR_ATLAS{"assets/farm-rpg/UI/Bars.png"};
constexpr std::string_view CROPS_ATLAS{"assets/farm-rpg/Icons/Crops.png"};
constexpr entt::hashed_string kInventoryPanelPresetKey{"inventory_panel"};
constexpr entt::hashed_string kInventorySlotPresetKey{"inventory_slot"};
constexpr entt::hashed_string kQuickBarSlotPresetKey{"quick_bar_slot"};
constexpr entt::hashed_string kQuickBarSelectedPresetKey{"quick_bar_selected"};
constexpr float DRAG_PREVIEW_ALPHA = 0.6f;

[[nodiscard]] engine::utils::FColor withAlpha(engine::utils::FColor color, float alpha) {
    color.a = std::clamp(alpha, 0.0f, 1.0f);
    return color;
}

void drawUIOutline(engine::render::Renderer& renderer,
                   const engine::utils::Rect& rect,
                   float thickness,
                   const engine::utils::FColor& color) {
    const float t = std::max(1.0f, thickness);
    const float w = std::max(0.0f, rect.size.x);
    const float h = std::max(0.0f, rect.size.y);
    if (w <= 0.0f || h <= 0.0f) {
        return;
    }

    auto opts = renderer.getDefaultUIColorOptions();
    opts.use_gradient = false;
    opts.start_color = color;
    opts.end_color = color;

    const float top_h = std::min(t, h);
    const float left_w = std::min(t, w);

    renderer.drawUIFilledRect(engine::utils::Rect{rect.pos, glm::vec2{w, top_h}}, &opts);
    renderer.drawUIFilledRect(engine::utils::Rect{glm::vec2{rect.pos.x, rect.pos.y + h - top_h}, glm::vec2{w, top_h}}, &opts);
    renderer.drawUIFilledRect(engine::utils::Rect{rect.pos, glm::vec2{left_w, h}}, &opts);
    renderer.drawUIFilledRect(engine::utils::Rect{glm::vec2{rect.pos.x + w - left_w, rect.pos.y}, glm::vec2{left_w, h}}, &opts);
}

void drawUICross(engine::render::Renderer& renderer,
                 const glm::vec2& center,
                 float half_size,
                 float thickness,
                 const engine::utils::FColor& color) {
    const float hs = std::max(1.0f, half_size);
    const float t = std::max(1.0f, thickness);

    auto opts = renderer.getDefaultUIColorOptions();
    opts.use_gradient = false;
    opts.start_color = color;
    opts.end_color = color;

    renderer.drawUIFilledRect(engine::utils::Rect{glm::vec2{center.x - hs, center.y - t * 0.5f}, glm::vec2{hs * 2.0f, t}}, &opts);
    renderer.drawUIFilledRect(engine::utils::Rect{glm::vec2{center.x - t * 0.5f, center.y - hs}, glm::vec2{t, hs * 2.0f}}, &opts);
}
} // namespace

UITestScene::UITestScene(std::string_view name, engine::core::Context& context)
    : engine::scene::Scene(name, context) {
    panel_skin_image_ = engine::render::Image(INVENTORY_ATLAS, engine::utils::Rect{glm::vec2{0.0f, 64.0f}, glm::vec2{48.0f, 48.0f}});
    panel_slice_margins_ = engine::render::NineSliceMargins{10.0f, 10.0f, 10.0f, 10.0f};
}

UITestScene::~UITestScene() {
    if (debug_panel_handle_) {
        context_.getDebugUIManager().unregisterPanel(debug_panel_handle_);
        debug_panel_handle_ = nullptr;
    }
}

bool UITestScene::init() {

    const auto logical_size = context_.getGameState().getLogicalSize();

    ui_manager_ = std::make_unique<engine::ui::UIManager>(context_, logical_size);
    buildUI();
    if (!debug_panel_handle_) {
        auto panel = std::make_unique<UITestControlPanel>(*this);
        debug_panel_handle_ = context_.getDebugUIManager().registerPanel(std::move(panel));
    }
    if (!Scene::init()) {
        spdlog::error("UI 测试场景: 基类初始化失败。");
        return false;
    }
    spdlog::info("UI 测试场景初始化完成: {} x {}", logical_size.x, logical_size.y);
    return true;
}

void UITestScene::update(float delta_time) {
    auto& renderer = context_.getRenderer();
    auto& camera = context_.getCamera();
    renderer.beginFrame(camera);
    Scene::update(delta_time);
    handleInput();

    // 动态模拟进度条
    if (progress_bar_) {
        float v = progress_bar_->getValue();
        v += delta_time * 0.2f;
        if(v > 1.0f) v = 0.0f;
        progress_bar_->setValue(v);
    }

    clampLayoutPanelToRoot();
}

void UITestScene::render() {
    Scene::render();
    renderDebugOverlay();
}

void UITestScene::clean() {
    if (debug_panel_handle_) {
        context_.getDebugUIManager().unregisterPanel(debug_panel_handle_);
        debug_panel_handle_ = nullptr;
    }
    demo_panel_ = nullptr;
    start_button_ = nullptr;
    preview_image_ = nullptr;
    progress_bar_ = nullptr;
    stack_layout_ = nullptr;
    grid_layout_ = nullptr;
    test_slot_ = nullptr;
    layout_drag_panel_ = nullptr;
    stack_dynamic_items_.clear();
    grid_dynamic_items_.clear();
    item_slots_.clear();
    drag_source_ = nullptr;
    drag_has_payload_ = false;
    drag_item_id_ = entt::null;
    drag_item_count_ = 0;
    start_button_label_initialized_ = false;
    spdlog::info("UI 测试场景清理");
    Scene::clean();
}

void UITestScene::applyImagePresets() {
    auto& preset_manager = context_.getResourceManager().getUIPresetManager();

    if (const auto* panel_preset = preset_manager.getImagePreset(kInventoryPanelPresetKey.value())) {
        panel_skin_image_ = *panel_preset;
        panel_slice_margins_ = panel_preset->getNineSliceMargins();
    } else {
        panel_skin_image_ = engine::render::Image(INVENTORY_ATLAS, engine::utils::Rect{glm::vec2{0.0f, 64.0f}, glm::vec2{48.0f, 48.0f}});
        panel_slice_margins_ = engine::render::NineSliceMargins{10.0f, 10.0f, 10.0f, 10.0f};
    }
}

void UITestScene::buildUI() {
    if (!ui_manager_) {
        spdlog::error("UI 测试: ui_manager_ 为空，无法构建界面。");
        return;
    }

    applyImagePresets();

    auto* root = ui_manager_->getRootElement();
    if (!root) {
        spdlog::error("UI 测试: 根 UIPanel 无效，无法构建界面。");
        return;
    }

    demo_panel_ = nullptr;
    start_button_ = nullptr;
    preview_image_ = nullptr;
    progress_bar_ = nullptr;
    stack_layout_ = nullptr;
    grid_layout_ = nullptr;
    test_slot_ = nullptr;
    layout_drag_panel_ = nullptr;
    start_button_label_initialized_ = false;
    stack_dynamic_items_.clear();
    grid_dynamic_items_.clear();
    item_slots_.clear();
    drag_source_ = nullptr;
    drag_has_payload_ = false;
    drag_item_id_ = entt::null;
    drag_item_count_ = 0;

    ui_manager_->clearElements();

    const auto logical_size = context_.getGameState().getLogicalSize();
    root->setAnchor({0.0f, 0.0f}, {1.0f, 1.0f});
    root->setPivot({0.0f, 0.0f});
    root->setSize(logical_size);
    root->setPadding(engine::ui::Thickness{32.0f, 32.0f, 32.0f, 32.0f});

    createDemoPanel(*root);
    
    // 创建一个可拖拽面板
    glm::vec2 layout_pos{10.0f, 10.0f};
    glm::vec2 layout_size{300.0f, 200.0f};
    auto drag_panel = std::make_unique<engine::ui::UIDraggablePanel>(
        context_, layout_pos, layout_size, std::nullopt, panel_skin_image_, panel_slice_margins_);
    layout_drag_panel_ = drag_panel.get();

    auto* layout_panel = drag_panel->getContentPanel();
    layout_panel->setPadding({10,10,10,10});
    
    // 使用垂直 StackLayout 
    auto stack = std::make_unique<engine::ui::UIStackLayout>();
    stack->setAnchor({0,0}, {1,1}); // 充满 Panel
    stack->setOrientation(stack_layout_orientation_ == 0 ? engine::ui::Orientation::Horizontal : engine::ui::Orientation::Vertical);
    stack->setSpacing(stack_layout_spacing_);
    stack->setContentAlignment(static_cast<engine::ui::Alignment>(stack_layout_alignment_));
    stack->setAutoResize(stack_layout_auto_resize_);
    stack_layout_ = stack.get();
    
    // 1. 进度条演示
    // Drag handle area (non-interactive): click/drag here to move the draggable panel.
    {
        auto header = std::make_unique<engine::ui::UIPanel>(glm::vec2{0.0f, 0.0f},
                                                           glm::vec2{0.0f, 22.0f},
                                                           engine::utils::FColor{0.15f, 0.15f, 0.18f, 0.35f});
        header->setAnchor({0.0f, 0.0f}, {1.0f, 0.0f});
        header->setPivot({0.0f, 0.0f});
        stack->addChild(std::move(header));
    }

    {
        auto bar = std::make_unique<engine::ui::UIProgressBar>(context_, glm::vec2{10, 10}, glm::vec2{41, 7});
        bar->showLabel(true);
        bar->setValue(0.5f);
        auto background = engine::render::Image(BAR_ATLAS, engine::utils::Rect{glm::vec2{99,101}, glm::vec2{41, 7}});
        auto fill = engine::render::Image(BAR_ATLAS, engine::utils::Rect{glm::vec2{54,88}, glm::vec2{26, 3}});
        bar->setBackground(background); 
        bar->setFill(fill); // 实际会被 Stretch/Clip
        // 进度需要动态更新，我们暂存指针
        progress_bar_ = bar.get();
        stack->addChild(std::move(bar));
    }

    // 2. 网格布局演示 (物品栏)
    {
        auto grid = std::make_unique<engine::ui::UIGridLayout>(glm::vec2{0, 0}, glm::vec2{280, 200});
        grid->setColumnCount(std::max(1, grid_layout_columns_));
        grid->setSpacing(grid_layout_spacing_);
        grid->setCellSize(grid_layout_cell_size_); // 假设 Slot 大小
        grid_layout_ = grid.get();

        auto& preset_manager = context_.getResourceManager().getUIPresetManager();
        const auto* slot_bg_preset = preset_manager.getImagePreset(kQuickBarSlotPresetKey.value());
        const auto* slot_selected_preset = preset_manager.getImagePreset(kQuickBarSelectedPresetKey.value());
        auto fallback_slot_bg = engine::render::Image(SLOT_ATLAS, engine::utils::Rect{glm::vec2{151,6}, glm::vec2{18, 18}});
        auto fallback_slot_selected = engine::render::Image(SLOT_ATLAS, engine::utils::Rect{glm::vec2{119,6}, glm::vec2{18, 18}});
        
        // 添加一些 Slot
        for(int i=0; i<8; ++i) {
             auto slot = std::make_unique<engine::ui::UIItemSlot>(context_);
             const engine::render::Image& selection = slot_selected_preset ? *slot_selected_preset : fallback_slot_selected;
             const engine::render::Image& bg = slot_bg_preset ? *slot_bg_preset : fallback_slot_bg;
             slot->setSelectionImage(selection);
             slot->setBackgroundImage(bg);
             // 初始化一些测试物品
             if (i == 0) {
                 slot->setSlotItem(entt::hashed_string{"apple"}.value(), 7,
                                   engine::render::Image(CROPS_ATLAS, engine::utils::Rect{glm::vec2{0,0}, glm::vec2{16,16}}));
	                 slot->setSelected(true);
	                 test_slot_ = slot.get(); // 记录第一个 Slot 用于测试
	             } else if (i == 1) {
	                 slot->setSlotItem(entt::hashed_string{"corn"}.value(), 5,
	                                   engine::render::Image(CROPS_ATLAS, engine::utils::Rect{glm::vec2{16,0}, glm::vec2{16,16}}));
	             }
	             if (i == 2) slot->setCooldown(0.4f);

	             item_slots_.push_back(slot.get());
	             grid->addChild(std::move(slot));
	        }
	        for (auto* slot : item_slots_) {
	            auto behavior = std::make_unique<engine::ui::DragBehavior>();
	            behavior->setOnBegin([slot, this](engine::ui::UIInteractive&, const glm::vec2& pos) {
	                auto item = slot->getSlotItem();
	                if (item && item->count > 0) {
	                    glm::vec2 preview_size = slot->getIconLayoutSize();
	                    if (preview_size.x <= 0.0f || preview_size.y <= 0.0f) {
	                        preview_size = slot->getLayoutSize();
                    }
                    if (preview_size.x <= 0.0f || preview_size.y <= 0.0f) {
	                        preview_size = slot->getSize();
	                    }

	                    drag_source_ = slot;
	                    drag_has_payload_ = true;
	                    drag_item_id_ = item->item_id;
	                    drag_item_count_ = item->count;
	                    drag_item_icon_ = item->icon;
	                    slot->clearItem(); // 拖起时暂时清空源槽显示
	                    if (ui_manager_) {
	                        ui_manager_->beginDragPreview(drag_item_icon_, drag_item_count_, preview_size, drag_preview_alpha_, FONT_PATH);
	                        ui_manager_->updateDragPreview(pos);
	                    }
	                } else {
	                    drag_source_ = nullptr;
	                    drag_has_payload_ = false;
	                    if (ui_manager_) {
	                        ui_manager_->endDragPreview();
	                    }
	                }
	            });
	            behavior->setOnUpdate([this](engine::ui::UIInteractive&, const glm::vec2& pos, const glm::vec2&) {
	                if (drag_has_payload_ && ui_manager_) {
	                    ui_manager_->updateDragPreview(pos);
	                }
	            });
	            behavior->setOnEnd([this](engine::ui::UIInteractive&, const glm::vec2& pos, bool) {
	                if (ui_manager_) {
	                    ui_manager_->endDragPreview();
	                }
	                if (!drag_has_payload_ || !drag_source_) {
	                    drag_source_ = nullptr;
	                    drag_has_payload_ = false;
	                    return;
	                }
	                engine::ui::UIItemSlot* target = nullptr;
	                for (auto* s : item_slots_) {
	                    if (s->isPointInside(pos)) {
	                        target = s;
	                        break;
	                    }
	                }
	                if (target) {
	                    const auto target_item = target->getSlotItem();
	                    target->setSlotItem(drag_item_id_, drag_item_count_, drag_item_icon_);
	                    if (drop_swaps_ && target != drag_source_ && target_item && target_item->count > 0) {
	                        drag_source_->setSlotItem(target_item->item_id, target_item->count, target_item->icon);
	                    }
	                } else {
	                    if (!drop_outside_discards_) {
	                        drag_source_->setSlotItem(drag_item_id_, drag_item_count_, drag_item_icon_);
	                    }
	                }
	                drag_source_ = nullptr;
	                drag_has_payload_ = false;
	            });
	            slot->addBehavior(std::move(behavior));
	        }
	        stack->addChild(std::move(grid));
    }
    
    layout_panel->addChild(std::move(stack));
    root->addChild(std::move(drag_panel));
}

void UITestScene::resetUI() {
    // Reset acceptance settings to baseline so that "one-click reset" returns to a known state.
    start_button_label_initialized_ = false;
    start_button_text_layout_mode_ = 0;
    start_button_text_padding_ = engine::ui::Thickness{8.0f, 4.0f, 8.0f, 4.0f};
    hover_sound_mode_ = 0;
    click_sound_mode_ = 0;
    hover_sound_choice_ = 0;
    click_sound_choice_ = 1;
    stack_layout_spacing_ = 10.0f;
    stack_layout_orientation_ = 1;
    stack_layout_alignment_ = 0;
    stack_layout_auto_resize_ = false;
    stack_dynamic_item_size_ = glm::vec2{180.0f, 26.0f};
    stack_dynamic_item_alpha_ = 0.25f;
    grid_layout_columns_ = 4;
    grid_layout_cell_size_ = glm::vec2{48.0f, 48.0f};
    grid_layout_spacing_ = glm::vec2{5.0f, 5.0f};
    drag_preview_alpha_ = DRAG_PREVIEW_ALPHA;
    drop_outside_discards_ = false;
    drop_swaps_ = false;
    slot_edit_index_ = 0;
    drag_panel_clamp_enabled_ = false;
    drag_panel_clamp_padding_ = 0.0f;
    buildUI();
}

void UITestScene::clampLayoutPanelToRoot() {
    if (!drag_panel_clamp_enabled_ || !layout_drag_panel_) {
        return;
    }

    auto* parent = layout_drag_panel_->getParent();
    if (!parent) {
        return;
    }

    const engine::utils::Rect parent_bounds = parent->getContentBounds();
    const engine::utils::Rect panel_bounds = layout_drag_panel_->getBounds();

    const float pad = std::max(0.0f, drag_panel_clamp_padding_);
    const float min_x = parent_bounds.pos.x + pad;
    const float min_y = parent_bounds.pos.y + pad;
    const float max_x = std::max(min_x, parent_bounds.pos.x + parent_bounds.size.x - pad - panel_bounds.size.x);
    const float max_y = std::max(min_y, parent_bounds.pos.y + parent_bounds.size.y - pad - panel_bounds.size.y);

    const float clamped_x = std::clamp(panel_bounds.pos.x, min_x, max_x);
    const float clamped_y = std::clamp(panel_bounds.pos.y, min_y, max_y);
    const glm::vec2 clamped_screen_pos{clamped_x, clamped_y};

    if (glm::distance(clamped_screen_pos, panel_bounds.pos) <= 0.001f) {
        return;
    }

    const glm::vec2 delta = clamped_screen_pos - panel_bounds.pos;
    layout_drag_panel_->setPosition(layout_drag_panel_->getPosition() + delta);
}

void UITestScene::renderDebugOverlay() {
    if (!debug_overlay_enabled_ || !ui_manager_) {
        return;
    }

    auto* root = ui_manager_->getRootElement();
    if (!root || !root->isVisible()) {
        return;
    }

    auto& renderer = context_.getRenderer();

    const glm::vec2 mouse_pos = context_.getInputManager().getLogicalMousePosition();
    engine::ui::UIInteractive* hovered = ui_manager_->findInteractiveAt(mouse_pos);

    auto drawElement = [&](engine::ui::UIElement& element, bool is_focus) {
        const engine::utils::Rect bounds = element.getBounds();
        const engine::utils::Rect content = element.getContentBounds();

        const float base_alpha = is_focus ? std::min(1.0f, debug_overlay_alpha_ * 1.8f) : debug_overlay_alpha_;

        if (debug_overlay_bounds_) {
            drawUIOutline(renderer, bounds, debug_overlay_thickness_, withAlpha(engine::utils::FColor{1.0f, 1.0f, 1.0f, 1.0f}, base_alpha));
        }

        if (debug_overlay_content_bounds_) {
            drawUIOutline(renderer, content, debug_overlay_thickness_, withAlpha(engine::utils::FColor{0.25f, 0.9f, 0.35f, 1.0f}, base_alpha));
        }

        if (debug_overlay_hit_boxes_) {
            if (auto* interactive = dynamic_cast<engine::ui::UIInteractive*>(&element)) {
                engine::utils::FColor color{0.2f, 0.85f, 0.95f, 1.0f}; // default
                if (!interactive->isInteractive()) {
                    color = engine::utils::FColor{0.6f, 0.6f, 0.6f, 1.0f};
                } else if (interactive->isDragging()) {
                    color = engine::utils::FColor{0.95f, 0.2f, 0.95f, 1.0f};
                } else if (interactive->isPressed()) {
                    color = engine::utils::FColor{0.95f, 0.25f, 0.25f, 1.0f};
                } else if (interactive->isHovered()) {
                    color = engine::utils::FColor{0.98f, 0.9f, 0.25f, 1.0f};
                }
                drawUIOutline(renderer, bounds, std::max(1.0f, debug_overlay_thickness_), withAlpha(color, std::min(1.0f, base_alpha * 1.2f)));
            }
        }

        if (debug_overlay_anchor_pivot_ && is_focus) {
            const glm::vec2 pivot = element.getPivot();
            const glm::vec2 pivot_pos = bounds.pos + bounds.size * pivot;
            drawUICross(renderer, pivot_pos, 6.0f, std::max(1.0f, debug_overlay_thickness_), withAlpha(engine::utils::FColor::yellow(), 0.9f));

            if (auto* parent = element.getParent()) {
                const auto parent_content = parent->getContentBounds();
                const glm::vec2 anchor_min = element.getAnchorMin();
                const glm::vec2 anchor_max = element.getAnchorMax();
                const glm::vec2 a0 = parent_content.pos + parent_content.size * anchor_min;
                const glm::vec2 a1 = parent_content.pos + parent_content.size * anchor_max;

                const glm::vec2 minp{std::min(a0.x, a1.x), std::min(a0.y, a1.y)};
                const glm::vec2 maxp{std::max(a0.x, a1.x), std::max(a0.y, a1.y)};
                const engine::utils::Rect anchor_rect{minp, glm::max(maxp - minp, glm::vec2{1.0f, 1.0f})};
                drawUIOutline(renderer, anchor_rect, std::max(1.0f, debug_overlay_thickness_), withAlpha(engine::utils::FColor{0.2f, 0.6f, 1.0f, 1.0f}, 0.7f));
                drawUICross(renderer, a0, 5.0f, std::max(1.0f, debug_overlay_thickness_), withAlpha(engine::utils::FColor{0.2f, 0.6f, 1.0f, 1.0f}, 0.9f));
                drawUICross(renderer, a1, 5.0f, std::max(1.0f, debug_overlay_thickness_), withAlpha(engine::utils::FColor{0.2f, 0.6f, 1.0f, 1.0f}, 0.9f));
            }
        }
    };

    if (debug_overlay_only_hovered_) {
        if (hovered) {
            engine::ui::UIElement* current = hovered;
            bool first = true;
            while (current) {
                drawElement(*current, first);
                first = false;
                current = current->getParent();
            }
        }
        return;
    }

    std::function<void(engine::ui::UIElement&)> walk = [&](engine::ui::UIElement& element) {
        if (!element.isVisible()) {
            return;
        }
        drawElement(element, &element == hovered);
        for (const auto& child : element.getChildren()) {
            if (child) {
                walk(*child);
            }
        }
    };
    walk(*root);
}

void UITestScene::createDemoPanel(engine::ui::UIPanel& root_panel) {
    auto panel = std::make_unique<engine::ui::UIPanel>(glm::vec2{-120.0f, -120.0f}, glm::vec2{420.0f, 320.0f});
    panel->setAnchor({0.5f, 0.5f}, {0.5f, 0.5f});
    // panel->setAnchor({0.1f, 0.1f}, {0.1f, 0.1f});
    panel->setPivot({0.0f, 0.0f});
    panel->setPadding(engine::ui::Thickness{24.0f, 24.0f, 24.0f, 24.0f});
    panel->setBackgroundColor(std::nullopt);

    auto* panel_ptr = panel.get();
    demo_panel_ = panel_ptr;
    demo_panel_->setSkinImage(panel_skin_image_);
    demo_panel_->setNineSliceMargins(panel_slice_margins_);
    root_panel.addChild(std::move(panel));

    createPreviewImage(*panel_ptr);
    createButton(*panel_ptr);
}

namespace {
constexpr entt::hashed_string kStartButtonPresetKey{"primary"};
}

void UITestScene::createButton(engine::ui::UIPanel& panel) {
    auto button = engine::ui::UIButton::create(
        context_,
        kStartButtonPresetKey.value(),
        glm::vec2{0.0f, 0.0f},
        glm::vec2{120.0f, 40.0f},
        []() {
            spdlog::info("[UI Tester] 点击 Start 按钮");
        },
        []() {
            spdlog::debug("[UI Tester] 鼠标进入按钮");
        },
        []() {
            spdlog::debug("[UI Tester] 鼠标离开按钮");
        });

    if (!button) {
        spdlog::error("[UI Tester] 创建 Start 按钮失败。");
        start_button_ = nullptr;
        return;
    }

    button->setAnchor({0.5f, 0.5f}, {0.5f, 0.5f});
    button->setPivot({0.5f, 0.5f});
    button->setMargin(engine::ui::Thickness{3.0f, 3.0f, 3.0f, 3.0f});

    if (start_button_text_layout_mode_ == 0) {
        button->setTextLayoutFixed();
    } else {
        button->setTextLayoutScaleToFit(start_button_text_padding_);
    }

    if (!start_button_label_initialized_) {
        std::memset(start_button_label_buffer_, 0, sizeof(start_button_label_buffer_));
        const std::string_view text = button->getLabelText();
        const std::size_t copy_count = std::min<std::size_t>(text.size(), sizeof(start_button_label_buffer_) - 1);
        std::memcpy(start_button_label_buffer_, text.data(), copy_count);
        start_button_label_buffer_[copy_count] = '\0';
        start_button_label_initialized_ = true;
    }

    start_button_ = button.get();
    panel.addChild(std::move(button));
}

void UITestScene::createPreviewImage(engine::ui::UIPanel& panel) {
    auto& preset_manager = context_.getResourceManager().getUIPresetManager();
    engine::render::Image preview_image_def = engine::render::Image(
        INVENTORY_ATLAS,
        engine::utils::Rect{glm::vec2{0.0f, 64.0f}, glm::vec2{48.0f, 48.0f}}
    );

    if (const auto* preset = preset_manager.getImagePreset(kInventorySlotPresetKey.value())) {
        preview_image_def = *preset;
    }

    auto preview = std::make_unique<engine::ui::UIImage>(std::move(preview_image_def), glm::vec2{0.0f, 64.0f}, glm::vec2{64.0f, 64.0f});
    preview->setAnchor({0.5f, 0.5f}, {0.5f, 0.5f});
    preview->setPivot({0.0f, 0.0f});
    preview_image_ = preview.get();
    panel.addChild(std::move(preview));
}

void UITestScene::handleInput() {
    auto& input = context_.getInputManager();
    using namespace entt::literals;
    if (input.isActionPressed("pause"_hs) || input.isActionPressed("mouse_right"_hs)) {
        spdlog::info("[UI Tester] 触发退出");
        requestPopScene();
    }
}

void UITestScene::drawControlPanel() {
    if (ImGui::Button("Reset UI (Rebuild)")) {
        resetUI();
    }

    if (ImGui::CollapsingHeader("Debug Overlay", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Enable Debug Overlay", &debug_overlay_enabled_);
        ImGui::Checkbox("Only Hovered Element (and parents)", &debug_overlay_only_hovered_);
        ImGui::Checkbox("Bounds", &debug_overlay_bounds_);
        ImGui::Checkbox("Content Bounds", &debug_overlay_content_bounds_);
        ImGui::Checkbox("Hit Boxes (Hover/Pressed/Dragging)", &debug_overlay_hit_boxes_);
        ImGui::Checkbox("Anchor/Pivot (focus element)", &debug_overlay_anchor_pivot_);
        ImGui::SliderFloat("Overlay Alpha", &debug_overlay_alpha_, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Line Thickness", &debug_overlay_thickness_, 1.0f, 6.0f, "%.1f");

        if (ui_manager_) {
            const glm::vec2 mouse_pos = context_.getInputManager().getLogicalMousePosition();
            const auto* hovered = ui_manager_->findInteractiveAt(mouse_pos);
            if (hovered) {
                ImGui::Text("Hovered: interactive=%d hovered=%d pressed=%d dragging=%d",
                            hovered->isInteractive(),
                            hovered->isHovered(),
                            hovered->isPressed(),
                            hovered->isDragging());
                if (dynamic_cast<const engine::ui::UIButton*>(hovered)) {
                    ImGui::TextUnformatted("Hovered type: UIButton");
                } else if (dynamic_cast<const engine::ui::UIItemSlot*>(hovered)) {
                    ImGui::TextUnformatted("Hovered type: UIItemSlot");
                } else if (dynamic_cast<const engine::ui::UIDraggablePanel*>(hovered)) {
                    ImGui::TextUnformatted("Hovered type: UIDraggablePanel");
                }
            } else {
                ImGui::TextUnformatted("Hovered: <none>");
            }
        }
    }

    if (!demo_panel_) {
        ImGui::TextUnformatted("UI 尚未初始化。");
        return;
    }

    if (ImGui::CollapsingHeader("Case Info", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto drawBlock = [](const char* title, const char* content) {
            if (!content || content[0] == '\0') {
                return;
            }
            ImGui::TextUnformatted(title);
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextUnformatted(content);
            ImGui::PopTextWrapPos();
        };

        drawBlock("目的:", "用于开发期手动验收自研 UI 组件/布局/交互的关键链路。");
        drawBlock("操作:", "1) 左键点击按钮，观察 hover/pressed/click。\n"
                         "2) 拖拽物品槽位，观察拖拽预览与 drop 行为。\n"
                         "3) 拖拽可拖拽面板（点击空白区域更容易触发拖拽）。\n"
                         "4) 在本面板中调整 Panel/Button/Image 参数，观察布局刷新。\n"
                         "5) 按 P/Esc 或右键退出该场景。");
        drawBlock("预期:", "- 命中准确、状态切换正确。\n"
                         "- 布局调整即时生效，无崩溃/卡死。\n"
                         "- 拖拽预览位置/透明度合理，drop 行为符合当前设计。");
        drawBlock("常见问题:", "- 鼠标命中偏移：逻辑坐标换算或 anchor/pivot 使用错误。\n"
                             "- 点击/释放语义异常：pressed 捕获与 release inside/outside 判断问题。\n"
                             "- 拖拽预览闪烁：布局刷新或渲染顺序问题。\n"
                             "- 九宫格显示异常：margins/sourceRect 配置或 nine-slice dirty 刷新问题。");
    }

    if (ImGui::CollapsingHeader("Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BulletText("Left Click: interact / drag");
        ImGui::BulletText("Right Click or P/Esc: exit scene");
        ImGui::BulletText("Tip: drag the top handle bar to move the panel");
    }

    if (ImGui::CollapsingHeader("Panel", ImGuiTreeNodeFlags_DefaultOpen)) {
        glm::vec2 panel_pos = demo_panel_->getPosition();
        float panel_pos_arr[2]{panel_pos.x, panel_pos.y};
        if (ImGui::DragFloat2("Panel Offset", panel_pos_arr, 1.0f, -1000.0f, 1000.0f, "%.1f")) {
            demo_panel_->setPosition(glm::vec2{panel_pos_arr[0], panel_pos_arr[1]});
        }

        glm::vec2 panel_size = demo_panel_->getRequestedSize();
        float panel_size_arr[2]{panel_size.x, panel_size.y};
        if (ImGui::DragFloat2("Panel Size", panel_size_arr, 1.0f, 32.0f, 2048.0f, "%.1f")) {
            panel_size_arr[0] = std::max(panel_size_arr[0], 1.0f);
            panel_size_arr[1] = std::max(panel_size_arr[1], 1.0f);
            demo_panel_->setSize(glm::vec2{panel_size_arr[0], panel_size_arr[1]});
        }

        const auto& padding = demo_panel_->getPadding();
        float padding_arr[4]{padding.left, padding.top, padding.right, padding.bottom};
        if (ImGui::DragFloat4("Panel Padding (L/T/R/B)", padding_arr, 0.5f, 0.0f, 512.0f, "%.1f")) {
            demo_panel_->setPadding(engine::ui::Thickness{padding_arr[0], padding_arr[1], padding_arr[2], padding_arr[3]});
        }

        bool visible = demo_panel_->isVisible();
        if (ImGui::Checkbox("Panel Visible", &visible)) {
            demo_panel_->setVisible(visible);
        }
    }

    if (demo_panel_ && ImGui::CollapsingHeader("Nine-Slice Skin", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (!panel_skin_image_.getTexturePath().empty()) {
            ImGui::Text("Texture: %s", panel_skin_image_.getTexturePath().data());
        }

        const engine::render::NineSliceMargins current_margins = panel_slice_margins_.value_or(engine::render::NineSliceMargins{});
        float margins_arr[4]{current_margins.left, current_margins.top,
                             current_margins.right, current_margins.bottom};
        if (ImGui::DragFloat4("Margins (L/T/R/B)", margins_arr, 0.5f, 0.0f, 256.0f, "%.1f")) {
            if (!panel_slice_margins_) {
                panel_slice_margins_ = engine::render::NineSliceMargins{};
            }
            panel_slice_margins_->left = std::max(0.0f, margins_arr[0]);
            panel_slice_margins_->top = std::max(0.0f, margins_arr[1]);
            panel_slice_margins_->right = std::max(0.0f, margins_arr[2]);
            panel_slice_margins_->bottom = std::max(0.0f, margins_arr[3]);
            demo_panel_->setSkinImage(panel_skin_image_);
            demo_panel_->setNineSliceMargins(panel_slice_margins_);
        }

        const auto& source_rect = panel_skin_image_.getSourceRect();
        float src_rect_arr[4]{source_rect.pos.x,
                               source_rect.pos.y,
                               source_rect.size.x,
                               source_rect.size.y};
        if (ImGui::DragFloat4("Source Rect (X/Y/W/H)", src_rect_arr, 1.0f, 0.0f, 4096.0f, "%.1f")) {
            engine::utils::Rect rect{glm::vec2{src_rect_arr[0], src_rect_arr[1]},
                                     glm::vec2{std::max(0.0f, src_rect_arr[2]), std::max(0.0f, src_rect_arr[3])}};
            panel_skin_image_.setSourceRect(rect);
            demo_panel_->setSkinImage(panel_skin_image_);
            demo_panel_->setNineSliceMargins(panel_slice_margins_);
        }

        if (demo_panel_->hasNineSliceSkin()) {
            if (const auto* margins = demo_panel_->getNineSliceMargins()) {
                const glm::vec2 min_size{
                    margins->left + margins->right,
                    margins->top + margins->bottom
                };
                ImGui::Text("Minimum Size: %.1f x %.1f", min_size.x, min_size.y);
            } else {
                ImGui::TextUnformatted("(皮肤尚未配置)");
            }
        }

        if (ImGui::Button("Reload Nine Slice")) {
            demo_panel_->markNineSliceDirty();
        }

        bool use_nine_slice = demo_panel_->hasNineSliceSkin();
        ImGui::SameLine();
        if (ImGui::Checkbox("Enable", &use_nine_slice)) {
            if (use_nine_slice) {
                demo_panel_->setSkinImage(panel_skin_image_);
                demo_panel_->setNineSliceMargins(panel_slice_margins_);
            } else {
                demo_panel_->clearSkinImage();
                demo_panel_->setBackgroundColor(engine::utils::FColor{0.08f, 0.08f, 0.12f, 0.9f});
            }
        }
    }

	    if (start_button_ && ImGui::CollapsingHeader("Button", ImGuiTreeNodeFlags_DefaultOpen)) {
        glm::vec2 btn_pos = start_button_->getPosition();
        float btn_pos_arr[2]{btn_pos.x, btn_pos.y};
        if (ImGui::DragFloat2("Button Offset", btn_pos_arr, 1.0f, -500.0f, 500.0f, "%.1f")) {
            start_button_->setPosition(glm::vec2{btn_pos_arr[0], btn_pos_arr[1]});
        }

        glm::vec2 btn_size = start_button_->getRequestedSize();
        float btn_size_arr[2]{btn_size.x, btn_size.y};
        if (ImGui::DragFloat2("Button Size", btn_size_arr, 1.0f, 16.0f, 1024.0f, "%.1f")) {
            btn_size_arr[0] = std::max(btn_size_arr[0], 1.0f);
            btn_size_arr[1] = std::max(btn_size_arr[1], 1.0f);
            start_button_->setSize(glm::vec2{btn_size_arr[0], btn_size_arr[1]});
        }

        const auto& margin = start_button_->getMargin();
        float margin_arr[4]{margin.left, margin.top, margin.right, margin.bottom};
        if (ImGui::DragFloat4("Button Margin (L/T/R/B)", margin_arr, 0.5f, 0.0f, 256.0f, "%.1f")) {
            start_button_->setMargin(engine::ui::Thickness{margin_arr[0], margin_arr[1], margin_arr[2], margin_arr[3]});
        }

	        bool visible = start_button_->isVisible();
	        if (ImGui::Checkbox("Button Visible", &visible)) {
	            start_button_->setVisible(visible);
	        }

	        // --- UIButton acceptance ---
	        ImGui::Separator();
	        ImGui::TextUnformatted("UIButton Acceptance");

	        bool interactive = start_button_->isInteractive();
	        if (ImGui::Checkbox("Interactive (Disabled when off)", &interactive)) {
	            start_button_->setInteractive(interactive);
	            start_button_->applyStateVisual(interactive ? engine::ui::UI_IMAGE_NORMAL_ID : engine::ui::UI_IMAGE_DISABLED_ID);
	        }

	        ImGui::Text("State: hovered=%d pressed=%d dragging=%d",
	                    start_button_->isHovered(),
	                    start_button_->isPressed(),
	                    start_button_->isDragging());

	        if (ImGui::InputText("Label", start_button_label_buffer_, sizeof(start_button_label_buffer_))) {
	            start_button_->setLabelText(std::string{start_button_label_buffer_});
	        }
	        if (ImGui::Button("Set Short Label")) {
	            constexpr const char* kShort = "按钮";
	            std::strncpy(start_button_label_buffer_, kShort, sizeof(start_button_label_buffer_) - 1);
	            start_button_label_buffer_[sizeof(start_button_label_buffer_) - 1] = '\0';
	            start_button_->setLabelText(std::string{kShort});
	        }
	        ImGui::SameLine();
	        if (ImGui::Button("Set Long Label")) {
	            constexpr const char* kLong = "A very long label: The quick brown fox jumps over the lazy dog 0123456789";
	            std::strncpy(start_button_label_buffer_, kLong, sizeof(start_button_label_buffer_) - 1);
	            start_button_label_buffer_[sizeof(start_button_label_buffer_) - 1] = '\0';
	            start_button_->setLabelText(std::string{kLong});
	        }

	        int layout_mode = start_button_text_layout_mode_;
	        if (ImGui::RadioButton("Text: Fixed", &layout_mode, 0)) {
	            start_button_text_layout_mode_ = 0;
	            start_button_->setTextLayoutFixed();
	        }
	        ImGui::SameLine();
	        if (ImGui::RadioButton("Text: ScaleToFit", &layout_mode, 1)) {
	            start_button_text_layout_mode_ = 1;
	            start_button_->setTextLayoutScaleToFit(start_button_text_padding_);
	        }

	        if (start_button_text_layout_mode_ == 1) {
	            float pad_arr[4]{start_button_text_padding_.left, start_button_text_padding_.top,
	                             start_button_text_padding_.right, start_button_text_padding_.bottom};
	            if (ImGui::DragFloat4("ScaleToFit Padding (L/T/R/B)", pad_arr, 0.5f, 0.0f, 128.0f, "%.1f")) {
	                start_button_text_padding_ = engine::ui::Thickness{pad_arr[0], pad_arr[1], pad_arr[2], pad_arr[3]};
	                start_button_->setTextLayoutScaleToFit(start_button_text_padding_);
	            }
	        }

	        static const char* kSoundModeLabels[] = {"Preset", "Override", "Disabled"};
	        static const char* kSoundChoiceLabels[] = {
	            "assets/audio/UI_button11.wav",
	            "assets/audio/UI_button08.wav",
	            "assets/audio/Fantasy_UI (1).wav",
	            "assets/audio/Fantasy_UI (10).wav",
	            "assets/audio/pop.mp3",
	        };

	        auto applySoundMode = [&](entt::id_type event_id, int mode, int choice) {
	            if (mode == 2) {
	                start_button_->disableSoundEvent(event_id);
	                return;
	            }

	            if (mode == 0) {
	                const auto* preset = context_.getResourceManager().getUIPresetManager().getButtonPreset(kStartButtonPresetKey.value());
	                if (!preset) {
	                    start_button_->clearSoundEventOverride(event_id);
	                    return;
	                }
	                if (auto it = preset->sound_events.find(event_id); it != preset->sound_events.end()) {
	                    start_button_->setSoundEvent(event_id, it->second);
	                } else {
	                    start_button_->clearSoundEventOverride(event_id);
	                }
	                return;
	            }

	            const int clamped = std::clamp(choice, 0, static_cast<int>(IM_ARRAYSIZE(kSoundChoiceLabels)) - 1);
	            start_button_->setSoundEvent(event_id, kSoundChoiceLabels[clamped]);
	        };

	        if (ImGui::Combo("Hover Sound", &hover_sound_mode_, kSoundModeLabels, IM_ARRAYSIZE(kSoundModeLabels))) {
	            applySoundMode(engine::ui::UI_SOUND_EVENT_HOVER_ID, hover_sound_mode_, hover_sound_choice_);
	        }
	        if (hover_sound_mode_ == 1) {
	            if (ImGui::Combo("Hover Override", &hover_sound_choice_, kSoundChoiceLabels, IM_ARRAYSIZE(kSoundChoiceLabels))) {
	                applySoundMode(engine::ui::UI_SOUND_EVENT_HOVER_ID, hover_sound_mode_, hover_sound_choice_);
	            }
	        }
	        if (ImGui::Button("Play Hover Sound Now")) {
	            start_button_->playSoundEvent(engine::ui::UI_SOUND_EVENT_HOVER_ID);
	        }

	        if (ImGui::Combo("Click Sound", &click_sound_mode_, kSoundModeLabels, IM_ARRAYSIZE(kSoundModeLabels))) {
	            applySoundMode(engine::ui::UI_SOUND_EVENT_CLICK_ID, click_sound_mode_, click_sound_choice_);
	        }
	        if (click_sound_mode_ == 1) {
	            if (ImGui::Combo("Click Override", &click_sound_choice_, kSoundChoiceLabels, IM_ARRAYSIZE(kSoundChoiceLabels))) {
	                applySoundMode(engine::ui::UI_SOUND_EVENT_CLICK_ID, click_sound_mode_, click_sound_choice_);
	            }
	        }
	        if (ImGui::Button("Play Click Sound Now")) {
	            start_button_->playSoundEvent(engine::ui::UI_SOUND_EVENT_CLICK_ID);
	        }
	    }

    if (preview_image_ && ImGui::CollapsingHeader("Preview Image", ImGuiTreeNodeFlags_DefaultOpen)) {
        glm::vec2 img_pos = preview_image_->getPosition();
        float img_pos_arr[2]{img_pos.x, img_pos.y};
        if (ImGui::DragFloat2("Image Offset", img_pos_arr, 1.0f, -500.0f, 500.0f, "%.1f")) {
            preview_image_->setPosition(glm::vec2{img_pos_arr[0], img_pos_arr[1]});
        }

        glm::vec2 img_size = preview_image_->getRequestedSize();
        float img_size_arr[2]{img_size.x, img_size.y};
        if (ImGui::DragFloat2("Image Size", img_size_arr, 1.0f, 16.0f, 2048.0f, "%.1f")) {
            img_size_arr[0] = std::max(img_size_arr[0], 1.0f);
            img_size_arr[1] = std::max(img_size_arr[1], 1.0f);
            preview_image_->setSize(glm::vec2{img_size_arr[0], img_size_arr[1]});
        }

        bool visible = preview_image_->isVisible();
        if (ImGui::Checkbox("Image Visible", &visible)) {
            preview_image_->setVisible(visible);
        }
    }

    if (progress_bar_ && ImGui::CollapsingHeader("Progress Bar Demo", ImGuiTreeNodeFlags_DefaultOpen)) {
        float val = progress_bar_->getValue();
        if (ImGui::SliderFloat("Value", &val, 0.0f, 1.0f)) {
            progress_bar_->setValue(val);
        }
        
        // Fill Type (Simple combo)
        // ... (Skipping complex enum combo for brevity, just reset test)
    }

    if (layout_drag_panel_ && ImGui::CollapsingHeader("Layout Panel (Draggable)", ImGuiTreeNodeFlags_DefaultOpen)) {
        glm::vec2 pos = layout_drag_panel_->getPosition();
        float pos_arr[2] = {pos.x, pos.y};
        if (ImGui::DragFloat2("Position", pos_arr, 1.0f, -2048.0f, 2048.0f, "%.1f")) {
            layout_drag_panel_->setPosition(glm::vec2{pos_arr[0], pos_arr[1]});
        }

        glm::vec2 size = layout_drag_panel_->getRequestedSize();
        float size_arr[2] = {size.x, size.y};
        if (ImGui::DragFloat2("Size", size_arr, 1.0f, 80.0f, 2048.0f, "%.1f")) {
            size_arr[0] = std::max(80.0f, size_arr[0]);
            size_arr[1] = std::max(80.0f, size_arr[1]);
            layout_drag_panel_->setSize(glm::vec2{size_arr[0], size_arr[1]});
        }

        ImGui::Checkbox("Clamp To Root", &drag_panel_clamp_enabled_);
        if (drag_panel_clamp_enabled_) {
            ImGui::SliderFloat("Clamp Padding", &drag_panel_clamp_padding_, 0.0f, 64.0f, "%.1f");
        }

        if (ImGui::Button("Preset: Tiny")) {
            layout_drag_panel_->setPosition(glm::vec2{10.0f, 10.0f});
            layout_drag_panel_->setSize(glm::vec2{220.0f, 160.0f});
        }
        ImGui::SameLine();
        if (ImGui::Button("Preset: Default")) {
            layout_drag_panel_->setPosition(glm::vec2{10.0f, 10.0f});
            layout_drag_panel_->setSize(glm::vec2{300.0f, 200.0f});
        }
        ImGui::SameLine();
        if (ImGui::Button("Preset: Large")) {
            layout_drag_panel_->setPosition(glm::vec2{10.0f, 10.0f});
            layout_drag_panel_->setSize(glm::vec2{720.0f, 520.0f});
        }
    }

    if (stack_layout_ && ImGui::CollapsingHeader("Stack Layout", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::DragFloat("Spacing", &stack_layout_spacing_, 1.0f, 0.0f, 100.0f, "%.1f")) {
            stack_layout_->setSpacing(stack_layout_spacing_);
        }

        if (ImGui::RadioButton("Horizontal", &stack_layout_orientation_, 0)) {
            stack_layout_->setOrientation(engine::ui::Orientation::Horizontal);
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Vertical", &stack_layout_orientation_, 1)) {
            stack_layout_->setOrientation(engine::ui::Orientation::Vertical);
        }

        if (ImGui::Combo("Alignment", &stack_layout_alignment_, "Start\0Center\0End\0")) {
            stack_layout_->setContentAlignment(static_cast<engine::ui::Alignment>(stack_layout_alignment_));
        }

        if (ImGui::Checkbox("Auto Resize", &stack_layout_auto_resize_)) {
            stack_layout_->setAutoResize(stack_layout_auto_resize_);
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Dynamic Children (Add/Remove)");
        float dyn_size[2] = {stack_dynamic_item_size_.x, stack_dynamic_item_size_.y};
        if (ImGui::DragFloat2("Item Size", dyn_size, 1.0f, 4.0f, 512.0f, "%.1f")) {
            stack_dynamic_item_size_ = glm::vec2{dyn_size[0], dyn_size[1]};
            for (auto* p : stack_dynamic_items_) {
                if (p) {
                    p->setSize(stack_dynamic_item_size_);
                }
            }
        }
        if (ImGui::SliderFloat("Item Alpha", &stack_dynamic_item_alpha_, 0.0f, 1.0f, "%.2f")) {
            for (std::size_t i = 0; i < stack_dynamic_items_.size(); ++i) {
                auto* p = stack_dynamic_items_[i];
                if (!p) {
                    continue;
                }
                const float t = static_cast<float>((i % 6)) / 6.0f;
                const engine::utils::FColor base{0.2f + 0.8f * t, 0.35f, 0.9f - 0.6f * t, 1.0f};
                p->setBackgroundColor(withAlpha(base, stack_dynamic_item_alpha_));
            }
        }

        if (ImGui::Button("Add Item")) {
            auto p = std::make_unique<engine::ui::UIPanel>(glm::vec2{0.0f, 0.0f}, stack_dynamic_item_size_);
            const float t = static_cast<float>((stack_dynamic_items_.size() % 6)) / 6.0f;
            const engine::utils::FColor base{0.2f + 0.8f * t, 0.35f, 0.9f - 0.6f * t, 1.0f};
            p->setBackgroundColor(withAlpha(base, stack_dynamic_item_alpha_));
            auto* ptr = p.get();
            stack_dynamic_items_.push_back(ptr);
            stack_layout_->addChild(std::move(p));
        }
        ImGui::SameLine();
        if (ImGui::Button("Remove Item")) {
            if (!stack_dynamic_items_.empty()) {
                auto* ptr = stack_dynamic_items_.back();
                stack_dynamic_items_.pop_back();
                stack_layout_->removeChild(ptr);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear Items")) {
            while (!stack_dynamic_items_.empty()) {
                auto* ptr = stack_dynamic_items_.back();
                stack_dynamic_items_.pop_back();
                stack_layout_->removeChild(ptr);
            }
        }
        ImGui::Text("Dynamic Item Count: %d", static_cast<int>(stack_dynamic_items_.size()));

        if (ImGui::Button("Force Layout")) {
            stack_layout_->forceLayout();
        }
    }

    if (grid_layout_ && ImGui::CollapsingHeader("Grid Layout", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::SliderInt("Columns", &grid_layout_columns_, 1, 10)) {
            grid_layout_->setColumnCount(grid_layout_columns_);
        }

        float cell_size[2] = {grid_layout_cell_size_.x, grid_layout_cell_size_.y};
        if (ImGui::DragFloat2("Cell Size", cell_size, 1.0f, 1.0f, 200.0f, "%.1f")) {
            grid_layout_cell_size_ = glm::vec2{cell_size[0], cell_size[1]};
            grid_layout_->setCellSize(grid_layout_cell_size_);
        }
        
        float spacing[2] = {grid_layout_spacing_.x, grid_layout_spacing_.y};
        if (ImGui::DragFloat2("Grid Spacing", spacing, 1.0f, 0.0f, 50.0f, "%.1f")) {
            grid_layout_spacing_ = glm::vec2{spacing[0], spacing[1]};
            grid_layout_->setSpacing(grid_layout_spacing_);
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Dynamic Children (Add/Remove)");
        if (ImGui::Button("Add Dummy Cell")) {
            auto p = std::make_unique<engine::ui::UIPanel>(glm::vec2{0.0f, 0.0f}, grid_layout_cell_size_);
            const float t = static_cast<float>((grid_dynamic_items_.size() % 6)) / 6.0f;
            const engine::utils::FColor base{0.9f - 0.6f * t, 0.25f + 0.6f * t, 0.35f, 1.0f};
            p->setBackgroundColor(withAlpha(base, 0.25f));
            auto* ptr = p.get();
            grid_dynamic_items_.push_back(ptr);
            grid_layout_->addChild(std::move(p));
        }
        ImGui::SameLine();
        if (ImGui::Button("Remove Dummy Cell")) {
            if (!grid_dynamic_items_.empty()) {
                auto* ptr = grid_dynamic_items_.back();
                grid_dynamic_items_.pop_back();
                grid_layout_->removeChild(ptr);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear Dummy Cells")) {
            while (!grid_dynamic_items_.empty()) {
                auto* ptr = grid_dynamic_items_.back();
                grid_dynamic_items_.pop_back();
                grid_layout_->removeChild(ptr);
            }
        }
        ImGui::Text("Dummy Cell Count: %d", static_cast<int>(grid_dynamic_items_.size()));

        if (ImGui::Button("Force Layout")) {
            grid_layout_->forceLayout();
        }
    }
    
    if (!item_slots_.empty() && ImGui::CollapsingHeader("Item Slots + Drag/Drop", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Swap On Drop (if target has item)", &drop_swaps_);
        ImGui::Checkbox("Discard When Dropped Outside", &drop_outside_discards_);
        ImGui::SliderFloat("Drag Preview Alpha", &drag_preview_alpha_, 0.0f, 1.0f, "%.2f");
        ImGui::Text("Slots: %d (drag apple/corn to empty slot, then drop)", static_cast<int>(item_slots_.size()));

        const int max_index = std::max(0, static_cast<int>(item_slots_.size()) - 1);
        slot_edit_index_ = std::clamp(slot_edit_index_, 0, max_index);
        if (ImGui::SliderInt("Edit Slot Index", &slot_edit_index_, 0, max_index)) {
            // No-op: keep index in range
        }

        auto* slot = item_slots_[slot_edit_index_];
        if (slot) {
            const auto item = slot->getSlotItem();
            ImGui::Text("Has Item: %s", item ? "Yes" : "No");

            bool selected = slot->isSelected();
            if (ImGui::Checkbox("Selected", &selected)) {
                slot->setSelected(selected);
            }

            int count = item ? item->count : 0;
            if (ImGui::InputInt("Count", &count)) {
                slot->setItemCount(std::max(0, count));
            }

            float cooldown = slot->getCooldown();
            if (ImGui::SliderFloat("Cooldown", &cooldown, 0.0f, 1.0f, "%.2f")) {
                slot->setCooldown(cooldown);
            }

            if (ImGui::Button("Set Empty")) {
                slot->clearItem();
            }
            ImGui::SameLine();
            if (ImGui::Button("Set Apple")) {
                slot->setSlotItem(entt::hashed_string{"apple"}.value(), 7,
                                  engine::render::Image(CROPS_ATLAS, engine::utils::Rect{glm::vec2{0,0}, glm::vec2{16,16}}));
            }
            ImGui::SameLine();
            if (ImGui::Button("Set Corn")) {
                slot->setSlotItem(entt::hashed_string{"corn"}.value(), 5,
                                  engine::render::Image(CROPS_ATLAS, engine::utils::Rect{glm::vec2{16,0}, glm::vec2{16,16}}));
            }
        }
    }
}

} // namespace tools::ui
