#include "game_scene_ui_controller.h"

#include "engine/core/context.h"
#include "engine/render/camera.h"
#include "engine/ui/rmlui/rml_screen_fade.h"
#include "game/component/hotbar_component.h"
#include "game/component/tags.h"
#include "game/data/game_time.h"
#include "game/data/item_catalog.h"
#include "game/defs/commands.h"
#include "game/defs/options_events.h"
#include "game/runtime/service_lookup.h"
#include "game/ui/dialogue_box_view.h"
#include "game/ui/dialogue_presentation_controller.h"
#include "game/ui/floating_notice_view.h"
#include "game/ui/hotbar_ui.h"
#include "game/ui/item_tooltip_ui.h"
#include "game/ui/time_clock_hud.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <spdlog/spdlog.h>

namespace game::ui {

GameSceneUiController::GameSceneUiController(engine::core::Context& context,
                                             entt::registry& registry,
                                             uint64_t scene_instance_id,
                                             game::data::ItemCatalog* item_catalog,
                                             const game::data::RpgCatalog* rpg_catalog)
    : context_(context),
      registry_(registry),
      scene_instance_id_(scene_instance_id),
      item_catalog_(item_catalog),
      rpg_catalog_(rpg_catalog) {
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

    auto* localization = game::runtime::findLocalizationService(registry_);
    time_clock_hud_ = std::make_unique<game::ui::TimeClockHud>(*rml_runtime, scene_instance_id_, localization);

    hotbar_ui_ = std::make_unique<game::ui::HotbarUI>(
        *rml_runtime,
        context_,
        scene_instance_id_,
        item_catalog_,
        localization);
    if (!hotbar_ui_ || !hotbar_ui_->isReady()) {
        spdlog::error("GameSceneUiController: 创建 HotbarUI 失败。");
        clean();
        return false;
    }

    item_tooltip_ui_ = std::make_unique<game::ui::ItemTooltipUI>(context_, scene_instance_id_);
    dialogue_box_ = std::make_unique<game::ui::DialogueBoxView>(context_, scene_instance_id_);
    floating_notices_[0] = std::make_unique<game::ui::FloatingNoticeView>(context_, scene_instance_id_);
    floating_notices_[1] = std::make_unique<game::ui::FloatingNoticeView>(context_, scene_instance_id_);
    dialogue_controller_ = std::make_unique<game::ui::DialoguePresentationController>(
        context_.getDispatcher(),
        registry_,
        dialogue_box_.get(),
        floating_notices_[0].get(),
        floating_notices_[1].get(),
        hotbar_ui_.get(),
        rpg_catalog_);

    hotbar_ui_->setTooltipUI(item_tooltip_ui_.get());
    if (const entt::entity player = findPlayerEntity(); player != entt::null) {
        hotbar_ui_->setTarget(player);
    }
    context_.getDispatcher().sink<game::defs::LanguageChangedEvent>()
        .connect<&GameSceneUiController::onLanguageChanged>(this);

    rml_screen_fade_ = std::make_unique<engine::ui::rmlui::RmlScreenFade>(*rml_runtime, scene_instance_id_);
    screen_fade_ = rml_screen_fade_.get();

    spdlog::debug("GameSceneUiController 初始化完成。");
    return true;
}

void GameSceneUiController::update(float delta_time) {
    if (time_clock_hud_) {
        time_clock_hud_->update(registry_.ctx().find<game::data::GameTime>());
    }
    if (rml_screen_fade_) {
        rml_screen_fade_->update(delta_time);
    }
    if (dialogue_controller_) {
        dialogue_controller_->update(delta_time);
    }
    if (item_tooltip_ui_) {
        item_tooltip_ui_->update(delta_time);
    }
}

void GameSceneUiController::refreshAnchoredWidgets(const engine::render::Camera& camera, float interpolation_alpha) {
    for (auto& notice : floating_notices_) {
        if (notice) {
            notice->refreshAnchoredPosition(camera, interpolation_alpha);
        }
    }
}

void GameSceneUiController::clean() {
    auto& dispatcher = context_.getDispatcher();
    dispatcher.sink<game::defs::LanguageChangedEvent>().disconnect<&GameSceneUiController::onLanguageChanged>(this);
    dispatcher.clear<game::defs::DialogueShowEvent>();
    dispatcher.clear<game::defs::DialogueMoveEvent>();
    dispatcher.clear<game::defs::DialogueHideEvent>();

    time_clock_hud_.reset();
    dialogue_controller_.reset();
    dialogue_box_.reset();
    for (auto& notice : floating_notices_) {
        notice.reset();
    }
    hotbar_ui_.reset();
    item_tooltip_ui_.reset();
    screen_fade_ = nullptr;
    rml_screen_fade_.reset();
}

void GameSceneUiController::onLanguageChanged(const game::defs::LanguageChangedEvent&) {
    if (time_clock_hud_) {
        time_clock_hud_->onLanguageChanged(registry_.ctx().find<game::data::GameTime>());
    }
    if (hotbar_ui_) {
        hotbar_ui_->onLanguageChanged();
    }
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

    hotbar_ui_->syncState(evt.target, evt.full_sync, evt.active_slot, evt.slots);
}

void GameSceneUiController::applyHotbarSlotChanged(const game::defs::HotbarSlotChanged& evt) {
    if (!hotbar_ui_ || !registry_.valid(evt.target)) {
        return;
    }

    hotbar_ui_->syncActiveSlot(evt.target, evt.slot_index);
}

entt::entity GameSceneUiController::findPlayerEntity() const {
    auto player_view = registry_.view<game::component::PlayerTag>();
    if (player_view.empty()) {
        return entt::null;
    }

    return *player_view.begin();
}

} // namespace game::ui
