#include "player_debug_panel.h"
#include "game/component/actor_component.h"
#include "game/component/tags.h"
#include "game/defs/constants.h"
#include "game/defs/crop_defs.h"
#include "game/defs/events.h"
#include "engine/component/velocity_component.h"
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <imgui.h>
#include <glm/geometric.hpp>
#include <string_view>

namespace {

[[nodiscard]] constexpr std::string_view toolToString(game::defs::Tool tool) {
    switch (tool) {
        case game::defs::Tool::Hoe: return "Hoe";
        case game::defs::Tool::WateringCan: return "Watering Can";
        case game::defs::Tool::Pickaxe: return "Pickaxe";
        case game::defs::Tool::Axe: return "Axe";
        case game::defs::Tool::Sickle: return "Sickle";
        case game::defs::Tool::None: return "None";
        default: return "Unknown";
    }
}

[[nodiscard]] constexpr std::string_view seedToString(game::defs::CropType seed) {
    switch (seed) {
        case game::defs::CropType::Strawberry: return "Strawberry";
        case game::defs::CropType::Potato: return "Potato";
        case game::defs::CropType::Unknown: return "None";
        default: return "Unknown";
    }
}

} // namespace

namespace game::debug {

PlayerDebugPanel::PlayerDebugPanel(entt::registry& registry, entt::dispatcher& dispatcher)
    : registry_(registry), dispatcher_(dispatcher) {
}

std::string_view PlayerDebugPanel::name() const {
    return "Player";
}

void PlayerDebugPanel::draw(bool& is_open) {
    if (!is_open) {
        return;
    }

    if (!ImGui::Begin("Player Debug", &is_open, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    // 查找玩家实体
    auto view = registry_.view<game::component::ActorComponent, game::component::PlayerTag>();
    if (view.begin() == view.end()) {
        ImGui::Text("未找到玩家实体");
        ImGui::End();
        return;
    }

    // 获取第一个玩家实体（通常只有一个）
    auto player_entity = *view.begin();
    auto& actor = view.get<game::component::ActorComponent>(player_entity);

    // 显示当前速度
    ImGui::Text("当前速度: %.2f", actor.speed_);
    ImGui::Separator();

    // 显示当前工具
    const auto tool_str = toolToString(actor.tool_);
    ImGui::Text("当前工具: %.*s", static_cast<int>(tool_str.size()), tool_str.data());
    ImGui::Separator();
    if (ImGui::Button("None")) {
        dispatcher_.trigger(game::defs::SwitchToolEvent{game::defs::Tool::None});
    }
    ImGui::SameLine();
    if (ImGui::Button("Hoe")) {
        dispatcher_.trigger(game::defs::SwitchToolEvent{game::defs::Tool::Hoe});
    }
    ImGui::SameLine();
    if (ImGui::Button("Watering Can")) {
        dispatcher_.trigger(game::defs::SwitchToolEvent{game::defs::Tool::WateringCan});
    }
    ImGui::SameLine();
    if (ImGui::Button("Pickaxe")) {
        dispatcher_.trigger(game::defs::SwitchToolEvent{game::defs::Tool::Pickaxe});
    }
    ImGui::SameLine();
    if (ImGui::Button("Axe")) {
        dispatcher_.trigger(game::defs::SwitchToolEvent{game::defs::Tool::Axe});
    }
    ImGui::SameLine();
    if (ImGui::Button("Sickle")) {
        dispatcher_.trigger(game::defs::SwitchToolEvent{game::defs::Tool::Sickle});
    }
    ImGui::Separator();
    
    // 显示当前种子
    const auto seed_str = seedToString(actor.hold_seed_);
    ImGui::Text("当前种子: %.*s", static_cast<int>(seed_str.size()), seed_str.data());
    ImGui::Separator();
    if (ImGui::Button("No Seed")) {
        dispatcher_.trigger(game::defs::SwitchSeedEvent{game::defs::CropType::Unknown});
    }
    ImGui::SameLine();
    if (ImGui::Button("Strawberry")) {
        dispatcher_.trigger(game::defs::SwitchSeedEvent{game::defs::CropType::Strawberry});
    }
    ImGui::SameLine();
    if (ImGui::Button("Potato")) {
        dispatcher_.trigger(game::defs::SwitchSeedEvent{game::defs::CropType::Potato});
    }
    ImGui::Separator();

    // 速度调节滑块

    ImGui::SliderFloat("Speed", &actor.speed_, 0.0f, 500.0f, "%.1f");

    // 显示当前速度向量（如果有VelocityComponent）
    if (registry_.all_of<engine::component::VelocityComponent>(player_entity)) {
        const auto& velocity = registry_.get<engine::component::VelocityComponent>(player_entity);
        ImGui::Separator();
        ImGui::Text("速度向量: (%.2f, %.2f)", velocity.velocity_.x, velocity.velocity_.y);
        const float velocity_magnitude = glm::length(velocity.velocity_);
        ImGui::Text("速度大小: %.2f", velocity_magnitude);
    }

    ImGui::End();
}

} // namespace game::debug

