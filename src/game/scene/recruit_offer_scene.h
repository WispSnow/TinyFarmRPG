#pragma once

#include "engine/scene/scene.h"
#include "engine/ui/rmlui/rml_document_controller.h"

#include <RmlUi/Core/Types.h>
#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>

#include <string_view>

namespace engine::core {
enum class State;
}

namespace game::data {
struct ActorData;
}

namespace game::defs {
struct LanguageChangedEvent;
}

namespace game::runtime {
class LocalizationService;
}

namespace game::scene {

class RecruitOfferScene final : public engine::scene::Scene {
    entt::registry& registry_;
    entt::entity player_{entt::null};
    entt::entity recruiter_{entt::null};
    const game::data::ActorData& actor_;
    const game::runtime::LocalizationService* localization_{nullptr};

    engine::core::State previous_state_{};
    bool context_pushed_{false};
    bool resolved_{false};

    engine::ui::rmlui::RmlDocumentController document_controller_{};

    Rml::String speaker_text_{};
    Rml::String offer_text_{};
    Rml::String actor_name_{};
    Rml::String portrait_decorator_{"none"};

public:
    RecruitOfferScene(std::string_view name,
                      engine::core::Context& context,
                      entt::registry& registry,
                      entt::entity player,
                      entt::entity recruiter,
                      const game::data::ActorData& actor);
    ~RecruitOfferScene() override;

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
    bool onMenuConfirmPressed();
    bool onMenuCancelPressed();
    void onLanguageChanged(const game::defs::LanguageChangedEvent& event);
    void onAccept();
    void onDecline();
};

} // namespace game::scene
