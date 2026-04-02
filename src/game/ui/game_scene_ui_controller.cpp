#include "game_scene_ui_controller.h"

#include "engine/core/context.h"
#include "engine/input/input_manager.h"
#include "engine/render/camera.h"
#include "engine/ui/rmlui/rml_bind_helpers.h"
#include "engine/ui/rmlui/rml_screen_fade.h"
#include "game/component/hotbar_component.h"
#include "game/component/tags.h"
#include "game/data/game_time.h"
#include "game/data/item_catalog.h"
#include "game/defs/commands.h"
#include "game/ui/dialogue_bubble_controller.h"
#include "game/ui/dialogue_bubble_view.h"
#include "game/ui/hotbar_ui.h"
#include "game/ui/item_tooltip_ui.h"
#include "game/ui/time_clock_hud.h"

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <string>
#include <string_view>

using namespace entt::literals;

namespace {

constexpr std::string_view GAME_OVERLAY_DOCUMENT_PATH = "ui/rmlui/hud/game_overlay.rml";
constexpr std::string_view GAME_OVERLAY_MODEL_NAME = "game_overlay";

using engine::ui::rmlui::updateBoundString;

[[nodiscard]] std::string promptTextForAction(engine::input::InputManager& input_manager, entt::id_type action_id) {
    if (const auto prompt = input_manager.getActionPrompt(action_id); prompt.has_value()) {
        return prompt->fallback_text;
    }

    return "-";
}

} // namespace

