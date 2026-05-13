#pragma once

#include "game/data/rpg_types.h"

#include <entt/entity/entity.hpp>

#include <string_view>

namespace game::component {
struct ActorEquipmentLoadout;
}

namespace game::data {
class RpgCatalog;
struct ActorData;
}

namespace game::battle {

struct ActorResolvedStats {
    game::data::ParamArray params{};
};

[[nodiscard]] ActorResolvedStats resolveActorStats(const game::data::RpgCatalog& catalog,
                                                   const game::data::ActorData& actor,
                                                   const game::component::ActorEquipmentLoadout* loadout);

[[nodiscard]] ActorResolvedStats resolveActorStats(const game::data::RpgCatalog& catalog,
                                                   std::string_view actor_id,
                                                   const game::component::ActorEquipmentLoadout* loadout);

} // namespace game::battle
