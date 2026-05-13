#include "item_use_system.h"
#include "inventory_helpers.h"
#include "system_helpers.h"

#include "game/battle/actor_stats_resolver.h"
#include "game/component/inventory_component.h"
#include "game/component/party_component.h"
#include "game/component/party_equipment_component.h"
#include "game/component/party_runtime_stats_component.h"
#include "game/data/item_catalog.h"
#include "game/data/rpg_catalog.h"
#include "game/defs/commands.h"
#include "game/defs/events.h"
#include "game/domain/inventory_domain_service.h"

#include <algorithm>
#include <cstddef>
#include <sstream>
#include <string_view>
#include <vector>

namespace game::system {

namespace {

constexpr float NOTIFICATION_SECONDS = 2.0f;
constexpr std::uint8_t NOTIFICATION_CHANNEL = 2;

using game::system::detail::stackLimitOrDefault;

[[nodiscard]] bool containsString(const std::vector<std::string>& values, const std::string_view value) {
    return std::any_of(values.begin(), values.end(), [value](const std::string& current) {
        return current == value;
    });
}

[[nodiscard]] const game::component::ActorEquipmentLoadout* findLoadout(const entt::registry& registry,
                                                                        entt::entity player,
                                                                        std::string_view actor_id) {
    const auto* equipment = registry.try_get<game::component::PartyEquipmentComponent>(player);
    if (!equipment) {
        return nullptr;
    }
    const auto it = equipment->loadouts_by_actor_id_.find(std::string{actor_id});
    return it == equipment->loadouts_by_actor_id_.end() ? nullptr : &it->second;
}

[[nodiscard]] int paramValue(const game::data::ParamArray& params, const game::data::ParamIndex index) {
    return params[static_cast<std::size_t>(index)];
}

} // namespace

ItemUseSystem::ItemUseSystem(entt::registry& registry,
                             entt::dispatcher& dispatcher,
                             game::data::ItemCatalog& catalog,
                             game::domain::InventoryDomainService& inventory_domain_service,
                             const game::data::RpgCatalog* rpg_catalog)
    : registry_(registry),
      dispatcher_(dispatcher),
      catalog_(catalog),
      rpg_catalog_(rpg_catalog),
      inventory_domain_service_(inventory_domain_service) {
    dispatcher_.sink<game::defs::UseItemCommand>().connect<&ItemUseSystem::onUseItem>(this);
}

ItemUseSystem::~ItemUseSystem() {
    dispatcher_.sink<game::defs::UseItemCommand>().disconnect<&ItemUseSystem::onUseItem>(this);
}

void ItemUseSystem::update(float delta_time) {
    updateNotification(delta_time);
}

void ItemUseSystem::updateNotification(float delta_time) {
    helpers::updateTimedNotification(registry_, dispatcher_, NOTIFICATION_CHANNEL, notification_, delta_time);
}

void ItemUseSystem::onUseItem(const game::defs::UseItemCommand& evt) {
    if (evt.target == entt::null || evt.inventory_slot_index < 0 || evt.count <= 0) return;
    if (!registry_.valid(evt.target) || !registry_.all_of<game::component::InventoryComponent>(evt.target)) return;

    auto& inv = registry_.get<game::component::InventoryComponent>(evt.target);
    if (evt.inventory_slot_index >= inv.slotCount()) return;

    auto& stack = inv.slot(evt.inventory_slot_index);
    if (stack.empty()) return;

    const auto* item = catalog_.findItem(stack.item_id_);
    if (!item) return;

    if (item->battle_use_ && evt.actor_target_id.has_value()) {
        const auto& use_cfg = *item->battle_use_;
        if (use_cfg.scope != game::data::Scope::OneAlly || !rpg_catalog_) {
            return;
        }
        const std::string& actor_id = *evt.actor_target_id;
        if (actor_id.empty() || !rpg_catalog_->findActor(actor_id)) {
            return;
        }
        const auto* party = registry_.try_get<game::component::PartyComponent>(evt.target);
        if (!party || !containsString(party->recruited_actor_ids_, actor_id)) {
            return;
        }

        const int use_times = std::max(1, evt.count);
        const int consume_total = std::max(1, use_cfg.consume) * use_times;
        if (stack.count_ < consume_total) {
            return;
        }

        auto& runtime_stats = registry_.get_or_emplace<game::component::PartyRuntimeStatsComponent>(evt.target);
        auto [state_it, inserted] = runtime_stats.states_by_actor_id_.try_emplace(actor_id);
        auto& state = state_it->second;
        const auto resolved = game::battle::resolveActorStats(*rpg_catalog_, actor_id, findLoadout(registry_, evt.target, actor_id));
        const int max_hp = paramValue(resolved.params, game::data::ParamIndex::Mhp);
        const int max_mp = paramValue(resolved.params, game::data::ParamIndex::Mmp);
        if (inserted) {
            state.current_hp = max_hp;
            state.current_mp = max_mp;
        } else {
            state.current_hp = std::clamp(state.current_hp, 0, max_hp);
            state.current_mp = std::clamp(state.current_mp, 0, max_mp);
        }

        for (const auto& effect : use_cfg.effects) {
            const int amount = effect.amount * use_times;
            if (effect.type == game::data::BattleItemEffectType::RecoverHp) {
                state.current_hp = std::clamp(state.current_hp + amount, 0, max_hp);
            } else if (effect.type == game::data::BattleItemEffectType::RecoverMp) {
                state.current_mp = std::clamp(state.current_mp + amount, 0, max_mp);
            }
        }
        ++runtime_stats.revision_;

        (void)inventory_domain_service_.removeItem(
            evt.target, stack.item_id_, consume_total, evt.inventory_slot_index);

        dispatcher_.trigger(game::defs::PartyRuntimeStatsChanged{
            .player = evt.target,
            .actor_id = actor_id,
            .full_sync = false,
        });

        if (evt.show_prompt) {
            helpers::showTimedNotification(
                registry_,
                dispatcher_,
                NOTIFICATION_CHANNEL,
                notification_,
                evt.target,
                std::string{},
                "Used",
                NOTIFICATION_SECONDS);
        }
        return;
    }

    if (!item->on_use_) return;

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
        if (!detail::simulateAdd(simulated, evt.inventory_slot_index, effect.item_id, add_total, stack_limit)) {
            ok = false;
            break;
        }
    }

