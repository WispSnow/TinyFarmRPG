#pragma once

#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <entt/signal/fwd.hpp>
#include <glm/vec2.hpp>

#include <unordered_map>

namespace game::defs {
struct DialogueHideEvent;
struct DialogueShowEvent;
} // namespace game::defs

namespace game::system {

/// @brief Tracks Lua-owned conversation bubbles and closes them when the player
/// walks away.
class ScriptedDialogueLifecycleSystem {
public:
    ScriptedDialogueLifecycleSystem(entt::registry& registry, entt::dispatcher& dispatcher);
    ~ScriptedDialogueLifecycleSystem();

    void update(float delta_time);

private:
    struct ActiveDialogue {
        glm::vec2 show_position{0.0F, 0.0F};
        float close_distance{0.0F};
    };

    entt::registry& registry_;
    entt::dispatcher& dispatcher_;
    std::unordered_map<entt::entity, ActiveDialogue> active_dialogues_;

    void onDialogueShow(const game::defs::DialogueShowEvent& event);
    void onDialogueHide(const game::defs::DialogueHideEvent& event);
    void closeDialogue(entt::entity target);
};

} // namespace game::system
