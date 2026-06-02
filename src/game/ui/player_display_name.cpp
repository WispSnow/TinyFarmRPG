#include "game/ui/player_display_name.h"

#include "game/component/player_identity_component.h"
#include "game/data/rpg_catalog.h"
#include "game/data/rpg_data.h"
#include "game/defs/party_ids.h"
#include "game/ui/localized_text.h"

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>

#include <string>

namespace game::ui {

std::string defaultPlayerDisplayName(const game::runtime::LocalizationService* localization) {
    return game::ui::localizeTextOrFallback(localization, "actor.player.name", "Alex");
}

std::string resolveActorDisplayName(const entt::registry& registry,
                                    const entt::entity player,
                                    const std::string_view actor_id,
                                    const game::data::RpgCatalog* rpg_catalog,
                                    const game::runtime::LocalizationService* localization) {
    if (actor_id == game::defs::kDefaultPlayerActorId) {
        if (player != entt::null && registry.valid(player)) {
            if (const auto* identity = registry.try_get<game::component::PlayerIdentityComponent>(player);
                identity && !identity->display_name_.empty()) {
                return identity->display_name_;
            }
        }
        return defaultPlayerDisplayName(localization);
    }

    const auto* actor = rpg_catalog ? rpg_catalog->findActor(actor_id) : nullptr;
    if (!actor) {
        return std::string{actor_id};
    }
    return actor->display_name_.empty()
               ? actor->id_
               : game::ui::tryLocalize(localization, actor->display_name_);
}

} // namespace game::ui
