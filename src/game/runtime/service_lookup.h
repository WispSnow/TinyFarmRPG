#pragma once

#include "game/runtime/localization_service.h"

#include <entt/entity/registry.hpp>

namespace game::runtime {

/// @brief Find the runtime localization service stored in an EnTT registry context.
[[nodiscard]] inline LocalizationService* findLocalizationService(entt::registry& registry) {
    auto** service = registry.ctx().find<LocalizationService*>();
    return service ? *service : nullptr;
}

} // namespace game::runtime
