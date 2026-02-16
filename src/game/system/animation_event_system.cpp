#include "animation_event_system.h"
#include "game/component/target_component.h"
#include "game/component/state_component.h"
#include "game/component/actor_component.h"
#include "game/component/inventory_component.h"
#include "game/component/tags.h"
#include "game/defs/events.h"
#include "game/defs/crop_defs.h"
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <spdlog/spdlog.h>

using namespace entt::literals;

namespace {
    inline game::defs::Tool getToolFromAction(game::component::Action action) {
        switch (action) {
            case game::component::Action::Hoe:
                return game::defs::Tool::Hoe;
            case game::component::Action::Watering:
                return game::defs::Tool::WateringCan;
            case game::component::Action::Pickaxe:
                return game::defs::Tool::Pickaxe;
            case game::component::Action::Axe:
                return game::defs::Tool::Axe;
            case game::component::Action::Sickle:
                return game::defs::Tool::Sickle;
            case game::component::Action::Planting:
                return game::defs::Tool::None;
            default:
                return game::defs::Tool::None;
        }
    }
}

namespace game::system {

AnimationEventSystem::AnimationEventSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : registry_(registry), dispatcher_(dispatcher) {
    dispatcher_.sink<engine::utils::AnimationEvent>().connect<&AnimationEventSystem::onAnimationEvent>(this);
}

AnimationEventSystem::~AnimationEventSystem() {
    dispatcher_.disconnect(this);
}

void AnimationEventSystem::onAnimationEvent(const engine::utils::AnimationEvent& event) {
    switch (event.event_name_id_) {
        case "tool_hit"_hs:
            handleToolHitEvent(event);
            break;
        case "seed_plant"_hs:
            handleSeedPlantEvent(event);
            break;
        case "harvest"_hs:
            handleHarvestEvent(event);
            break;
    }
}

void AnimationEventSystem::handleToolHitEvent(const engine::utils::AnimationEvent& event) {
    if (!registry_.valid(event.entity_)) {
        spdlog::warn("tool_hit 事件的 source entity 无效");
        return;
    }

    const auto* state_component = registry_.try_get<game::component::StateComponent>(event.entity_);
    if (state_component == nullptr) {
        spdlog::warn("tool_hit 事件的 source entity 缺少 StateComponent");
        return;
    }

    auto target_view = registry_.view<game::component::TargetComponent>();
    if (target_view.begin() == target_view.end()) {
        spdlog::warn("tool_hit 事件缺少 TargetComponent");
        return;
    }
    auto target_it = target_view.begin();
    const entt::entity target_entity = *target_it;
    ++target_it;
    if (target_it != target_view.end()) {
        spdlog::warn("tool_hit 事件的 TargetComponent 不唯一");
        return;
    }
    const auto& target_component = target_view.get<game::component::TargetComponent>(target_entity);

    const auto tool = getToolFromAction(state_component->action_);
    dispatcher_.enqueue<game::defs::UseToolEvent>(tool, target_component.position_);
}

void AnimationEventSystem::handleSeedPlantEvent(const engine::utils::AnimationEvent& event) {
    if (!registry_.valid(event.entity_)) {
        spdlog::warn("seed_plant 事件的 source entity 无效");
        return;
    }

    auto target_view = registry_.view<game::component::TargetComponent>();
    if (target_view.begin() == target_view.end()) {
        spdlog::warn("尝试种植但目标位置不存在");
        return;
    }
    const auto& target_component = target_view.get<game::component::TargetComponent>(*target_view.begin());

    auto* actor_component = registry_.try_get<game::component::ActorComponent>(event.entity_);
    auto* inventory_component = registry_.try_get<game::component::InventoryComponent>(event.entity_);
    if (!actor_component || !inventory_component) {
        spdlog::warn("seed_plant 事件的 source entity 缺少 ActorComponent 或 InventoryComponent");
        return;
    }

    if (actor_component->hold_seed_ == game::defs::CropType::Unknown) {
        spdlog::warn("尝试种植但未持有种子");
        return;
    }

    const int slot_index = actor_component->hold_seed_inventory_slot_;
    entt::id_type seed_item_id = actor_component->hold_seed_item_id_;
    if (slot_index < 0 || slot_index >= inventory_component->slotCount()) {
        spdlog::warn("尝试种植但激活槽位无效: {}", slot_index);
        return;
    }
    const auto& stack = inventory_component->slot(slot_index);
    if (stack.empty()) {
        spdlog::warn("尝试种植但激活槽位无可用种子");
        return;
    }
    if (seed_item_id == entt::null) {
        seed_item_id = stack.item_id_;
    } else if (stack.item_id_ != seed_item_id) {
        spdlog::warn("尝试种植但槽位物品与持有种子不匹配");
        return;
    }

    game::defs::UseSeedEvent seed_evt{};
    seed_evt.seed_type_ = actor_component->hold_seed_;
    seed_evt.world_pos_ = target_component.position_;
    seed_evt.source = event.entity_;
    seed_evt.seed_item_id_ = seed_item_id;
    seed_evt.inventory_slot_index_ = slot_index;
    dispatcher_.enqueue(seed_evt);

    spdlog::info("触发种植事件，种子类型: {}，位置: {}, {}，槽位: {}",
                static_cast<int>(actor_component->hold_seed_),
                target_component.position_.x, target_component.position_.y,
                slot_index);
}

void AnimationEventSystem::handleHarvestEvent(const engine::utils::AnimationEvent& /*event*/) {
    auto target_view = registry_.view<game::component::TargetComponent>();
    if (target_view.begin() == target_view.end()) return;
    const auto& target_component = target_view.get<game::component::TargetComponent>(*target_view.begin());

    dispatcher_.enqueue<game::defs::UseToolEvent>(game::defs::Tool::Sickle, target_component.position_);
    spdlog::info("触发收获事件，位置: {}, {}", target_component.position_.x, target_component.position_.y);
}

} // namespace game::system
