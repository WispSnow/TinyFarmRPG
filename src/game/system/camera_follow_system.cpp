#include "camera_follow_system.h"
#include "game/component/tags.h"
#include "engine/component/transform_component.h"
#include "engine/render/camera.h"
#include "engine/input/input_manager.h"
#include <entt/core/hashed_string.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>
#include <cmath>

namespace game::system {

using namespace entt::literals;

namespace {

constexpr float DEFAULT_CAMERA_ZOOM = 2.0f;
constexpr float CAMERA_ZOOM_STEP_PER_WHEEL_TICK = 0.15f;
constexpr entt::id_type ACTION_CAMERA_RESET_ZOOM = "camera_reset_zoom"_hs;

} // namespace

CameraFollowSystem::CameraFollowSystem(entt::registry& registry, engine::render::Camera& camera, engine::input::InputManager& input_manager)
    : registry_(registry), camera_(camera), input_manager_(input_manager) {}

void CameraFollowSystem::update(float delta_time) {
    // 更新鼠标滚轮缩放
    updateMouseWheel(delta_time);
    
    // 查找玩家实体（需要 TransformComponent 和 PlayerTag）
    auto view = registry_.view<engine::component::TransformComponent, game::component::PlayerTag>();
    // 这里的view应该只有一个玩家实体，若非如此则不更新相机位置
    if (view.size_hint() != 1) {
        return;
    }
    
    // 获取第一个玩家实体（通常只有一个玩家）
    auto player_entity = *view.begin();
    const auto& player_transform = view.get<engine::component::TransformComponent>(player_entity);

    // 获取相机当前位置和目标
    const auto current_camera_pos = camera_.getPosition();
    const auto target_pos = player_transform.position_;

    const float distance_to_target = glm::distance(current_camera_pos, target_pos);

    // 处理“接近目标”的最后阶段：进入阈值后不再移动，避免最后一帧 snap
    if (distance_to_target <= stop_distance_threshold_) {
        return;
    }
    
    // 计算平滑跟随：使用线性插值（Lerp）
    // 使用 delta_time 和 follow_speed_ 来计算插值因子，确保帧率无关
    const float lerp_factor = 1.0f - std::exp(-follow_speed_ * delta_time);
    // 使用 glm::mix 进行线性插值
    auto new_camera_pos = glm::mix(current_camera_pos, target_pos, lerp_factor);
    // 更新相机位置（setPosition 会自动处理边界限制）
    camera_.setPosition(new_camera_pos);
}

void CameraFollowSystem::updateMouseWheel(float /* delta_time */) {

    if (input_manager_.isActionPressed(ACTION_CAMERA_RESET_ZOOM)) {
        camera_.setZoom(DEFAULT_CAMERA_ZOOM);
    }

    auto mouse_wheel_delta = input_manager_.getMouseWheelDelta();
    if (mouse_wheel_delta.y != 0.0f) {
        camera_.zoom(mouse_wheel_delta.y * CAMERA_ZOOM_STEP_PER_WHEEL_TICK);
    }
}

} // namespace game::system
