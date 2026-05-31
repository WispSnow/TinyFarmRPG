#include "light_toggle_system.h"

#include "game/component/tags.h"
#include "game/data/game_time.h"
#include "game/defs/events.h"
#include "game/world/world_state.h"
#include "engine/component/light_component.h"
#include "engine/component/tags.h"
#include "engine/component/transform_component.h"
#include "engine/utils/json_file_loader.h"
#include "engine/utils/json_helpers.h"

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

using namespace entt::literals;

namespace game::system {

namespace {
constexpr entt::id_type PLAYER_FOLLOW_LIGHT_TYPE = "player_follow_light"_hs;
} // namespace

LightToggleSystem::LightToggleSystem(entt::registry& registry, entt::dispatcher& dispatcher, std::string_view config_path)
    : registry_(registry), dispatcher_(dispatcher) {
    (void)loadConfig(config_path);
    subscribe();
}

LightToggleSystem::~LightToggleSystem() {
    unsubscribe();
}

void LightToggleSystem::subscribe() {
    dispatcher_.sink<game::defs::ToggleLightRequest>().connect<&LightToggleSystem::onToggleLightRequest>(this);
}

void LightToggleSystem::unsubscribe() {
    dispatcher_.sink<game::defs::ToggleLightRequest>().disconnect<&LightToggleSystem::onToggleLightRequest>(this);
}

bool LightToggleSystem::loadConfig(std::string_view config_path) {
    nlohmann::json json;
    if (!engine::utils::loadJsonObjectFile(config_path, json, "LightToggleSystem")) {
        return false;
    }

    const auto* cfg = engine::utils::json::findMember(json, "player_follow_light");
    if (!cfg || !cfg->is_object()) {
        return true;
    }

    bool next_wanted_on = wanted_on_;
    float next_radius = radius_;
    glm::vec2 next_offset = offset_;
    engine::utils::PointLightOptions next_options = options_;

    next_wanted_on = engine::utils::json::boolOr(*cfg, "enabled_by_default", next_wanted_on);
    next_radius = engine::utils::json::numberOr(*cfg, "radius", next_radius);
    next_options.intensity = engine::utils::json::numberOr(*cfg, "intensity", next_options.intensity);

    if (const auto* color = engine::utils::json::findMember(*cfg, "color"); color && color->is_object()) {
        next_options.color.r = engine::utils::json::numberOr(*color, "r", next_options.color.r);
        next_options.color.g = engine::utils::json::numberOr(*color, "g", next_options.color.g);
        next_options.color.b = engine::utils::json::numberOr(*color, "b", next_options.color.b);
    }

    if (const auto* offset = engine::utils::json::findMember(*cfg, "offset"); offset && offset->is_object()) {
        next_offset.x = engine::utils::json::numberOr(*offset, "x", next_offset.x);
        next_offset.y = engine::utils::json::numberOr(*offset, "y", next_offset.y);
    }

    wanted_on_ = next_wanted_on;
    radius_ = next_radius;
    offset_ = next_offset;
    options_ = next_options;
    return true;
}

void LightToggleSystem::onToggleLightRequest(const game::defs::ToggleLightRequest& evt) {
    if (evt.light_type_id != PLAYER_FOLLOW_LIGHT_TYPE) {
        return;
    }
    wanted_on_ = !wanted_on_;
}

void LightToggleSystem::update() {
    auto view = registry_.view<game::component::PlayerTag, engine::component::TransformComponent>();
    if (view.begin() == view.end()) {
        return;
    }
    applyToPlayer(*view.begin());
}

void LightToggleSystem::applyToPlayer(entt::entity player) {
    auto* game_time = registry_.ctx().find<game::data::GameTime>();
    const bool is_dark = game_time ? game_time->isDarkForEmissives() : false;

    bool is_indoor = false;
    if (auto** world_state_ptr = registry_.ctx().find<game::world::WorldState*>(); world_state_ptr && *world_state_ptr) {
        const auto map_id = (*world_state_ptr)->getCurrentMap();
        if (const auto* map_state = (*world_state_ptr)->getMapState(map_id)) {
            is_indoor = !map_state->info.in_world;
        }
    }

    auto* light_ptr = registry_.try_get<engine::component::PointLightComponent>(player);
    if (!light_ptr) {
        light_ptr = &registry_.emplace<engine::component::PointLightComponent>(player);
    }
    light_ptr->radius = radius_;
    light_ptr->offset = offset_;
    light_ptr->options = options_;

    const bool allowed = is_indoor || is_dark;
    const bool enabled = wanted_on_ && allowed;
    if (enabled) {
        if (registry_.all_of<engine::component::LightDisabledTag>(player)) {
            registry_.remove<engine::component::LightDisabledTag>(player);
        }
        return;
    }

    registry_.emplace_or_replace<engine::component::LightDisabledTag>(player);
}

} // namespace game::system
