#include "quest_debug_panel.h"

#include "game/component/quest_log_component.h"
#include "game/component/inventory_component.h"
#include "game/component/player_wallet_component.h"
#include "game/component/tags.h"
#include "game/data/quest_catalog.h"
#include "game/data/quest_data.h"
#include "game/data/item_catalog.h"
#include "game/domain/quest_log_ops.h"
#include "game/domain/quest_turn_in_service.h"

#include <entt/entity/registry.hpp>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using QuestList = std::vector<const game::data::QuestData*>;

constexpr std::array<game::debug::QuestDebugFilter, 5> kQuestFilters{{
    game::debug::QuestDebugFilter::All,
    game::debug::QuestDebugFilter::Offerable,
    game::debug::QuestDebugFilter::Active,
    game::debug::QuestDebugFilter::ReadyToTurnIn,
    game::debug::QuestDebugFilter::Completed,
}};

[[nodiscard]] entt::entity findQuestPlayer(entt::registry& registry) {
    auto view = registry.view<game::component::PlayerTag, game::component::QuestLogComponent>();
    if (view.begin() == view.end()) {
        return entt::null;
    }
    return *view.begin();
}

[[nodiscard]] QuestList collectVisibleQuests(const game::data::QuestCatalog& quest_catalog,
                                             const game::component::QuestLogComponent& quest_log,
                                             const game::debug::QuestDebugFilter filter) {
    QuestList visible = quest_catalog.listQuests();
    visible.erase(
        std::remove_if(
            visible.begin(),
            visible.end(),
            [&quest_log, filter](const game::data::QuestData* quest) {
                const auto state = game::debug::detail::resolveQuestDebugState(quest_log, *quest);
                return !game::debug::detail::matchesQuestDebugFilter(state, filter);
            }),
        visible.end());

    std::sort(visible.begin(), visible.end(), [](const game::data::QuestData* lhs, const game::data::QuestData* rhs) {
        if (lhs->title_ != rhs->title_) {
            return lhs->title_ < rhs->title_;
        }
        return lhs->id_ < rhs->id_;
    });

    return visible;
}

void syncQuestSelection(std::string& selected_quest_id, int& selected_objective_index, const QuestList& visible_quests) {
    const auto found_it = std::find_if(visible_quests.begin(), visible_quests.end(), [&selected_quest_id](const auto* quest) {
        return quest != nullptr && quest->id_ == selected_quest_id;
    });

    if (found_it != visible_quests.end()) {
        return;
    }

    if (!visible_quests.empty()) {
        selected_quest_id = visible_quests.front()->id_;
        selected_objective_index = 0;
        return;
    }

    selected_quest_id.clear();
    selected_objective_index = 0;
}

[[nodiscard]] int countReadyQuests(const game::component::QuestLogComponent& quest_log,
                                   const game::data::QuestCatalog& quest_catalog) {
    int ready_count = 0;
    for (const std::string& quest_id : quest_log.active_quests) {
        const auto* quest = quest_catalog.findQuest(quest_id);
        if (quest != nullptr && game::domain::quest_log_ops::isQuestReadyToTurnIn(quest_log, *quest)) {
            ++ready_count;
        }
    }
    return ready_count;
}

[[nodiscard]] std::string rewardItemLabel(const game::data::ItemCatalog* item_catalog,
                                          const game::data::QuestRewardItemData& reward_item) {
    if (item_catalog != nullptr) {
        if (const auto* item = item_catalog->findItem(reward_item.item_id_hash_)) {
            if (!item->display_name_.empty()) {
                return item->display_name_;
            }
        }
    }
    return reward_item.item_id_;
}

[[nodiscard]] std::string buildTurnInStatus(const game::data::QuestData& quest,
                                            const game::domain::QuestTurnInResult& result) {
    if (!result.completed()) {
        if (!result.failure_message.empty()) {
            return "Turn In 失败: " + result.failure_message;
        }
        return "Turn In 失败: " + quest.title_;
    }

    std::string status = "Turn In 完成: " + quest.title_;
    bool has_reward = false;
    if (result.gold_reward > 0) {
        status += " | Gold +" + std::to_string(result.gold_reward);
        has_reward = true;
    }
    for (const auto& item_reward : result.item_rewards) {
        status += has_reward ? ", " : " | ";
        status += item_reward.item_name.empty() ? item_reward.item_id : item_reward.item_name;
        status += " x" + std::to_string(item_reward.count);
        has_reward = true;
    }
    if (!has_reward) {
        status += " | 无奖励";
    }
    return status;
}

