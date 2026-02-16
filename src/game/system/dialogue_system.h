#pragma once

#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <unordered_map>
#include <string>
#include <vector>
#include <glm/vec2.hpp>
#include <entt/signal/dispatcher.hpp>

namespace game::component {
struct DialogueComponent;
}

namespace game::defs {
struct InteractRequest;
}

namespace game::system {

class DialogueSystem {
    entt::registry& registry_;
    entt::dispatcher& dispatcher_;
    std::unordered_map<entt::id_type, std::vector<std::string>> dialogue_table_;
    entt::entity active_entity_{entt::null};

public:
    DialogueSystem(entt::registry& registry, entt::dispatcher& dispatcher);
    ~DialogueSystem();

    bool loadDialogueFile(std::string_view file_path);
    void update(float delta_time);

private:
    void onInteractRequest(const game::defs::InteractRequest& event);
    void startDialogue(entt::entity entity,
                       game::component::DialogueComponent& dialogue,
                       const std::vector<std::string>& lines,
                       glm::vec2 head_pos);
    [[nodiscard]] bool advanceDialogue(entt::entity entity,
                                      game::component::DialogueComponent& dialogue,
                                      const std::vector<std::string>& lines,
                                      glm::vec2 head_pos);
    void endDialogue(entt::entity entity, game::component::DialogueComponent& dialogue);
    void closeDialogue(entt::entity entity);
    void showLine(entt::entity entity, const std::vector<std::string>& lines, std::size_t line_index, glm::vec2 head_pos);
};

} // namespace game::system
