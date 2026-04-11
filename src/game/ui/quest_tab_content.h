#pragma once

#include "engine/ui/rmlui/rml_document_controller.h"
#include "game/ui/menu_tab_content.h"

#include <RmlUi/Core/Types.h>
#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>

#include <string>
#include <vector>

namespace Rml {
class DataModelConstructor;
}

namespace game::data {
class QuestCatalog;
}

namespace game::ui {

struct QuestEntryViewModel {
    Rml::String title{};
    Rml::String description{};
    Rml::String progress_summary{};
    Rml::String status_label{};
    bool has_description{false};
    bool has_progress_summary{false};
};

struct QuestTabViewState {
    std::vector<QuestEntryViewModel> active_quest_entries{};
    std::vector<QuestEntryViewModel> completed_quest_entries{};
    bool has_active_quests{false};
    bool has_completed_quests{false};
};

[[nodiscard]] bool registerQuestTabDataTypes(Rml::DataModelConstructor& constructor);
[[nodiscard]] QuestTabViewState buildQuestTabViewState(const entt::registry& registry,
                                                       entt::entity player,
                                                       const game::data::QuestCatalog* quest_catalog);

class QuestTabContent final : public IMenuTabContent {
public:
    QuestTabContent(engine::ui::rmlui::RmlDocumentController& document_controller,
                    entt::registry& game_registry,
                    entt::entity player,
                    const game::data::QuestCatalog* quest_catalog);
    ~QuestTabContent() override = default;

    [[nodiscard]] bool bindModel(Rml::DataModelConstructor& constructor) override;
    void onActivated() override;
    void onDeactivated() override;
    void update(float delta_time) override;
    [[nodiscard]] bool onCancel() override;

private:
    engine::ui::rmlui::RmlDocumentController& document_controller_;
    entt::registry& game_registry_;
    entt::entity player_{entt::null};
    const game::data::QuestCatalog* quest_catalog_{nullptr};

    std::vector<QuestEntryViewModel> active_quest_entries_{};
    std::vector<QuestEntryViewModel> completed_quest_entries_{};
    bool has_active_quests_{false};
    bool has_completed_quests_{false};

    void syncViewState();
};

} // namespace game::ui