void resetSelectedQuest(game::component::QuestLogComponent& quest_log, const std::string_view quest_id) {
    quest_log.active_quests.erase(
        std::remove(quest_log.active_quests.begin(), quest_log.active_quests.end(), quest_id),
        quest_log.active_quests.end());
    quest_log.completed_quests.erase(
        std::remove(quest_log.completed_quests.begin(), quest_log.completed_quests.end(), quest_id),
        quest_log.completed_quests.end());
    game::domain::quest_log_ops::eraseQuestProgress(quest_log, quest_id);
}

void setObjectiveProgress(game::component::QuestLogComponent& quest_log,
                          const game::data::QuestData& quest,
                          const game::data::QuestObjectiveData& objective,
                          const int next_progress) {
    quest_log.objective_progress[game::data::makeQuestObjectiveProgressKey(quest.id_, objective.id_)] =
        std::clamp(next_progress, 0, std::max(0, objective.required_count_));
}

} // namespace

namespace game::debug {

QuestDebugPanel::QuestDebugPanel(entt::registry& registry,
                                 const game::data::QuestCatalog* quest_catalog,
                                 const game::data::ItemCatalog* item_catalog,
                                 game::domain::QuestTurnInService* quest_turn_in_service)
    : registry_(registry),
      quest_catalog_(quest_catalog),
      item_catalog_(item_catalog),
      quest_turn_in_service_(quest_turn_in_service) {
}

std::string_view QuestDebugPanel::name() const {
    return "Quests";
}

void QuestDebugPanel::draw(bool& is_open) {
    if (!is_open) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(980.0f, 640.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Quest Debug", &is_open)) {
        ImGui::End();
        return;
    }

    const entt::entity player = findQuestPlayer(registry_);
    if (player == entt::null) {
        ImGui::Text("未找到带 QuestLogComponent 的玩家实体");
        ImGui::End();
        return;
    }

    if (quest_catalog_ == nullptr) {
        ImGui::Text("QuestCatalog 不可用");
        ImGui::End();
        return;
    }

    auto* quest_log = registry_.try_get<game::component::QuestLogComponent>(player);
    if (quest_log == nullptr) {
        ImGui::Text("玩家缺少 QuestLogComponent");
        ImGui::End();
        return;
    }

    progress_step_ = std::max(progress_step_, 1);

    const QuestList visible_quests = collectVisibleQuests(*quest_catalog_, *quest_log, filter_);
    syncQuestSelection(selected_quest_id_, selected_objective_index_, visible_quests);
    const QuestDebugSelectionResult selection = detail::resolveQuestDebugSelection(quest_catalog_, selected_quest_id_);
    const game::data::QuestData* selected_quest = selection.quest;

    ImGui::Text("Active: %d", static_cast<int>(quest_log->active_quests.size()));
    ImGui::SameLine();
    ImGui::Text("Completed: %d", static_cast<int>(quest_log->completed_quests.size()));
    ImGui::SameLine();
    ImGui::Text("Ready: %d", countReadyQuests(*quest_log, *quest_catalog_));

    int filter_index = static_cast<int>(filter_);
    if (ImGui::BeginCombo("Filter", game::debug::questDebugFilterLabel(filter_).data())) {
        for (int i = 0; i < static_cast<int>(kQuestFilters.size()); ++i) {
            const bool is_selected = (i == filter_index);
            if (ImGui::Selectable(game::debug::questDebugFilterLabel(kQuestFilters[static_cast<std::size_t>(i)]).data(),
                                  is_selected)) {
                filter_ = kQuestFilters[static_cast<std::size_t>(i)];
                selected_objective_index_ = 0;
            }
            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::Separator();

    ImGui::BeginChild("QuestList", ImVec2(320.0f, 0.0f), true);
    if (visible_quests.empty()) {
        ImGui::TextWrapped("当前筛选下没有可见任务。");
    } else {
        for (const auto* quest : visible_quests) {
            const auto state = detail::resolveQuestDebugState(*quest_log, *quest);
            std::string label = quest->title_ + " [" + std::string(game::debug::questDebugStateLabel(state)) + "]##" + quest->id_;
            if (ImGui::Selectable(label.c_str(), selected_quest_id_ == quest->id_)) {
                selected_quest_id_ = quest->id_;
                selected_objective_index_ = 0;
            }
            ImGui::TextDisabled("%s", quest->id_.c_str());
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("QuestDetail", ImVec2(0.0f, 0.0f), true);
    if (selected_quest == nullptr) {
        ImGui::TextWrapped("%s", selection.error.empty() ? "未选择任务" : selection.error.c_str());
        if (!status_.empty()) {
            ImGui::Separator();
            ImGui::TextWrapped("%s", status_.c_str());
        }
        ImGui::EndChild();
        ImGui::End();
        return;
    }

    const auto quest_state = detail::resolveQuestDebugState(*quest_log, *selected_quest);
    selected_objective_index_ = std::clamp(
        selected_objective_index_,
        0,
        std::max(0, static_cast<int>(selected_quest->objectives_.size()) - 1));

    ImGui::Text("%s", selected_quest->title_.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("[%s]", game::debug::questDebugStateLabel(quest_state).data());
    ImGui::TextDisabled("%s", selected_quest->id_.c_str());
    if (!selected_quest->description_.empty()) {
        ImGui::TextWrapped("%s", selected_quest->description_.c_str());
    }

    if (registry_.all_of<game::component::PlayerWalletComponent>(player)) {
        ImGui::Text("Gold: %d", registry_.get<game::component::PlayerWalletComponent>(player).gold_);
    }
    if (registry_.all_of<game::component::InventoryComponent>(player)) {
        ImGui::Text("Inventory Slots: %d", registry_.get<game::component::InventoryComponent>(player).slotCount());
    }

    if (ImGui::Button("Accept")) {
        if (quest_state == QuestDebugState::Offerable) {
            status_ = game::domain::quest_log_ops::tryAcceptQuest(*quest_log, *selected_quest)
                ? "Accept 成功: " + selected_quest->title_
                : "Accept 失败: " + selected_quest->title_;
        } else {
            status_ = "Accept 跳过: 当前状态为 " + std::string(game::debug::questDebugStateLabel(quest_state));
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Selected")) {
        resetSelectedQuest(*quest_log, selected_quest->id_);
        status_ = "已重置任务: " + selected_quest->title_;
    }
    ImGui::SameLine();
    if (ImGui::Button("Turn In")) {
        if (quest_turn_in_service_ == nullptr) {
            status_ = "QuestTurnInService 不可用";
        } else {
            const auto result = quest_turn_in_service_->turnIn(player, *selected_quest, *quest_log);
            status_ = buildTurnInStatus(*selected_quest, result);
        }
    }

    ImGui::Separator();
    ImGui::InputInt("Progress Step", &progress_step_);
    progress_step_ = std::max(progress_step_, 1);

    if (!selected_quest->objectives_.empty()) {
        ImGui::Text("Objectives");
        for (int i = 0; i < static_cast<int>(selected_quest->objectives_.size()); ++i) {
            const auto& objective = selected_quest->objectives_[static_cast<std::size_t>(i)];
            const int current = detail::clampQuestObjectiveProgress(*quest_log, *selected_quest, objective);

            std::string label =
                objective.id_ + " (" + objective.enemy_id_ + ") " + std::to_string(current) + "/" +
                std::to_string(std::max(0, objective.required_count_)) + "##objective_" + std::to_string(i);
            if (ImGui::Selectable(label.c_str(), selected_objective_index_ == i)) {
                selected_objective_index_ = i;
            }
        }

        const auto& selected_objective =
            selected_quest->objectives_[static_cast<std::size_t>(selected_objective_index_)];
        const int current_progress = detail::clampQuestObjectiveProgress(*quest_log, *selected_quest, selected_objective);

        if (ImGui::Button("+Progress Objective")) {
            setObjectiveProgress(*quest_log, *selected_quest, selected_objective, current_progress + progress_step_);
            status_ = "已推进 objective: " + selected_objective.id_;
        }
        ImGui::SameLine();
        if (ImGui::Button("Fill Objective")) {
            setObjectiveProgress(*quest_log, *selected_quest, selected_objective, selected_objective.required_count_);
            status_ = "已填满 objective: " + selected_objective.id_;
        }
        ImGui::SameLine();
        if (ImGui::Button("Fill All Objectives")) {
            for (const auto& objective : selected_quest->objectives_) {
                setObjectiveProgress(*quest_log, *selected_quest, objective, objective.required_count_);
            }
            status_ = "已填满全部 objectives: " + selected_quest->title_;
        }
    } else {
        ImGui::TextWrapped("该任务没有 objectives。");
    }

    ImGui::Separator();
    if (ImGui::Button("Clear All Quest State")) {
        quest_log->active_quests.clear();
        quest_log->completed_quests.clear();
        quest_log->objective_progress.clear();
        status_ = "已清空全部任务状态";
    }

    ImGui::Separator();
    ImGui::Text("Rewards");
    if (selected_quest->rewards_.empty()) {
        ImGui::TextDisabled("无奖励");
    } else {
        if (selected_quest->rewards_.gold_ > 0) {
            ImGui::BulletText("Gold x%d", selected_quest->rewards_.gold_);
        }
        for (const auto& reward_item : selected_quest->rewards_.items_) {
            const std::string label = rewardItemLabel(item_catalog_, reward_item);
            ImGui::BulletText("%s x%d", label.c_str(), reward_item.count_);
        }
    }

    if (!status_.empty()) {
        ImGui::Separator();
        ImGui::TextWrapped("%s", status_.c_str());
    }

    ImGui::EndChild();
    ImGui::End();
}

} // namespace game::debug
