#include "player_debug_panel.h"
#include "game/component/actor_component.h"
#include "game/component/appearance_component.h"
#include "game/component/party_component.h"
#include "game/component/party_runtime_stats_component.h"
#include "game/component/tags.h"
#include "game/data/appearance_catalog.h"
#include "game/data/rpg_catalog.h"
#include "game/defs/commands.h"
#include "game/defs/constants.h"
#include "game/defs/crop_defs.h"
#include "game/defs/events.h"
#include "game/domain/actor_progression_service.h"
#include "engine/component/velocity_component.h"
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <imgui.h>
#include <glm/geometric.hpp>
#include <algorithm>
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

[[nodiscard]] bool runtimeStateEquals(const game::component::ActorRuntimeState& lhs,
                                      const game::component::ActorRuntimeState& rhs) {
    return lhs.current_hp == rhs.current_hp &&
           lhs.current_mp == rhs.current_mp &&
           lhs.level == rhs.level &&
           lhs.total_exp == rhs.total_exp;
}

void triggerPartyRuntimeSync(entt::dispatcher& dispatcher, const entt::entity player) {
    dispatcher.trigger(game::defs::PartyRuntimeStatsChanged{
        .player = player,
        .actor_id = {},
        .full_sync = true,
    });
}

void commitRuntimeState(entt::dispatcher& dispatcher,
                        game::component::PartyRuntimeStatsComponent& runtime_stats,
                        const entt::entity player,
                        const std::string& actor_id,
                        const game::component::ActorRuntimeState& state) {
    auto& stored = runtime_stats.states_by_actor_id_[actor_id];
    if (runtimeStateEquals(stored, state)) {
        return;
    }

    stored = state;
    ++runtime_stats.revision_;
    triggerPartyRuntimeSync(dispatcher, player);
}

void drawActorProgressionDebug(entt::registry& registry,
                               entt::dispatcher& dispatcher,
                               const entt::entity player,
                               const game::data::RpgCatalog& rpg_catalog) {
    const auto* party = registry.try_get<game::component::PartyComponent>(player);
    if (!party) {
        return;
    }

    ImGui::Separator();
    ImGui::Text("Progression");

    auto& runtime_stats = registry.get_or_emplace<game::component::PartyRuntimeStatsComponent>(player);
    for (const auto& actor_id : party->recruited_actor_ids_) {
        const auto* actor = rpg_catalog.findActor(actor_id);
        if (!actor) {
            continue;
        }

        const auto state_it = runtime_stats.states_by_actor_id_.find(actor_id);
        game::component::ActorRuntimeState state = state_it == runtime_stats.states_by_actor_id_.end()
            ? game::domain::ActorProgressionService::initialState(rpg_catalog, *actor, nullptr)
            : game::domain::ActorProgressionService::normalizeState(rpg_catalog, *actor, state_it->second, nullptr);

        const int exp_to_next =
            game::domain::ActorProgressionService::expToNextLevel(rpg_catalog, *actor, state.total_exp);
        const std::string label = actor->display_name_.empty() ? actor_id : actor->display_name_;
        ImGui::Text(
            "%s: Lv.%d  EXP %d  Next %d",
            label.c_str(),
            state.level,
            state.total_exp,
            exp_to_next);

        const int previous_level = std::max(actor->initial_level_, state.level - 1);
        const int next_level = std::min(actor->max_level_, state.level + 1);
        const std::string previous_label = "- Lv##" + actor_id;
        if (ImGui::Button(previous_label.c_str())) {
            state.total_exp =
                game::domain::ActorProgressionService::expForLevel(rpg_catalog, *actor, previous_level);
            state.level = previous_level;
            state = game::domain::ActorProgressionService::normalizeState(rpg_catalog, *actor, state, nullptr);
            commitRuntimeState(dispatcher, runtime_stats, player, actor_id, state);
        }
        ImGui::SameLine();
        const std::string next_label = "+ Lv##" + actor_id;
        if (ImGui::Button(next_label.c_str()) && next_level > state.level) {
            const int exp_needed =
                game::domain::ActorProgressionService::expForLevel(rpg_catalog, *actor, next_level) -
                state.total_exp;
            const auto result = game::domain::ActorProgressionService::grantExperience(
                registry,
                player,
                rpg_catalog,
                {actor_id},
                std::max(0, exp_needed));
            if (result.runtime_state_changed) {
                triggerPartyRuntimeSync(dispatcher, player);
            }
        }
        ImGui::SameLine();
        const std::string exp_50_label = "+50 EXP##" + actor_id;
        if (ImGui::Button(exp_50_label.c_str())) {
            const auto result = game::domain::ActorProgressionService::grantExperience(
                registry,
                player,
                rpg_catalog,
                {actor_id},
                50);
            if (result.runtime_state_changed) {
                triggerPartyRuntimeSync(dispatcher, player);
            }
        }
        ImGui::SameLine();
        const std::string exp_500_label = "+500 EXP##" + actor_id;
        if (ImGui::Button(exp_500_label.c_str())) {
            const auto result = game::domain::ActorProgressionService::grantExperience(
                registry,
                player,
                rpg_catalog,
                {actor_id},
                500);
            if (result.runtime_state_changed) {
                triggerPartyRuntimeSync(dispatcher, player);
            }
        }
        ImGui::SameLine();
        const std::string max_label = "Max##" + actor_id;
        if (ImGui::Button(max_label.c_str())) {
            state.total_exp =
                game::domain::ActorProgressionService::expForLevel(rpg_catalog, *actor, actor->max_level_);
            state.level = actor->max_level_;
            state = game::domain::ActorProgressionService::normalizeState(rpg_catalog, *actor, state, nullptr);
            commitRuntimeState(dispatcher, runtime_stats, player, actor_id, state);
        }
        ImGui::SameLine();
        const std::string reset_label = "Reset##" + actor_id;
        if (ImGui::Button(reset_label.c_str())) {
            commitRuntimeState(
                dispatcher,
                runtime_stats,
                player,
                actor_id,
                game::domain::ActorProgressionService::initialState(rpg_catalog, *actor, nullptr));
        }
    }
}

} // namespace

namespace game::debug {

PlayerDebugPanel::PlayerDebugPanel(entt::registry& registry,
                                   entt::dispatcher& dispatcher,
                                   const game::data::AppearanceCatalog* appearance_catalog,
                                   const game::data::RpgCatalog* rpg_catalog)
    : registry_(registry),
      dispatcher_(dispatcher),
      appearance_catalog_(appearance_catalog),
      rpg_catalog_(rpg_catalog) {
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

    if (rpg_catalog_) {
        drawActorProgressionDebug(registry_, dispatcher_, player_entity, *rpg_catalog_);
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