namespace game::ui {

GameSceneUiController::GameSceneUiController(engine::core::Context& context,
                                             entt::registry& registry,
                                             uint64_t scene_instance_id,
                                             game::data::ItemCatalog* item_catalog,
                                             MenuRequestHandler on_menu_requested)
    : context_(context),
      registry_(registry),
      scene_instance_id_(scene_instance_id),
      item_catalog_(item_catalog),
      on_menu_requested_(std::move(on_menu_requested)) {
}

GameSceneUiController::~GameSceneUiController() {
    clean();
}

bool GameSceneUiController::init() {
    clean();

    auto* rml_runtime = context_.getRmlUi();
    if (!rml_runtime) {
        spdlog::error("GameSceneUiController: RmlUiRuntime 不可用，无法初始化游戏 HUD。");
        return false;
    }

    time_clock_hud_ = std::make_unique<game::ui::TimeClockHud>(*rml_runtime, scene_instance_id_);

    hotbar_ui_ = std::make_unique<game::ui::HotbarUI>(*rml_runtime, context_, scene_instance_id_, item_catalog_);
    if (!hotbar_ui_ || !hotbar_ui_->isReady()) {
        spdlog::error("GameSceneUiController: 创建 HotbarUI 失败。");
        clean();
        return false;
    }

    item_tooltip_ui_ = std::make_unique<game::ui::ItemTooltipUI>(context_, scene_instance_id_);
    dialogue_controller_ = std::make_unique<game::ui::DialogueBubbleController>(context_.getDispatcher());

    dialogue_bubbles_[0] = std::make_unique<game::ui::DialogueBubbleView>(context_, scene_instance_id_);
    dialogue_bubbles_[1] = std::make_unique<game::ui::DialogueBubbleView>(context_, scene_instance_id_);
    dialogue_bubbles_[2] = std::make_unique<game::ui::DialogueBubbleView>(context_, scene_instance_id_);

    dialogue_controller_->registerBubble(0, dialogue_bubbles_[0].get());
    dialogue_controller_->registerBubble(1, dialogue_bubbles_[1].get());
    dialogue_controller_->registerBubble(2, dialogue_bubbles_[2].get(), {0.0F, -56.0F});

    hotbar_ui_->setTooltipUI(item_tooltip_ui_.get());
    if (const entt::entity player = findPlayerEntity(); player != entt::null) {
        hotbar_ui_->setTarget(player);
    }

    overlay_controller_.attach(rml_runtime, scene_instance_id_);
    if (auto overlay_constructor = overlay_controller_.createModel(GAME_OVERLAY_MODEL_NAME)) {
        overlay_constructor.Bind("primary_prompt_text", &primary_prompt_text_);
        overlay_constructor.Bind("secondary_prompt_text", &secondary_prompt_text_);
        overlay_constructor.Bind("inventory_prompt_text", &inventory_prompt_text_);
        overlay_constructor.Bind("pause_prompt_text", &pause_prompt_text_);
        overlay_constructor.Bind("show_prompt_bar", &show_prompt_bar_);

        if (!overlay_controller_.bindSimpleEvent(overlay_constructor, "menu", [this] {
                if (on_menu_requested_) {
                    on_menu_requested_();
                }
            })) {
            spdlog::error("GameSceneUiController: 绑定 overlay data event 回调失败，游戏内菜单按钮将不可用。");
            overlay_controller_.unload();
        } else {
            refreshOverlayPrompts();
            overlay_controller_.markAllDirty();

            if (!overlay_controller_.load(GAME_OVERLAY_DOCUMENT_PATH)) {
                spdlog::error("GameSceneUiController: 加载 overlay 文档失败，游戏内菜单按钮将不可用。");
                overlay_controller_.unload();
            }
        }
    } else {
        spdlog::error("GameSceneUiController: 创建 overlay data model 失败，输入提示将不可用。");
    }

    rml_screen_fade_ = std::make_unique<engine::ui::rmlui::RmlScreenFade>(*rml_runtime, scene_instance_id_);
    screen_fade_ = rml_screen_fade_.get();

    spdlog::debug("GameSceneUiController 初始化完成。");
    return true;
}

void GameSceneUiController::update(float delta_time) {
    refreshOverlayPrompts();

    if (time_clock_hud_) {
        time_clock_hud_->update(registry_.ctx().find<game::data::GameTime>());
    }
    if (rml_screen_fade_) {
        rml_screen_fade_->update(delta_time);
    }
    if (item_tooltip_ui_) {
        item_tooltip_ui_->update(delta_time);
    }
}

void GameSceneUiController::refreshAnchoredWidgets(const engine::render::Camera& camera, float interpolation_alpha) {
    for (auto& bubble : dialogue_bubbles_) {
        if (bubble) {
            bubble->refreshAnchoredPosition(camera, interpolation_alpha);
        }
    }
}

void GameSceneUiController::clean() {
    auto& dispatcher = context_.getDispatcher();
    dispatcher.clear<game::defs::DialogueShowEvent>();
    dispatcher.clear<game::defs::DialogueMoveEvent>();
    dispatcher.clear<game::defs::DialogueHideEvent>();

    time_clock_hud_.reset();
    dialogue_controller_.reset();
    for (auto& bubble : dialogue_bubbles_) {
        bubble.reset();
    }
    hotbar_ui_.reset();
    item_tooltip_ui_.reset();
    screen_fade_ = nullptr;
    rml_screen_fade_.reset();
    overlay_controller_.unload();
    primary_prompt_text_.clear();
    secondary_prompt_text_.clear();
    inventory_prompt_text_.clear();
    pause_prompt_text_.clear();
    show_prompt_bar_ = true;
}

bool GameSceneUiController::toggleHotbar() {
    if (!hotbar_ui_) {
        return false;
    }

    hotbar_ui_->toggle();
    if (!hotbar_ui_->isVisible()) {
        return true;
    }

    const entt::entity player = findPlayerEntity();
    if (player == entt::null) {
        return true;
    }

    // HotbarSyncCommand 走 dispatcher 队列，显示 hotbar 后到同步事件抵达前存在一个短窗口。
    // 这里先刷新 target，确保用户立刻点击/右键 hotbar 时也会路由到当前玩家。
    hotbar_ui_->setTarget(player);
    context_.getDispatcher().enqueue<game::defs::HotbarSyncCommand>(player);
    return true;
}

void GameSceneUiController::applyHotbarChanged(const game::defs::HotbarChanged& evt) {
    if (!hotbar_ui_ || !registry_.valid(evt.target)) {
        return;
    }

    hotbar_ui_->setTarget(evt.target);

    if (evt.full_sync) {
        hotbar_ui_->clearAllSlots();
        hotbar_ui_->resetInventoryMappings();
    }

    for (const auto& slot : evt.slots) {
        if (slot.hotbar_index < 0 || slot.hotbar_index >= game::component::HotbarComponent::SLOT_COUNT) {
            continue;
        }

        hotbar_ui_->setSlotInventoryIndex(slot.hotbar_index, slot.inventory_slot_index);
        if (slot.item_id != entt::null && slot.count > 0) {
            hotbar_ui_->setSlotItem(slot.hotbar_index, engine::ui::SlotItem{slot.item_id, slot.count});
        } else {
            hotbar_ui_->clearSlot(slot.hotbar_index);
        }
    }
}

void GameSceneUiController::applyHotbarSlotChanged(const game::defs::HotbarSlotChanged& evt) {
    if (!hotbar_ui_ || !registry_.valid(evt.target)) {
        return;
    }

    hotbar_ui_->setTarget(evt.target);
    hotbar_ui_->setActiveSlot(evt.slot_index);
}

void GameSceneUiController::setPromptBarVisible(bool visible) {
    if (show_prompt_bar_ == visible) {
        return;
    }

    show_prompt_bar_ = visible;
    if (overlay_controller_.isModelValid()) {
        overlay_controller_.markDirty("show_prompt_bar");
    }
}

entt::entity GameSceneUiController::findPlayerEntity() const {
    auto player_view = registry_.view<game::component::PlayerTag>();
    if (player_view.empty()) {
        return entt::null;
    }

    return *player_view.begin();
}

void GameSceneUiController::refreshOverlayPrompts() {
    if (!overlay_controller_.isModelValid()) {
        return;
    }

    auto& input_manager = context_.getInputManager();
    if (updateBoundString(primary_prompt_text_, promptTextForAction(input_manager, "primary_action"_hs))) {
        overlay_controller_.markDirty("primary_prompt_text");
    }
    if (updateBoundString(secondary_prompt_text_, promptTextForAction(input_manager, "secondary_action"_hs))) {
        overlay_controller_.markDirty("secondary_prompt_text");
    }
    if (updateBoundString(inventory_prompt_text_, promptTextForAction(input_manager, "inventory"_hs))) {
        overlay_controller_.markDirty("inventory_prompt_text");
    }
    if (updateBoundString(pause_prompt_text_, promptTextForAction(input_manager, "pause"_hs))) {
        overlay_controller_.markDirty("pause_prompt_text");
    }
}

} // namespace game::ui
