#include "game/scene/shop_menu_scene.h"

#include "game/data/item_catalog.h"
#include "game/data/shop_catalog.h"
#include "game/domain/shop_transaction_service.h"

#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/input/input_manager.h"

#include <entt/core/hashed_string.hpp>
#include <spdlog/spdlog.h>

#include <utility>

namespace {

constexpr std::string_view DOCUMENT_PATH = "ui/rmlui/scenes/shop_menu.rml";
constexpr std::string_view MODEL_NAME = "shop_menu";

} // namespace

namespace game::scene {

using namespace entt::literals;

ShopMenuScene::ShopMenuScene(std::string_view name,
                             engine::core::Context& context,
                             entt::registry& game_registry,
                             const entt::entity player,
                             std::string shop_id,
                             const game::data::ShopCatalog* shop_catalog,
                             game::data::ItemCatalog* item_catalog,
                             game::domain::ShopTransactionService* shop_transaction_service)
    : engine::scene::Scene(name, context),
      game_registry_(game_registry),
      player_(player),
      shop_id_(std::move(shop_id)),
      shop_catalog_(shop_catalog),
      item_catalog_(item_catalog),
      shop_transaction_service_(shop_transaction_service),
      previous_state_(context.getGameState().getCurrentState()) {
}

ShopMenuScene::~ShopMenuScene() {
    disconnectRuntimeListeners();
    shutdownUI();
}

bool ShopMenuScene::init() {
    previous_state_ = context_.getGameState().getCurrentState();
    context_.getGameState().setState(engine::core::State::Paused);
    context_.getInputManager().pushContext(engine::input::InputContextId::Menu);
    context_pushed_ = true;

    if (!shop_catalog_ || !item_catalog_ || !shop_transaction_service_) {
        spdlog::error("ShopMenuScene: gameplay 依赖未完整注入。");
        return false;
    }
    if (player_ == entt::null || !game_registry_.valid(player_)) {
        spdlog::error("ShopMenuScene: player 实体无效。");
        return false;
    }

    const auto* shop = shop_catalog_->findShop(shop_id_);
    if (!shop) {
        spdlog::error("ShopMenuScene: shop_id='{}' 未在 ShopCatalog 中找到。", shop_id_);
        return false;
    }

    shop_title_ = shop->title_.empty() ? Rml::String{"Shop"} : Rml::String{shop->title_};
    shop_greeting_ = shop->greeting_.empty() ? Rml::String{"Welcome."} : Rml::String{shop->greeting_};

    if (!initUI()) {
        return false;
    }

    context_.getInputManager().onAction("menu_cancel"_hs).connect<&ShopMenuScene::onMenuCancelPressed>(this);
    if (!Scene::init()) {
        return false;
    }

    return true;
}

void ShopMenuScene::clean() {
    shutdownUI();
    disconnectRuntimeListeners();
    context_.getGameState().setState(previous_state_);
    if (context_pushed_) {
        context_.getInputManager().popContext();
        context_pushed_ = false;
    }
    Scene::clean();
}

bool ShopMenuScene::initUI() {
    auto* runtime = context_.getRmlUi();
    if (!runtime) {
        spdlog::error("ShopMenuScene: RmlUiRuntime 不可用。");
        return false;
    }

    document_controller_.attach(runtime, instanceId());
    auto constructor = document_controller_.createModel(MODEL_NAME);
    if (!constructor) {
        spdlog::error("ShopMenuScene: 创建 data model 失败。");
        return false;
    }

    constructor.Bind("shop_title", &shop_title_);
    constructor.Bind("shop_greeting", &shop_greeting_);

    if (!document_controller_.bindSimpleEvent(constructor, "close", [this] { onClose(); })) {
        spdlog::error("ShopMenuScene: 绑定 close 事件失败。");
        document_controller_.unload();
        return false;
    }

    if (!document_controller_.load(DOCUMENT_PATH)) {
        spdlog::error("ShopMenuScene: 加载 RML 文档失败。");
        document_controller_.unload();
        return false;
    }

    return true;
}

void ShopMenuScene::shutdownUI() {
    document_controller_.unload();
}

void ShopMenuScene::disconnectRuntimeListeners() {
    context_.getInputManager().onAction("menu_cancel"_hs).disconnect<&ShopMenuScene::onMenuCancelPressed>(this);
}

bool ShopMenuScene::onMenuCancelPressed() {
    requestPopScene();
    return true;
}

void ShopMenuScene::onClose() {
    requestPopScene();
}

} // namespace game::scene
