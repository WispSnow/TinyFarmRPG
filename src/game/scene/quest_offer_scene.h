#pragma once

#include "engine/scene/scene.h"
#include "engine/ui/rmlui/rml_document_controller.h"

#include <RmlUi/Core/Types.h>
#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>

#include <string>
#include <string_view>

namespace engine::core {
enum class State;
}

namespace game::data {
class ItemCatalog;
struct QuestData;
}

namespace game::defs {
struct LanguageChangedEvent;
}

namespace game::runtime {
class LocalizationService;
}

namespace game::scene {

class QuestOfferScene final : public engine::scene::Scene {
private:
    entt::registry& registry_;
    entt::entity player_{entt::null};
    entt::entity giver_{entt::null};
    const game::data::QuestData& quest_;
    const game::data::ItemCatalog* item_catalog_{nullptr};
    const game::runtime::LocalizationService* localization_{nullptr};

    engine::core::State previous_state_{};
    bool context_pushed_{false};

    engine::ui::rmlui::RmlDocumentController document_controller_{};

    Rml::String speaker_text_{};
    Rml::String offer_text_{};
    Rml::String quest_title_{};
    Rml::String quest_description_{};
    Rml::String objectives_text_{};
    Rml::String rewards_text_{};

public:
    QuestOfferScene(std::string_view name,
                    engine::core::Context& context,
                    entt::registry& registry,
                    entt::entity player,
                    entt::entity giver,
                    const game::data::QuestData& quest,
                    const game::data::ItemCatalog* item_catalog);
    ~QuestOfferScene() override;

    bool init() override;
    void clean() override;
    [[nodiscard]] engine::scene::SceneUiCoverage uiCoverage() const override;

private:
    [[nodiscard]] bool initUI();
    void shutdownUI();
    void connectRuntimeListeners();
    void disconnectRuntimeListeners();
    void refreshBindings();
    void focusDefaultAction();
    bool onMenuCancelPressed();
    void onLanguageChanged(const game::defs::LanguageChangedEvent& event);
    void onAccept();
    void onDecline();
};

} // namespace game::scene
