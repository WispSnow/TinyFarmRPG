#include "chest_system.h"
#include "system_helpers.h"

#include "game/component/chest_component.h"
#include "game/component/map_component.h"
#include "game/component/tags.h"
#include "game/data/item_catalog.h"
#include "game/defs/events.h"
#include "game/world/world_state.h"
#include "engine/component/animation_component.h"
#include "engine/component/sprite_component.h"
#include "engine/component/transform_component.h"
#include "engine/utils/events.h"

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <glm/geometric.hpp>
#include <algorithm>
#include <limits>
#include <sstream>

using namespace entt::literals;

namespace {
constexpr float NOTIFICATION_SECONDS = 2.0f;
constexpr std::uint8_t NOTIFICATION_CHANNEL = 1;
constexpr entt::id_type CHEST_OPEN_ANIM = "open"_hs;

std::string buildLootText(const game::data::ItemCatalog& catalog,
                          const std::vector<game::component::ItemReward>& rewards) {
    std::ostringstream out;
    bool first = true;
    for (const auto& reward : rewards) {
        if (reward.item_id_ == entt::null || reward.count_ <= 0) continue;
        const auto* item = catalog.findItem(reward.item_id_);
        const std::string_view name = item ? std::string_view(item->display_name_) : std::string_view("Unknown");
        if (!first) out << '\n';
        first = false;
        out << name << " x" << reward.count_;
    }
    return out.str();
}

void applyOpenedVisual(entt::registry& registry, entt::entity chest) {
    auto* sprite = registry.try_get<engine::component::SpriteComponent>(chest);
    auto* anim = registry.try_get<engine::component::AnimationComponent>(chest);
    if (!sprite || !anim) return;

    auto it = anim->animations_.find(CHEST_OPEN_ANIM);
    if (it == anim->animations_.end() || it->second.frames_.empty()) return;

    sprite->sprite_.src_rect_ = it->second.frames_.back().src_rect_;
}
} // namespace

namespace game::system {

ChestSystem::ChestSystem(entt::registry& registry,
                         entt::dispatcher& dispatcher,
                         game::world::WorldState& world_state,
                         game::data::ItemCatalog& item_catalog)
    : registry_(registry),
      dispatcher_(dispatcher),
      world_state_(world_state),
      item_catalog_(item_catalog) {
    dispatcher_.sink<engine::utils::AnimationFinishedEvent>().connect<&ChestSystem::onAnimationFinished>(this);
    dispatcher_.sink<game::defs::InteractRequest>().connect<&ChestSystem::onInteractRequest>(this);
}

ChestSystem::~ChestSystem() {
    dispatcher_.disconnect(this);
}

void ChestSystem::update(float delta_time) {
    updateNotification(delta_time);
}

void ChestSystem::updateNotification(float delta_time) {
    if (notification_timer_ <= 0.0f) {
        return;
    }

    notification_timer_ = std::max(0.0f, notification_timer_ - delta_time);
    if (notification_target_ != entt::null) {
        const glm::vec2 head_pos = helpers::computeHeadPosition(registry_, notification_target_);
        helpers::emitDialogueBubbleMove(dispatcher_, NOTIFICATION_CHANNEL, notification_target_, head_pos);
    }

    if (notification_timer_ <= 0.0f && notification_target_ != entt::null) {
        hideNotification(notification_target_);
        notification_target_ = entt::null;
    }
}

void ChestSystem::showNotification(entt::entity player, std::string text) {
    if (player == entt::null || text.empty()) return;

    notification_timer_ = NOTIFICATION_SECONDS;
    notification_target_ = player;
    helpers::emitDialogueBubbleShow(dispatcher_,
                                   NOTIFICATION_CHANNEL,
                                   player,
                                   std::string{},
                                   std::move(text),
                                   helpers::computeHeadPosition(registry_, player));
}

void ChestSystem::hideNotification(entt::entity player) {
    helpers::emitDialogueBubbleHide(dispatcher_, NOTIFICATION_CHANNEL, player);
}

void ChestSystem::onInteractRequest(const game::defs::InteractRequest& event) {
    const entt::entity player = helpers::getPlayerEntity(registry_);
    if (player == entt::null || event.player != player) return;
    if (event.target == entt::null || !registry_.valid(event.target)) return;
    if (!registry_.any_of<game::component::ChestComponent>(event.target)) return;

    tryOpenChest(event.player, event.target);
}

bool ChestSystem::tryOpenChest(entt::entity player, entt::entity chest_entity) {
    if (player == entt::null || chest_entity == entt::null) return false;

    const entt::id_type current_map = world_state_.getCurrentMap();
    if (auto* map = registry_.try_get<game::component::MapId>(chest_entity); map && map->id_ != current_map) {
        return false;
    }

    auto* chest = registry_.try_get<game::component::ChestComponent>(chest_entity);
    if (!chest || chest->opened_) {
        return false;
    }

    const auto loot_text = buildLootText(item_catalog_, chest->rewards_);
    if (!loot_text.empty()) {
        showNotification(player, loot_text);
    }

    for (const auto& reward : chest->rewards_) {
        if (reward.item_id_ == entt::null || reward.count_ <= 0) continue;
        dispatcher_.trigger(game::defs::AddItemRequest{player, reward.item_id_, reward.count_});
    }

    dispatcher_.enqueue(engine::utils::PlayAnimationEvent{chest_entity, CHEST_OPEN_ANIM, false});
    chest->opened_ = true;

    if (auto* map_state = world_state_.getMapStateMutable(current_map)) {
        map_state->persistent.opened_chests.insert(chest->chest_id_);
    }

    return true;
}

void ChestSystem::onAnimationFinished(const engine::utils::AnimationFinishedEvent& event) {
    if (event.animation_id_ != CHEST_OPEN_ANIM) return;
    if (!registry_.any_of<game::component::ChestComponent>(event.entity_)) return;

    if (auto* anim = registry_.try_get<engine::component::AnimationComponent>(event.entity_)) {
        anim->current_animation_id_ = entt::null;
    }
    applyOpenedVisual(registry_, event.entity_);
}

} // namespace game::system
