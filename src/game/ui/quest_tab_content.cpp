#include "game/ui/quest_tab_content.h"

#include "game/component/quest_log_component.h"
#include "game/data/quest_catalog.h"
#include "game/domain/quest_log_ops.h"
#include "game/runtime/service_lookup.h"
#include "game/ui/localized_text.h"

#include <RmlUi/Core/DataTypeRegister.h>
#include <entt/entity/registry.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <utility>

namespace game::ui {
namespace {

using QuestEntryViewModels = std::vector<QuestEntryViewModel>;

[[nodiscard]] std::string humanizeEnemyId(std::string_view enemy_id) {
    const std::size_t separator = enemy_id.find_last_of(".:/");
    std::string label = std::string(separator == std::string_view::npos ? enemy_id : enemy_id.substr(separator + 1));
    std::replace(label.begin(), label.end(), '_', ' ');
    std::replace(label.begin(), label.end(), '-', ' ');

    bool capitalize = true;
    for (char& ch : label) {
        const unsigned char uch = static_cast<unsigned char>(ch);
        if (std::isspace(uch)) {
            capitalize = true;
            continue;
        }
        if (capitalize && std::isalpha(uch)) {
            ch = static_cast<char>(std::toupper(uch));
        }
        capitalize = false;
    }

    return label;
}

[[nodiscard]] std::string localizedEnemyName(const game::runtime::LocalizationService* localization,
                                             const std::string_view enemy_id) {
    const std::string enemy_name_key = std::string{enemy_id} + ".name";
    if (localization && localization->hasText(enemy_name_key)) {
        return localization->tr(enemy_name_key);
    }
    return humanizeEnemyId(enemy_id);
}

void appendSummaryLine(std::string& summary, const std::string& line) {
    if (line.empty()) {
        return;
    }
    if (!summary.empty()) {
        summary.append("\n");
    }
    summary.append(line);
}

[[nodiscard]] std::string buildProgressSummary(const game::component::QuestLogComponent& quest_log,
                                               const game::data::QuestData& quest,
                                               const game::runtime::LocalizationService* localization) {
    std::string summary{};

    for (const auto& objective : quest.objectives_) {
        const std::string progress_key = game::data::makeQuestObjectiveProgressKey(quest.id_, objective.id_);
        const auto progress_it = quest_log.objective_progress.find(progress_key);
        const int current = progress_it == quest_log.objective_progress.end() ? 0 : progress_it->second;
        const int clamped = std::clamp(current, 0, objective.required_count_);
        appendSummaryLine(
            summary,
            game::ui::formatTextOrFallback(
                localization,
                "inventory.quest.progress.defeat_enemy_count",
                {
                    {"enemy", localizedEnemyName(localization, objective.enemy_id_)},
                    {"current", std::to_string(clamped)},
                    {"count", std::to_string(objective.required_count_)},
                },
                [&objective, clamped] {
                    return humanizeEnemyId(objective.enemy_id_) + " " + std::to_string(clamped) + "/" +
                           std::to_string(objective.required_count_);
                }));
    }

    return summary;
}

[[nodiscard]] QuestEntryViewModel makeActiveQuestEntry(const game::component::QuestLogComponent& quest_log,
                                                       const game::data::QuestData& quest,
                                                       const game::runtime::LocalizationService* localization) {
    const std::string progress_summary = buildProgressSummary(quest_log, quest, localization);

    QuestEntryViewModel entry{};
    entry.title = quest.title_.empty() ? quest.id_ : game::ui::tryLocalize(localization, quest.title_);
    entry.description = game::ui::tryLocalize(localization, quest.description_);
    entry.progress_summary = progress_summary;
    entry.status_label = game::domain::quest_log_ops::isQuestReadyToTurnIn(quest_log, quest)
                             ? game::ui::localizeTextOrFallback(localization, "inventory.quest.status.ready", "Ready")
                             : game::ui::localizeTextOrFallback(localization, "inventory.quest.status.in_progress", "In Progress");
    entry.has_description = !entry.description.empty();
    entry.has_progress_summary = !entry.progress_summary.empty();
    return entry;
}

[[nodiscard]] QuestEntryViewModel makeCompletedQuestEntry(const game::data::QuestData& quest,
                                                          const game::runtime::LocalizationService* localization) {
    QuestEntryViewModel entry{};
    entry.title = quest.title_.empty() ? quest.id_ : game::ui::tryLocalize(localization, quest.title_);
    entry.status_label = game::ui::localizeTextOrFallback(localization, "inventory.quest.status.completed", "Completed");
    return entry;
}

void appendActiveQuestEntries(const game::component::QuestLogComponent& quest_log,
                              const game::data::QuestCatalog* quest_catalog,
                              std::vector<QuestEntryViewModel>& out_entries,
                              const game::runtime::LocalizationService* localization) {
    for (const std::string& quest_id : quest_log.active_quests) {
        const auto* quest = quest_catalog ? quest_catalog->findQuest(quest_id) : nullptr;
        if (!quest) {
            spdlog::warn("QuestTabContent: active quest '{}' 未在 QuestCatalog 中找到，任务页跳过该条目。", quest_id);
            continue;
        }
        out_entries.push_back(makeActiveQuestEntry(quest_log, *quest, localization));
    }
}

void appendCompletedQuestEntries(const game::component::QuestLogComponent& quest_log,
                                 const game::data::QuestCatalog* quest_catalog,
                                 std::vector<QuestEntryViewModel>& out_entries,
                                 const game::runtime::LocalizationService* localization) {
    for (const std::string& quest_id : quest_log.completed_quests) {
        const auto* quest = quest_catalog ? quest_catalog->findQuest(quest_id) : nullptr;
        if (!quest) {
            spdlog::warn("QuestTabContent: completed quest '{}' 未在 QuestCatalog 中找到，任务页跳过该条目。", quest_id);
            continue;
        }
        out_entries.push_back(makeCompletedQuestEntry(*quest, localization));
    }
}

} // namespace

namespace {

[[nodiscard]] bool registerQuestEntryViewModelType(Rml::DataModelConstructor& constructor) {
    if (auto handle = constructor.RegisterStruct<QuestEntryViewModel>()) {
        handle.RegisterMember("title", &QuestEntryViewModel::title);
        handle.RegisterMember("description", &QuestEntryViewModel::description);
        handle.RegisterMember("progress_summary", &QuestEntryViewModel::progress_summary);
        handle.RegisterMember("status_label", &QuestEntryViewModel::status_label);
        handle.RegisterMember("has_description", &QuestEntryViewModel::has_description);
        handle.RegisterMember("has_progress_summary", &QuestEntryViewModel::has_progress_summary);
        return true;
    }

    return false;
}

} // namespace

bool registerQuestTabDataTypes(Rml::DataModelConstructor& constructor) {
    if (!registerQuestEntryViewModelType(constructor)) {
        return false;
    }
    if (!constructor.RegisterArray<QuestEntryViewModels>()) {
        return false;
    }

    return true;
}

QuestTabViewState buildQuestTabViewState(const entt::registry& registry,
                                         const entt::entity player,
                                         const game::data::QuestCatalog* quest_catalog,
                                         const game::runtime::LocalizationService* localization) {
    QuestTabViewState state{};
    if (player == entt::null || !registry.valid(player) || quest_catalog == nullptr) {
        return state;
    }

    const auto* quest_log = registry.try_get<game::component::QuestLogComponent>(player);
    if (!quest_log) {
        return state;
    }

    appendActiveQuestEntries(*quest_log, quest_catalog, state.active_quest_entries, localization);
    appendCompletedQuestEntries(*quest_log, quest_catalog, state.completed_quest_entries, localization);

    state.has_active_quests = !state.active_quest_entries.empty();
    state.has_completed_quests = !state.completed_quest_entries.empty();
    return state;
}

QuestTabContent::QuestTabContent(engine::ui::rmlui::RmlDocumentController& document_controller,
                                 entt::registry& game_registry,
                                 const entt::entity player,
                                 const game::data::QuestCatalog* quest_catalog)
    : document_controller_(document_controller),
      game_registry_(game_registry),
      player_(player),
      quest_catalog_(quest_catalog),
      localization_(game::runtime::findLocalizationService(game_registry)) {
}

bool QuestTabContent::bindModel(Rml::DataModelConstructor& constructor) {
    if (!constructor.Bind("active_quest_entries", &active_quest_entries_) ||
        !constructor.Bind("completed_quest_entries", &completed_quest_entries_) ||
        !constructor.Bind("has_active_quests", &has_active_quests_) ||
        !constructor.Bind("has_completed_quests", &has_completed_quests_)) {
        spdlog::error("QuestTabContent: 绑定 quest 页 data model 失败。");
        return false;
    }

    return true;
}

void QuestTabContent::onActivated() {
    syncViewState();
}

void QuestTabContent::onDeactivated() {
}

void QuestTabContent::update(const float /*delta_time*/) {
}

bool QuestTabContent::onCancel() {
    return false;
}

void QuestTabContent::onLanguageChanged() {
    localization_ = game::runtime::findLocalizationService(game_registry_);
    syncViewState();
}

void QuestTabContent::syncViewState() {
    QuestTabViewState state = buildQuestTabViewState(game_registry_, player_, quest_catalog_, localization_);
    active_quest_entries_ = std::move(state.active_quest_entries);
    completed_quest_entries_ = std::move(state.completed_quest_entries);
    has_active_quests_ = state.has_active_quests;
    has_completed_quests_ = state.has_completed_quests;

    document_controller_.markDirty("active_quest_entries");
    document_controller_.markDirty("completed_quest_entries");
    document_controller_.markDirty("has_active_quests");
    document_controller_.markDirty("has_completed_quests");
}

} // namespace game::ui
