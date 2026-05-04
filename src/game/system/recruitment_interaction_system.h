#pragma once

#include "game/component/npc_component.h"
#include "game/defs/commands.h"
#include "game/system/system_helpers.h"

#include <glm/vec2.hpp>
#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <entt/signal/fwd.hpp>

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace game::data {
class RpgCatalog;
}

namespace game::system {

class RecruitmentInteractionSystem final {
    entt::registry& registry_;
    entt::dispatcher& dispatcher_;
    const game::data::RpgCatalog& rpg_catalog_;

    std::unordered_map<entt::id_type, std::vector<std::string>> dialogue_table_{};
    entt::entity active_entity_{entt::null};
    helpers::NotificationTimer notification_{};

public:
    RecruitmentInteractionSystem(entt::registry& registry,
                                 entt::dispatcher& dispatcher,
                                 const game::data::RpgCatalog& rpg_catalog);
    ~RecruitmentInteractionSystem();

    [[nodiscard]] bool loadDialogueFile(std::string_view file_path);
    void update(float delta_time);

private:
    void onInteractCommand(const game::defs::InteractCommand& event);
    [[nodiscard]] bool isRecruited(std::string_view actor_id) const;
    void showNotification(entt::entity target, std::string text);
    void startDialogue(entt::entity entity, game::component::DialogueComponent& dialogue,
                       const std::vector<std::string>& lines, glm::vec2 head_pos);
    [[nodiscard]] bool advanceDialogue(entt::entity entity, game::component::DialogueComponent& dialogue,
                                        const std::vector<std::string>& lines, glm::vec2 head_pos);
    void endDialogueAndRequestJoin(entt::entity player, entt::entity entity, game::component::DialogueComponent& dialogue);
    void closeDialogue(entt::entity entity);
    void showLine(entt::entity entity, const std::vector<std::string>& lines, std::size_t line_index, glm::vec2 head_pos);
};

} // namespace game::system
