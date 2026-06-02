#pragma once

#include <entt/entity/fwd.hpp>

#include <string>
#include <string_view>

namespace game::data {
class RpgCatalog;
}

namespace game::runtime {
class LocalizationService;
}

namespace game::ui {

/// @brief Localized fallback for the default player actor name.
[[nodiscard]] std::string defaultPlayerDisplayName(const game::runtime::LocalizationService* localization);

/// @brief Resolve an actor's display name, honoring custom player identity for actor.player.
[[nodiscard]] std::string resolveActorDisplayName(const entt::registry& registry,
                                                  entt::entity player,
                                                  std::string_view actor_id,
                                                  const game::data::RpgCatalog* rpg_catalog,
                                                  const game::runtime::LocalizationService* localization);

} // namespace game::ui
