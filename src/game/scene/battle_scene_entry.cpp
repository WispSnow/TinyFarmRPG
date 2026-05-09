#include "game/scene/battle_scene_entry.h"

#include "game/component/appearance_component.h"
#include "game/component/tags.h"
#include "game/data/appearance_catalog.h"

#include <entt/entity/registry.hpp>

#include <utility>

namespace game::scene {

namespace {

[[nodiscard]] entt::entity findPlayer(entt::registry& registry) {
    auto players = registry.view<game::component::PlayerTag>();
    return players.begin() == players.end() ? entt::null : *players.begin();
}

} // namespace

std::optional<AppearanceSnapshot> capturePlayerBattleAppearance(entt::registry& registry) {
    const entt::entity player = findPlayer(registry);
    if (player == entt::null) {
        return std::nullopt;
    }

    const auto* appearance = registry.try_get<game::component::AppearanceComponent>(player);
    if (!appearance) {
        return std::nullopt;
    }

    AppearanceSnapshot snapshot{};
    snapshot.profile_id = appearance->profile_id_;
    snapshot.gender = appearance->gender_;
    snapshot.slot_variants = appearance->slot_variants_;
    snapshot.valid = true;
    return snapshot;
}

AppearanceSnapshot makeBattleAppearanceSnapshot(const game::data::AppearanceProfile& profile) {
    AppearanceSnapshot snapshot{};
    snapshot.profile_id = profile.id_;
    snapshot.gender = profile.gender_;
    snapshot.slot_variants = profile.slots_;
    snapshot.valid = true;
    return snapshot;
}

std::optional<AppearanceSnapshot> defaultBattleAppearanceSnapshot(const game::data::AppearanceCatalog& catalog) {
    const auto* profile = catalog.defaultProfile();
    if (!profile) {
        return std::nullopt;
    }
    return makeBattleAppearanceSnapshot(*profile);
}

std::vector<BattleSpriteSeed> buildBattleSpriteSeeds(
    const std::vector<game::battle::BattleUnit>& units,
    std::optional<AppearanceSnapshot> player_appearance,
    std::string_view player_actor_id) {
    std::vector<BattleSpriteSeed> seeds;
    seeds.reserve(units.size());

    for (const auto& unit : units) {
        BattleSpriteSeed seed{};
        seed.unit_id = unit.id;
        seed.source_actor_id = unit.source_actor_id;
        seed.source_enemy_id = unit.source_enemy_id;
        if (unit.source_actor_id && *unit.source_actor_id == player_actor_id && player_appearance) {
            seed.appearance = *player_appearance;
        }
        seeds.push_back(std::move(seed));
    }

    return seeds;
}

} // namespace game::scene