    if (!ok) {
        if (evt.show_prompt) {
            helpers::showTimedNotification(
                registry_,
                dispatcher_,
                NOTIFICATION_CHANNEL,
                notification_,
                evt.target,
                std::string{},
                "Inventory full",
                NOTIFICATION_SECONDS);
        }
        return;
    }

    // Consume first (may free the source slot), then trigger effects.
    (void)inventory_domain_service_.removeItem(
        evt.target, stack.item_id_, consume_total, evt.inventory_slot_index);

    for (const auto& effect : use_cfg.effects) {
        if (effect.type != game::data::ItemUseEffectType::AddItem) continue;
        const int add_total = effect.count * use_times;
        if (effect.item_id == entt::null || add_total <= 0) continue;
        (void)inventory_domain_service_.addItem(
            evt.target, effect.item_id, add_total, evt.inventory_slot_index);
    }

    if (evt.show_prompt) {
        std::ostringstream oss;
        bool first = true;
        for (const auto& effect : use_cfg.effects) {
            if (effect.type != game::data::ItemUseEffectType::AddItem) continue;
            const int add_total = effect.count * use_times;
            if (effect.item_id == entt::null || add_total <= 0) continue;
            const auto* out_item = catalog_.findItem(effect.item_id);
            const std::string name = out_item ? out_item->display_name_ : std::to_string(static_cast<unsigned long long>(effect.item_id));
            if (!first) {
                oss << "\n";
            }
            first = false;
            oss << "Gained " << name << " x" << add_total;
        }
        const std::string text = first ? "Used" : oss.str();

        helpers::showTimedNotification(
            registry_,
            dispatcher_,
            NOTIFICATION_CHANNEL,
            notification_,
            evt.target,
            std::string{},
            text,
            NOTIFICATION_SECONDS);
    }
}

} // namespace game::system
