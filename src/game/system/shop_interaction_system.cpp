#include "game/system/shop_interaction_system.h"

#include "game/component/merchant_component.h"
#include "game/data/item_catalog.h"
#include "game/data/shop_catalog.h"
#include "game/defs/commands.h"
#include "game/domain/shop_transaction_service.h"
#include "game/scene/shop_menu_scene.h"

#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/utils/events.h"

#include <cstdint>
#include <memory>
#include <utility>

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <spdlog/spdlog.h>

namespace game::system {

ShopInteractionSystem::ShopInteractionSystem(entt::registry& registry,
                                             engine::core::Context& context,
                                             const game::data::ShopCatalog& shop_catalog,
                                             game::data::ItemCatalog& item_catalog,
                                             game::domain::ShopTransactionService& shop_transaction_service)
    : registry_(registry),
      context_(context),
      dispatcher_(context.getDispatcher()),
      shop_catalog_(shop_catalog),
      item_catalog_(item_catalog),
      shop_transaction_service_(shop_transaction_service) {
    dispatcher_.sink<game::defs::InteractCommand>().connect<&ShopInteractionSystem::onInteractCommand>(this);
}

ShopInteractionSystem::~ShopInteractionSystem() {
    dispatcher_.disconnect(this);
}

void ShopInteractionSystem::onInteractCommand(const game::defs::InteractCommand& event) {
    if (context_.getGameState().isPaused()) {
        return;
    }
    if (event.player == entt::null || event.target == entt::null) {
        return;
    }
    if (!registry_.valid(event.player) || !registry_.valid(event.target)) {
        return;
    }

    const auto* merchant = registry_.try_get<game::component::MerchantComponent>(event.target);
    if (!merchant) {
        return;
    }
    if (merchant->shop_id_hash_ == entt::null || merchant->shop_id_.empty()) {
        spdlog::warn("ShopInteractionSystem: merchant target={} 缺少合法 shop_id，忽略交互。",
                     static_cast<std::uint32_t>(event.target));
        return;
    }

    const auto* shop = shop_catalog_.findShop(merchant->shop_id_hash_);
    if (!shop) {
        spdlog::warn("ShopInteractionSystem: merchant target={} 引用的 shop_id='{}' 未在 ShopCatalog 中找到。",
                     static_cast<std::uint32_t>(event.target),
                     merchant->shop_id_);
        return;
    }

    auto scene = std::make_unique<game::scene::ShopMenuScene>(
        "ShopMenu",
        context_,
        registry_,
        event.player,
        shop->id_,
        &shop_catalog_,
        &item_catalog_,
        &shop_transaction_service_);
    dispatcher_.trigger<engine::utils::PushSceneEvent>(engine::utils::PushSceneEvent{std::move(scene)});
}

} // namespace game::system
