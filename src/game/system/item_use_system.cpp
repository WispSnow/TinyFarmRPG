#include "item_use_system.h"
#include "inventory_helpers.h"
#include "system_helpers.h"

#include "game/component/inventory_component.h"
#include "game/data/item_catalog.h"
#include "game/defs/events.h"

#include <algorithm>
#include <sstream>

namespace game::system {

using detail::tryMerge;
using detail::tryFillEmpty;

namespace {

constexpr float NOTIFICATION_SECONDS = 2.0f;
constexpr std::uint8_t NOTIFICATION_CHANNEL = 2;

using game::system::detail::stackLimitOrDefault;

[[nodiscard]] bool simulateAdd(std::vector<game::component::ItemStack>& slots,
                               int preferred_slot_index,
                               entt::id_type item_id,
                               int count,
                               int stack_limit) {
    if (item_id == entt::null || count <= 0) return true;
    int remaining = count;

    if (preferred_slot_index >= 0 && preferred_slot_index < static_cast<int>(slots.size()) && remaining > 0) {
        remaining = tryMerge(slots[static_cast<std::size_t>(preferred_slot_index)], item_id, remaining, stack_limit);
        remaining = tryFillEmpty(slots[static_cast<std::size_t>(preferred_slot_index)], item_id, remaining, stack_limit);
    }

    for (auto& slot : slots) {
        if (remaining <= 0) break;
        remaining = tryMerge(slot, item_id, remaining, stack_limit);
    }

    for (auto& slot : slots) {
        if (remaining <= 0) break;
        remaining = tryFillEmpty(slot, item_id, remaining, stack_limit);
    }

    return remaining <= 0;
}

} // namespace

ItemUseSystem::ItemUseSystem(entt::registry& registry, entt::dispatcher& dispatcher, game::data::ItemCatalog& catalog)
    : registry_(registry), dispatcher_(dispatcher), catalog_(catalog) {
    dispatcher_.sink<game::defs::UseItemRequest>().connect<&ItemUseSystem::onUseItem>(this);
}

ItemUseSystem::~ItemUseSystem() {
    dispatcher_.sink<game::defs::UseItemRequest>().disconnect<&ItemUseSystem::onUseItem>(this);
}

void ItemUseSystem::update(float delta_time) {
    updateNotification(delta_time);
}

void ItemUseSystem::updateNotification(float delta_time) {
    if (notification_timer_ <= 0.0f) {
        return;
    }

    notification_timer_ = std::max(0.0f, notification_timer_ - delta_time);

    if (notification_target_ != entt::null && registry_.valid(notification_target_)) {
        helpers::emitDialogueBubbleMove(dispatcher_,
                                       NOTIFICATION_CHANNEL,
                                       notification_target_,
                                       helpers::computeHeadPosition(registry_, notification_target_));
    }

    if (notification_timer_ <= 0.0f && notification_target_ != entt::null) {
        helpers::emitDialogueBubbleHide(dispatcher_, NOTIFICATION_CHANNEL, notification_target_);
        notification_target_ = entt::null;
    }
}

void ItemUseSystem::onUseItem(const game::defs::UseItemRequest& evt) {
    if (evt.target == entt::null || evt.inventory_slot_index < 0 || evt.count <= 0) return;
    if (!registry_.valid(evt.target) || !registry_.all_of<game::component::InventoryComponent>(evt.target)) return;

    auto& inv = registry_.get<game::component::InventoryComponent>(evt.target);
    if (evt.inventory_slot_index >= inv.slotCount()) return;

    auto& stack = inv.slot(evt.inventory_slot_index);
    if (stack.empty()) return;

    const auto* item = catalog_.findItem(stack.item_id_);
    if (!item || !item->on_use_) return;

    const auto& use_cfg = *item->on_use_;
    const int use_times = std::max(1, evt.count);
    const int consume_total = std::max(1, use_cfg.consume) * use_times;
    if (stack.count_ < consume_total) {
        return;
    }

    // Pre-check capacity: simulate consume first, then apply effects (events).
    std::vector<game::component::ItemStack> simulated = inv.slots_;
    {
        auto& src = simulated[static_cast<std::size_t>(evt.inventory_slot_index)];
        if (src.empty() || src.item_id_ != stack.item_id_) {
            return;
        }
        src.count_ -= consume_total;
        if (src.count_ <= 0) {
            src.clear();
        }
    }

    bool ok = true;
    for (const auto& effect : use_cfg.effects) {
        if (effect.type != game::data::ItemUseEffectType::AddItem) continue;
        const int add_total = effect.count * use_times;
        if (effect.item_id == entt::null || add_total <= 0) continue;
        const int stack_limit = stackLimitOrDefault(&catalog_, effect.item_id);
        if (!simulateAdd(simulated, evt.inventory_slot_index, effect.item_id, add_total, stack_limit)) {
            ok = false;
            break;
        }
    }

    if (!ok) {
        if (evt.show_prompt) {
            notification_timer_ = NOTIFICATION_SECONDS;
            notification_target_ = evt.target;
            helpers::emitDialogueBubbleShow(dispatcher_,
                                           NOTIFICATION_CHANNEL,
                                           evt.target,
                                           std::string{},
                                           "背包空间不足",
                                           helpers::computeHeadPosition(registry_, evt.target));
        }
        return;
    }

    // Consume first (may free the source slot), then trigger effects.
    dispatcher_.trigger(game::defs::RemoveItemRequest{evt.target, stack.item_id_, consume_total, evt.inventory_slot_index});

    for (const auto& effect : use_cfg.effects) {
        if (effect.type != game::data::ItemUseEffectType::AddItem) continue;
        const int add_total = effect.count * use_times;
        if (effect.item_id == entt::null || add_total <= 0) continue;
        game::defs::AddItemRequest add{};
        add.target = evt.target;
        add.item_id = effect.item_id;
        add.count = add_total;
        add.preferred_slot_index = evt.inventory_slot_index;
        dispatcher_.trigger(add);
    }

    if (evt.show_prompt) {
        std::ostringstream oss;
        bool first = true;
        for (const auto& effect : use_cfg.effects) {
            if (effect.type != game::data::ItemUseEffectType::AddItem) continue;
            const int add_total = effect.count * use_times;
            if (effect.item_id == entt::null || add_total <= 0) continue;
            const auto* out_item = catalog_.findItem(effect.item_id);
            const std::string name = out_item ? out_item->display_name_ : std::to_string(static_cast<std::uint32_t>(effect.item_id));
            if (!first) {
                oss << "\n";
            }
            first = false;
            oss << "获得 " << name << " x" << add_total;
        }
        const std::string text = first ? "使用成功" : oss.str();

        notification_timer_ = NOTIFICATION_SECONDS;
        notification_target_ = evt.target;
        helpers::emitDialogueBubbleShow(dispatcher_,
                                       NOTIFICATION_CHANNEL,
                                       evt.target,
                                       std::string{},
                                       text,
                                       helpers::computeHeadPosition(registry_, evt.target));
    }
}

} // namespace game::system
