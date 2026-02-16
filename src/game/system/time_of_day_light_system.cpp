#include "time_of_day_light_system.h"
#include "game/component/tags.h"
#include "game/data/game_time.h"
#include "engine/component/light_component.h"
#include "engine/component/tags.h"
#include "engine/utils/events.h"
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

namespace game::system {

namespace {

[[nodiscard]] bool isLightEntity(entt::registry& registry, entt::entity entity) {
    return registry.any_of<
        engine::component::PointLightComponent,
        engine::component::SpotLightComponent,
        engine::component::EmissiveRectComponent,
        engine::component::EmissiveSpriteComponent
    >(entity);
}

} // namespace

TimeOfDayLightSystem::TimeOfDayLightSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : registry_(registry), dispatcher_(dispatcher) {
    subscribe();
    applyVisibilityFromGameTime();
}

TimeOfDayLightSystem::~TimeOfDayLightSystem() {
    unsubscribe();
}

void TimeOfDayLightSystem::subscribe() {
    dispatcher_.sink<engine::utils::HourChangedEvent>().connect<&TimeOfDayLightSystem::onHourChanged>(this);
}

void TimeOfDayLightSystem::unsubscribe() {
    dispatcher_.sink<engine::utils::HourChangedEvent>().disconnect<&TimeOfDayLightSystem::onHourChanged>(this);
}

void TimeOfDayLightSystem::onHourChanged(const engine::utils::HourChangedEvent&) {
    applyVisibilityFromGameTime();
}

void TimeOfDayLightSystem::applyVisibilityFromGameTime() {
    auto* game_time = registry_.ctx().find<game::data::GameTime>();
    if (!game_time) {
        return;
    }

    const bool is_dark = game_time->isDarkForEmissives();

    auto apply_visibility = [&](entt::entity entity, bool should_be_visible) {
        if (!isLightEntity(registry_, entity)) {
            return;
        }

        if (should_be_visible) {
            if (registry_.all_of<engine::component::LightDisabledTag>(entity)) {
                registry_.remove<engine::component::LightDisabledTag>(entity);
            }
            return;
        }

        registry_.emplace_or_replace<engine::component::LightDisabledTag>(entity);
    };

    auto conflicting = registry_.view<game::component::NightOnlyTag, game::component::DayOnlyTag>();
    for (auto entity : conflicting) {
        apply_visibility(entity, false);
    }

    auto night_only = registry_.view<game::component::NightOnlyTag>(entt::exclude<game::component::DayOnlyTag>);
    for (auto entity : night_only) {
        apply_visibility(entity, is_dark);
    }

    auto day_only = registry_.view<game::component::DayOnlyTag>(entt::exclude<game::component::NightOnlyTag>);
    for (auto entity : day_only) {
        apply_visibility(entity, !is_dark);
    }
}

} // namespace game::system
