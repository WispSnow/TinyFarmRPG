#include "light_toggle_system.h"

#include "game/component/tags.h"
#include "game/data/game_time.h"
#include "game/defs/events.h"
#include "game/world/world_state.h"
#include "engine/component/light_component.h"
#include "engine/component/tags.h"
#include "engine/component/transform_component.h"

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>

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
    std::ifstream file(std::filesystem::path(config_path.data()));
    if (!file.is_open()) {
        return false;
    }

    try {
        nlohmann::json json;
        file >> json;

        const auto& cfg = json.value("player_follow_light", nlohmann::json::object());
        wanted_on_ = cfg.value("enabled_by_default", wanted_on_);
        radius_ = cfg.value("radius", radius_);
        options_.intensity = cfg.value("intensity", options_.intensity);

        if (cfg.contains("color")) {
            const auto& color = cfg.at("color");
            options_.color.r = color.value("r", options_.color.r);
            options_.color.g = color.value("g", options_.color.g);
            options_.color.b = color.value("b", options_.color.b);
        }

        if (cfg.contains("offset")) {
            const auto& offset = cfg.at("offset");
            offset_.x = offset.value("x", offset_.x);
            offset_.y = offset.value("y", offset_.y);
        }

        return true;
    } catch (const std::exception& e) {
        spdlog::error("LightToggleSystem: 读取配置失败 {} - {}", config_path, e.what());
        return false;
    }
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
