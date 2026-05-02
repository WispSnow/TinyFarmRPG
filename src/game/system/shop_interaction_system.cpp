#include "game/system/shop_interaction_system.h"

#include "game/component/merchant_component.h"
#include "game/component/npc_component.h"
#include "game/component/state_component.h"
#include "game/data/item_catalog.h"
#include "game/data/shop_catalog.h"
#include "game/defs/commands.h"
#include "game/defs/events.h"
#include "game/domain/shop_transaction_service.h"
#include "game/scene/shop_menu_scene.h"
#include "game/system/system_helpers.h"

#include "engine/component/name_component.h"
#include "engine/component/transform_component.h"
#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/utils/events.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <glm/geometric.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <spdlog/spdlog.h>

namespace {

constexpr std::uint8_t MERCHANT_DIALOGUE_CHANNEL = 0;
constexpr float MERCHANT_DIALOGUE_CLOSE_DISTANCE_PX = 64.0F;
constexpr std::string_view DEFAULT_MERCHANT_GREETING = "Welcome to the shop";

[[nodiscard]] std::string merchantGreeting(const game::data::ShopData& shop) {
    return shop.greeting_.empty() ? std::string{DEFAULT_MERCHANT_GREETING} : shop.greeting_;
}

[[nodiscard]] std::string merchantSpeaker(entt::registry& registry, const entt::entity merchant) {
    if (auto* name = registry.try_get<engine::component::NameComponent>(merchant)) {
        return name->name_;
    }
    return {};
}

void emitDialogueBubbleHideNow(entt::dispatcher& dispatcher, const entt::entity merchant) {
    game::defs::DialogueHideEvent event{};
    event.target = merchant;
    event.channel = MERCHANT_DIALOGUE_CHANNEL;
    dispatcher.trigger(event);
}

} // namespace

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

    auto& dialogue = registry_.get_or_emplace<game::component::DialogueComponent>(event.target);
    if (dialogue.dialogue_id_ == entt::null) {
        dialogue.dialogue_id_ = shop->id_hash_;
    }

    if (!dialogue.active_) {
        showMerchantGreeting(event.target, dialogue, *shop);
        return;
    }

    if (!isMerchantGreetingInRange(event.player, event.target, dialogue)) {
        closeMerchantGreeting(event.target);
        return;
    }

    if (dialogue.cooldown_timer_ > 0.0F) {
        return;
    }

    closeMerchantGreeting(event.target);
    openShopMenu(event.player, *shop);
}

void ShopInteractionSystem::update(float) {
    if (active_merchant_ == entt::null) {
        return;
    }

    if (!registry_.valid(active_merchant_)) {
        active_merchant_ = entt::null;
        return;
    }

    auto* dialogue = registry_.try_get<game::component::DialogueComponent>(active_merchant_);
    if (!dialogue || !dialogue->active_) {
        active_merchant_ = entt::null;
        return;
    }

    if (!isMerchantGreetingInRange(helpers::getPlayerEntity(registry_), active_merchant_, *dialogue)) {
        closeMerchantGreeting(active_merchant_);
        return;
    }

    helpers::emitDialogueBubbleMove(
        dispatcher_,
        MERCHANT_DIALOGUE_CHANNEL,
        active_merchant_,
        helpers::computeHeadPosition(registry_, active_merchant_));
}

void ShopInteractionSystem::showMerchantGreeting(entt::entity merchant,
                                                 game::component::DialogueComponent& dialogue,
                                                 const game::data::ShopData& shop) {
    if (active_merchant_ != entt::null && active_merchant_ != merchant) {
        closeMerchantGreeting(active_merchant_);
    }

    dialogue.active_ = true;
    dialogue.current_line_ = 0;
    dialogue.cooldown_timer_ = dialogue.cooldown_;
    active_merchant_ = merchant;

    helpers::emitDialogueBubbleShow(
        dispatcher_,
        MERCHANT_DIALOGUE_CHANNEL,
        merchant,
        merchantSpeaker(registry_, merchant),
        merchantGreeting(shop),
        helpers::computeHeadPosition(registry_, merchant));

    registry_.emplace_or_replace<game::component::StateDirtyTag>(merchant);
    if (auto* state = registry_.try_get<game::component::StateComponent>(merchant)) {
        state->action_ = game::component::Action::Idle;
    }
}

bool ShopInteractionSystem::isMerchantGreetingInRange(const entt::entity player,
                                                      const entt::entity merchant,
                                                      const game::component::DialogueComponent& dialogue) const {
    if (player == entt::null || merchant == entt::null) {
        return false;
    }

    const auto* player_transform = registry_.try_get<engine::component::TransformComponent>(player);
    const auto* merchant_transform = registry_.try_get<engine::component::TransformComponent>(merchant);
    if (!player_transform || !merchant_transform) {
        return false;
    }

    const float close_distance =
        dialogue.interact_distance_ < MERCHANT_DIALOGUE_CLOSE_DISTANCE_PX
            ? dialogue.interact_distance_
            : MERCHANT_DIALOGUE_CLOSE_DISTANCE_PX;
    return glm::distance(player_transform->position_, merchant_transform->position_) <= close_distance;
}

void ShopInteractionSystem::closeMerchantGreeting(entt::entity merchant) {
    if (merchant == entt::null || !registry_.valid(merchant)) {
        if (active_merchant_ == merchant) {
            active_merchant_ = entt::null;
        }
        return;
    }

    if (auto* dialogue = registry_.try_get<game::component::DialogueComponent>(merchant)) {
        dialogue->active_ = false;
        dialogue->current_line_ = 0;
    }

    emitDialogueBubbleHideNow(dispatcher_, merchant);
    if (active_merchant_ == merchant) {
        active_merchant_ = entt::null;
    }
}

void ShopInteractionSystem::openShopMenu(entt::entity player, const game::data::ShopData& shop) {
    auto scene = std::make_unique<game::scene::ShopMenuScene>(
        "ShopMenu",
        context_,
        registry_,
        player,
        shop.id_,
        &shop_catalog_,
        &item_catalog_,
        &shop_transaction_service_);
    dispatcher_.trigger<engine::utils::PushSceneEvent>(engine::utils::PushSceneEvent{std::move(scene)});
}

} // namespace game::system
