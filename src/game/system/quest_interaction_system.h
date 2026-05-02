#pragma once

#include "game/component/quest_log_component.h"
#include "game/system/system_helpers.h"

#include <cstdint>
#include <string>

#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <entt/signal/fwd.hpp>

namespace game::data {
class QuestCatalog;
struct QuestData;
}

namespace game::domain {
class QuestTurnInService;
struct QuestTurnInResult;
}

namespace game::defs {
struct AcceptQuestCommand;
struct InteractCommand;
}

namespace game::system {

class QuestInteractionSystem final {
public:
    enum class InteractionState : std::uint8_t {
        Offerable = 0,
        InProgress,
        ReadyToTurnIn,
        Completed
    };

    QuestInteractionSystem(entt::registry& registry,
                           entt::dispatcher& dispatcher,
                           const game::data::QuestCatalog& quest_catalog,
                           game::domain::QuestTurnInService& quest_turn_in_service);
    ~QuestInteractionSystem();

    void update(float delta_time);

private:
    entt::registry& registry_;
    entt::dispatcher& dispatcher_;
    const game::data::QuestCatalog& quest_catalog_;
    game::domain::QuestTurnInService& quest_turn_in_service_;
    game::system::helpers::NotificationTimer notification_{};

    void onInteractCommand(const game::defs::InteractCommand& event);
    void onAcceptQuestCommand(const game::defs::AcceptQuestCommand& command);
    [[nodiscard]] InteractionState resolveState(const game::component::QuestLogComponent& quest_log,
                                                const game::data::QuestData& quest) const;
    void showText(entt::entity giver, std::string text);
    void showQuestText(entt::entity giver,
                       const game::data::QuestData& quest,
                       InteractionState state);
};

} // namespace game::system
