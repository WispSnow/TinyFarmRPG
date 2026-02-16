#include "pickup_system.h"

#include "game/component/pickup_component.h"
#include "game/component/map_component.h"
#include "game/component/tags.h"
#include "game/defs/events.h"

#include "engine/component/collider_component.h"
#include "engine/component/tags.h"
#include "engine/component/transform_component.h"
#include "engine/utils/events.h"

#include <glm/common.hpp>
#include <glm/gtc/constants.hpp>

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <entt/core/hashed_string.hpp>

#include <algorithm>
#include <cmath>

using namespace entt::literals;

namespace {

float easeOutCubic(float t) {
    t = glm::clamp(t, 0.0f, 1.0f);
    const float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}

bool rectCircleOverlap(const glm::vec2& rect_min,
                       const glm::vec2& rect_size,
                       const glm::vec2& circle_center,
                       float circle_radius) {
    const glm::vec2 rect_max = rect_min + rect_size;
    const float closest_x = glm::clamp(circle_center.x, rect_min.x, rect_max.x);
    const float closest_y = glm::clamp(circle_center.y, rect_min.y, rect_max.y);
    const float dx = circle_center.x - closest_x;
    const float dy = circle_center.y - closest_y;
    return (dx * dx + dy * dy) <= circle_radius * circle_radius;
}

} // namespace

namespace game::system {

PickupSystem::PickupSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : registry_(registry), dispatcher_(dispatcher) {
}

void PickupSystem::update(float delta_time) {
    if (delta_time <= 0.0f) {
        return;
    }

    {
        auto view = registry_.view<game::component::ScatterMotionComponent, engine::component::TransformComponent>(
            entt::exclude<engine::component::NeedRemoveTag>
        );
        for (auto entity : view) {
            auto& motion = view.get<game::component::ScatterMotionComponent>(entity);
            auto& transform = view.get<engine::component::TransformComponent>(entity);

            motion.elapsed_ += delta_time;
            const float denom = std::max(motion.duration_, 1.0e-5f);
            const float t = glm::clamp(motion.elapsed_ / denom, 0.0f, 1.0f);
            const float ease = easeOutCubic(t);

            glm::vec2 pos = glm::mix(motion.start_pos_, motion.target_pos_, ease);
            pos.y -= std::sin(t * glm::pi<float>()) * motion.arc_height_;

            transform.position_ = pos;
            registry_.emplace_or_replace<engine::component::TransformDirtyTag>(entity);

            if (t >= 1.0f) {
                transform.position_ = motion.target_pos_;
                registry_.emplace_or_replace<engine::component::TransformDirtyTag>(entity);
                registry_.remove<game::component::ScatterMotionComponent>(entity);
            }
        }
    }

    entt::entity player = entt::null;
    game::component::MapId player_map{};
    glm::vec2 player_center{};
    float player_radius = 0.0f;
    {
        auto view = registry_.view<game::component::PlayerTag,
                                   game::component::MapId,
                                   engine::component::TransformComponent,
                                   engine::component::CircleCollider>();
        if (view.begin() == view.end()) {
            return;
        }
        player = *view.begin();
        player_map = view.get<game::component::MapId>(player);
        const auto& transform = view.get<engine::component::TransformComponent>(player);
        const auto& circle = view.get<engine::component::CircleCollider>(player);
        player_center = transform.position_ + circle.offset_;
        player_radius = circle.radius_;
    }

    auto pickup_view = registry_.view<game::component::PickupComponent,
                                      game::component::MapId,
                                      engine::component::TransformComponent,
                                      engine::component::AABBCollider>(
        entt::exclude<engine::component::NeedRemoveTag>
    );

    for (auto entity : pickup_view) {
        const auto& map = pickup_view.get<game::component::MapId>(entity);
        if (map.id_ != player_map.id_) {
            continue;
        }

        auto& pickup = pickup_view.get<game::component::PickupComponent>(entity);
        if (pickup.pickup_delay_ > 0.0f) {
            pickup.pickup_delay_ = std::max(0.0f, pickup.pickup_delay_ - delta_time);
            continue;
        }

        const auto& transform = pickup_view.get<engine::component::TransformComponent>(entity);
        const auto& collider = pickup_view.get<engine::component::AABBCollider>(entity);
        const glm::vec2 rect_center = transform.position_ + collider.offset_;
        const glm::vec2 rect_min = rect_center - collider.size_ * 0.5f;

        if (!rectCircleOverlap(rect_min, collider.size_, player_center, player_radius)) {
            continue;
        }

        dispatcher_.trigger(game::defs::AddItemRequest{player, pickup.item_id_, pickup.count_});
        dispatcher_.enqueue(engine::utils::PlaySoundEvent{entt::null, "pop"_hs});
        registry_.emplace_or_replace<engine::component::NeedRemoveTag>(entity);
    }
}

} // namespace game::system

