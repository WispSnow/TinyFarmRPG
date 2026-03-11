#include "inventory_menu_scene.h"

#include "game/ui/inventory_menu_ui.h"
#include "game/ui/item_tooltip_ui.h"

#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/input/input_manager.h"
#include "engine/render/opengl/gl_renderer.h"
#include "engine/ui/rmlui/rml_ui_layer.h"

#include <entt/core/hashed_string.hpp>
#include <spdlog/spdlog.h>

using namespace entt::literals;

namespace game::scene {

InventoryMenuScene::InventoryMenuScene(std::string_view name,
                                       engine::core::Context& context,
                                       entt::entity target,
                                       game::data::ItemCatalog* item_catalog)
    : engine::scene::Scene(name, context),
      target_(target),
      item_catalog_(item_catalog),
      previous_state_(context.getGameState().getCurrentState()) {
}

InventoryMenuScene::~InventoryMenuScene() {
    disconnectRuntimeListeners();
    menu_ui_.reset();
    tooltip_ui_.reset();
}

bool InventoryMenuScene::init() {
    previous_state_ = context_.getGameState().getCurrentState();
    context_.getGameState().setState(engine::core::State::Paused);
    context_.getInputManager().pushContext(engine::input::InputContextId::Menu);
    context_pushed_ = true;

    if (!initUI()) {
        return false;
    }

    context_.getInputManager().onAction("menu_cancel"_hs).connect<&InventoryMenuScene::onMenuCancelPressed>(this);

    if (!Scene::init()) {
        return false;
    }

    return true;
}

void InventoryMenuScene::update(float delta_time) {
    if (menu_ui_) {
        menu_ui_->update(delta_time);
        if (menu_ui_->consumeCloseRequested()) {
            requestPopScene();
        }
    }

    if (tooltip_ui_) {
        tooltip_ui_->update(delta_time);
    }

    Scene::update(delta_time);
}

void InventoryMenuScene::clean() {
    disconnectRuntimeListeners();
    menu_ui_.reset();
    tooltip_ui_.reset();

    context_.getGameState().setState(previous_state_);
    if (context_pushed_) {
        context_.getInputManager().popContext();
        context_pushed_ = false;
    }

    Scene::clean();
}

bool InventoryMenuScene::initUI() {
    auto* layer = context_.getGLRenderer().getRmlUILayer();
    if (!layer) {
        spdlog::error("InventoryMenuScene: RmlUILayer 不可用。");
        return false;
    }

    tooltip_ui_ = std::make_unique<game::ui::ItemTooltipUI>(context_, instance_id_);
    menu_ui_ = std::make_unique<game::ui::InventoryMenuUI>(*layer, context_, instance_id_, item_catalog_);
    if (!menu_ui_ || !menu_ui_->isReady()) {
        spdlog::error("InventoryMenuScene: 创建 InventoryMenuUI 失败。");
        tooltip_ui_.reset();
        menu_ui_.reset();
        return false;
    }

    menu_ui_->setTarget(target_);
    menu_ui_->setTooltipUI(tooltip_ui_.get());
    menu_ui_->syncFromTarget();
    menu_ui_->queueInitialFocus();
    return true;
}

void InventoryMenuScene::disconnectRuntimeListeners() {
    context_.getInputManager().onAction("menu_cancel"_hs).disconnect<&InventoryMenuScene::onMenuCancelPressed>(this);
}

bool InventoryMenuScene::onMenuCancelPressed() {
    if (menu_ui_ && menu_ui_->handleMenuCancel()) {
        return true;
    }

    requestPopScene();
    return true;
}

} // namespace game::scene
