#pragma once

#include "engine/script/script_entity_handle.h"

#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <entt/signal/fwd.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace engine::script {
class ScriptHost;
}

namespace game::script {

struct ScriptCommandResult {
    bool ok{false};
    std::string reason{};
};

struct QuestProgressSnapshot {
    int current{0};
    int required{0};
};

/// @brief 游戏脚本 API 的 C++ facade。
///
/// 该类型负责把 Lua 绑定层的调用转译为稳定的游戏 command/event。
/// 它不直接承载新玩法规则；只有当操作需要多步原子写入或共享校验时，
/// 才应在 facade 内部委托给对应 domain service。
class ScriptGameApi final {
public:
    ScriptGameApi(engine::script::ScriptHost& host,
                  entt::registry& registry,
                  entt::dispatcher& dispatcher);

    [[nodiscard]] std::uint32_t day() const;
    [[nodiscard]] float hour() const;
    [[nodiscard]] float minute() const;
    [[nodiscard]] std::string formattedTime() const;

    [[nodiscard]] bool playerExists() const;
    [[nodiscard]] std::optional<engine::script::ScriptEntityHandle> playerHandle() const;
    [[nodiscard]] std::tuple<float, float> playerPosition() const;

    [[nodiscard]] std::optional<std::string> entityActorId(const engine::script::ScriptEntityHandle& handle) const;
    [[nodiscard]] std::optional<std::string> entityName(const engine::script::ScriptEntityHandle& handle) const;
    [[nodiscard]] std::tuple<float, float> entityPosition(const engine::script::ScriptEntityHandle& handle) const;
    [[nodiscard]] bool entityHasComponent(const engine::script::ScriptEntityHandle& handle,
                                          std::string_view kind) const;

    [[nodiscard]] std::string questStatus(std::string_view quest_id) const;
    [[nodiscard]] QuestProgressSnapshot questProgress(std::string_view quest_id,
                                                      std::string_view objective_id) const;
    [[nodiscard]] bool questIsAvailable(std::string_view quest_id) const;
    [[nodiscard]] ScriptCommandResult questAccept(
        std::string_view quest_id,
        const std::optional<engine::script::ScriptEntityHandle>& giver_handle);
    [[nodiscard]] ScriptCommandResult questTurnIn(
        std::string_view quest_id,
        const std::optional<engine::script::ScriptEntityHandle>& giver_handle);

    [[nodiscard]] std::vector<std::string> partyMembers() const;
    [[nodiscard]] bool partyIsRecruited(std::string_view actor_id) const;
    [[nodiscard]] ScriptCommandResult partyRequestRecruit(
        std::string_view actor_id,
        const std::optional<engine::script::ScriptEntityHandle>& recruiter_handle);
    [[nodiscard]] int partyLevel(std::string_view actor_id) const;
    [[nodiscard]] int partyInitialLevel(std::string_view actor_id) const;

    [[nodiscard]] ScriptCommandResult shopOpen(
        std::string_view shop_id,
        const std::optional<engine::script::ScriptEntityHandle>& merchant_handle);

    [[nodiscard]] ScriptCommandResult battleStart(std::string_view troop_id,
                                                  std::vector<std::string> actor_ids,
                                                  std::string_view battle_background_id);

    [[nodiscard]] std::string currentMap() const;

    [[nodiscard]] bool addItem(std::string_view item_id,
                               int count,
                               const std::optional<engine::script::ScriptEntityHandle>& target_handle,
                               int preferred_slot);
    [[nodiscard]] bool removeItem(std::string_view item_id,
                                  int count,
                                  const std::optional<engine::script::ScriptEntityHandle>& target_handle,
                                  int slot_index);
    [[nodiscard]] bool inventorySync(const std::optional<engine::script::ScriptEntityHandle>& target_handle);
    [[nodiscard]] bool hotbarSync(const std::optional<engine::script::ScriptEntityHandle>& target_handle,
                                  bool full_sync);
    [[nodiscard]] bool interact(const engine::script::ScriptEntityHandle& target_handle,
                                const std::optional<engine::script::ScriptEntityHandle>& player_handle);

    [[nodiscard]] bool showDialogue(std::string_view text,
                                    std::string_view speaker,
                                    int channel,
                                    const std::optional<engine::script::ScriptEntityHandle>& target_handle,
                                    std::string_view speaker_actor_id);
    [[nodiscard]] bool hideDialogue(int channel,
                                    const std::optional<engine::script::ScriptEntityHandle>& target_handle);

private:
    [[nodiscard]] bool resolveTargetEntity(const std::optional<engine::script::ScriptEntityHandle>& raw_target,
                                           std::string_view api_name,
                                           entt::entity& out_target,
                                           bool require_default_player) const;

    engine::script::ScriptHost& host_;
    entt::registry& registry_;
    entt::dispatcher& dispatcher_;
};

} // namespace game::script
