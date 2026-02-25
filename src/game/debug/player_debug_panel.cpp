#include "player_debug_panel.h"
#include "game/battle/battle_types.h"
#include "game/component/actor_component.h"
#include "game/component/appearance_component.h"
#include "game/component/tags.h"
#include "game/data/appearance_catalog.h"
#include "game/defs/commands.h"
#include "game/defs/constants.h"
#include "game/defs/crop_defs.h"
#include "game/defs/events.h"
#include "engine/component/transform_component.h"
#include "engine/component/velocity_component.h"
#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <imgui.h>
#include <glm/geometric.hpp>
#include <array>
#include <string>
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

struct AppearanceSlotDebugEntry {
    std::string_view slot{};
    std::string_view label{};
};

constexpr std::array<AppearanceSlotDebugEntry, 5> kDebugSwitchSlots{{
    {"skin", "Skin"},
    {"eyes", "Eyes"},
    {"clothes", "Clothes"},
    {"hair", "Hair"},
    {"acc", "Accessory"},
}};

[[nodiscard]] game::defs::EnterBattleCommand buildDebugBattleCommand() {
    game::defs::EnterBattleCommand command{};
    command.player_units = {
        game::battle::BattleUnit{1, "Hero", game::battle::BattleSide::Player, 120, 120, 24, 18},
        game::battle::BattleUnit{2, "Partner", game::battle::BattleSide::Player, 100, 100, 18, 13}
    };
    command.enemy_units = {
        game::battle::BattleUnit{101, "Slime A", game::battle::BattleSide::Enemy, 88, 88, 12, 11},
        game::battle::BattleUnit{102, "Slime B", game::battle::BattleSide::Enemy, 76, 76, 14, 9}
    };
    return command;
}

} // namespace

namespace game::debug {

PlayerDebugPanel::PlayerDebugPanel(entt::registry& registry,
                                   entt::dispatcher& dispatcher,
                                   const game::data::AppearanceCatalog* appearance_catalog)
    : registry_(registry),
      dispatcher_(dispatcher),
      appearance_catalog_(appearance_catalog) {
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

    ImGui::Separator();
    if (ImGui::Button("Start Test Battle (2v2)")) {
        dispatcher_.trigger(buildDebugBattleCommand());
    }
    ImGui::SameLine();
    if (ImGui::Button("Play VFX Laser01")) {
        glm::vec2 world_position{0.0f, 0.0f};
        if (registry_.all_of<engine::component::TransformComponent>(player_entity)) {
            world_position = registry_.get<engine::component::TransformComponent>(player_entity).position_;
        }

        game::defs::PlayVfxCommand command{};
        command.effect_id = entt::hashed_string{"laser01"}.value();
        command.world_position = world_position;
        dispatcher_.trigger(command);
    }

    if (appearance_catalog_ && registry_.all_of<game::component::AppearanceComponent>(player_entity)) {
        auto& appearance = registry_.get<game::component::AppearanceComponent>(player_entity);
        ImGui::Separator();
        ImGui::Text("Appearance");

        bool has_switchable_slot = false;
        for (const auto& entry : kDebugSwitchSlots) {
            const auto& variants = appearance_catalog_->variantsForSlot(entry.slot);
            if (variants.empty()) {
                continue;
            }

            has_switchable_slot = true;
            std::string current_variant = "none";
            if (const auto it = appearance.slot_variants_.find(std::string(entry.slot));
                it != appearance.slot_variants_.end()) {
                current_variant = it->second;
            }

            std::size_t index = 0;
            for (std::size_t i = 0; i < variants.size(); ++i) {
                if (variants[i] == current_variant) {
                    index = i;
                    break;
                }
            }

            ImGui::Text("%s: %s", entry.label.data(), variants[index].c_str());
            const std::string prev_label =
                std::string(entry.label) + " Prev##" + std::string(entry.slot);
            if (ImGui::Button(prev_label.c_str())) {
                const std::size_t next = (index == 0) ? (variants.size() - 1) : (index - 1);
                dispatcher_.trigger(game::defs::SetAppearanceSlotCommand{
                    player_entity, std::string(entry.slot), variants[next]});
            }
            ImGui::SameLine();
            const std::string next_label =
                std::string(entry.label) + " Next##" + std::string(entry.slot);
            if (ImGui::Button(next_label.c_str())) {
                const std::size_t next = (index + 1) % variants.size();
                dispatcher_.trigger(game::defs::SetAppearanceSlotCommand{
                    player_entity, std::string(entry.slot), variants[next]});
            }
        }

        if (!has_switchable_slot) {
            ImGui::Text("No appearance variants configured");
        }

        if (ImGui::Button("Reset To Profile Default")) {
            const game::data::AppearanceProfile* profile = nullptr;
            if (!appearance.profile_id_.empty()) {
                profile = appearance_catalog_->findProfile(appearance.profile_id_);
            }
            if (!profile) {
                profile = appearance_catalog_->defaultProfile();
            }
            if (profile) {
                bool has_change = false;
                for (const auto& [slot, variant] : profile->slots_) {
                    if (!appearance_catalog_->isRuntimeSwitchableSlot(slot)) {
                        continue;
                    }
                    auto it = appearance.slot_variants_.find(slot);
                    if (it != appearance.slot_variants_.end() && it->second == variant) {
                        continue;
                    }
                    appearance.slot_variants_[slot] = variant;
                    has_change = true;
                }
                if (has_change) {
                    appearance.dirty_ = true;
                }
                dispatcher_.trigger(game::defs::RefreshAppearanceCommand{player_entity});
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Refresh Appearance")) {
            dispatcher_.trigger(game::defs::RefreshAppearanceCommand{player_entity});
        }
    }

    ImGui::End();
}

} // namespace game::debug
