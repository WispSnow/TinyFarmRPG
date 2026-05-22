#pragma once

#include <entt/entity/fwd.hpp>
#include <entt/signal/fwd.hpp>

namespace engine::script {
class ScriptHost;
}

namespace engine::utils {
struct DayChangedEvent;
struct TimeOfDayChangedEvent;
}

namespace game::defs {
struct BattleEndedEvent;
struct BattleStartedEvent;
struct DialogueHideEvent;
struct InteractCommand;
struct InventoryChanged;
struct ItemUsedEvent;
struct QuestAcceptedEvent;
struct QuestCompletedEvent;
}

namespace game::script {

/// @brief 将 C++ typed event 中转为 Lua tf.event 回调。
class ScriptEventBridge final {
public:
    ScriptEventBridge(engine::script::ScriptHost& host,
                      entt::registry& registry,
                      entt::dispatcher& dispatcher);
    ~ScriptEventBridge();

    ScriptEventBridge(const ScriptEventBridge&) = delete;
    ScriptEventBridge& operator=(const ScriptEventBridge&) = delete;
    ScriptEventBridge(ScriptEventBridge&&) = delete;
    ScriptEventBridge& operator=(ScriptEventBridge&&) = delete;

    void drainDeferredCommands();

private:
    void subscribe();
    void unsubscribe();

    void onInteract(const game::defs::InteractCommand& event);
    void onDialogueClosed(const game::defs::DialogueHideEvent& event);
    void onInventoryChanged(const game::defs::InventoryChanged& event);
    void onItemUsed(const game::defs::ItemUsedEvent& event);
    void onBattleStarted(const game::defs::BattleStartedEvent& event);
    void onBattleEnded(const game::defs::BattleEndedEvent& event);
    void onDayChanged(const engine::utils::DayChangedEvent& event);
    void onTimeOfDayChanged(const engine::utils::TimeOfDayChangedEvent& event);
    void onQuestAccepted(const game::defs::QuestAcceptedEvent& event);
    void onQuestCompleted(const game::defs::QuestCompletedEvent& event);

    engine::script::ScriptHost& host_;
    entt::registry& registry_;
    entt::dispatcher& dispatcher_;
};

} // namespace game::script
