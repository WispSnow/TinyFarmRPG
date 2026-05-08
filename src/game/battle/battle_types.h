#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace game::battle {

/// @brief 单场战斗内稳定的单位标识。
using BattleUnitId = std::uint32_t;

/// @brief JRPG 回合战斗中当前支持的双方阵营。
enum class BattleSide {
    Player,
    Enemy
};

/// @brief 表现层提交给 BattleSession 的行动类型。
enum class BattleActionType {
    Attack,     ///< 使用普通攻击公式的单目标攻击。
    Skill,      ///< 由 RPG 目录数据驱动的技能行动，通过 BattleAction::skill_id 指定。
    Item,       ///< 由物品目录数据驱动的道具行动，通过 BattleAction::item_id 指定。
    Guard,
    Escape,
    EndTurn
};

/// @brief 会话、场景和结算事件共享的战斗结果。
enum class BattleOutcome {
    Ongoing,    ///< 玩家方和敌方都至少有一个存活单位。
    Victory,    ///< 玩家方仍有存活单位，敌方无存活单位。
    Defeat,     ///< 玩家方无存活单位。
    Escaped     ///< 队伍在正常胜负结算前成功逃跑。
};

/// @brief 返回阵营对应的稳定调试字符串。
[[nodiscard]] constexpr const char* toString(BattleSide side) {
    switch (side) {
        case BattleSide::Player:
            return "Player";
        case BattleSide::Enemy:
            return "Enemy";
    }

    return "Unknown";
}

/// @brief 返回战斗结果对应的稳定调试字符串。
[[nodiscard]] constexpr const char* toString(BattleOutcome outcome) {
    switch (outcome) {
        case BattleOutcome::Ongoing:
            return "Ongoing";
        case BattleOutcome::Victory:
            return "Victory";
        case BattleOutcome::Defeat:
            return "Defeat";
        case BattleOutcome::Escaped:
            return "Escaped";
    }

    return "Unknown";
}

/// @brief 已提交行动是否成功改变了战斗状态。
enum class BattleActionStatus {
    Applied,    ///< 行动通过校验，效果已经应用。
    Rejected    ///< 行动被拒绝，原因见 BattleActionResult::failure_reason。
};

struct BattlePortrait {
    std::string path{};
    int x{0};
    int y{0};
    int width{0};
    int height{0};

    [[nodiscard]] bool valid() const { return !path.empty() && width > 0 && height > 0; }
};

/// @brief 回合制战斗领域使用的战斗单位运行时副本。
///
/// BattleUnit 有意不区分玩家/敌人的具体子类型，让双方可以复用同一套回合排序、
/// 目标选择、公式计算与快照序列化路径。
struct BattleUnit {
    BattleUnitId id{0};                        ///< 会话内单位标识；在 units 列表中必须唯一。
    std::string name{};                        ///< UI 与战斗日志使用的显示名。
    BattleSide side{BattleSide::Player};
    int hp{100};
    int max_hp{100};
    int mp{0};
    int max_mp{0};
    int attack{10};
    int defense{10};
    int magic_attack{10};
    int magic_defense{10};
    int speed{10};
    int luck{10};
    std::vector<std::string> skill_ids{};       ///< 该单位可在战斗菜单中使用的技能 ID 列表。
    std::optional<std::string> source_actor_id{};   ///< 对应 RPG catalog actor id；无来源时为空。
    std::optional<std::string> source_enemy_id{};   ///< 对应 RPG catalog enemy id；无来源时为空。
    BattlePortrait portrait{};                  ///< 玩家方角色肖像；敌人或缺失资源时可为空。

    [[nodiscard]] bool isAlive() const { return hp > 0; }
};

/// @brief 玩家、AI 或脚本提交给 BattleSession 的行动意图。
///
/// 该结构只携带标识与轻量参数。校验、目录查表、资源消耗和状态写入由
/// session/resolver 层处理，避免 UI 代码直接操作 TurnCore。
struct BattleAction {
    BattleActionType type{BattleActionType::EndTurn};    ///< 要执行的行动分支。
    BattleUnitId actor_id{0};                            ///< 期望拥有当前回合的行动者。
    std::optional<BattleUnitId> target_id{};              ///< NOTE: 目前仅支持单目标；未来考虑扩展
    std::string skill_id{};                               ///< type == BattleActionType::Skill 时使用的 RPG 目录技能 ID。
    std::string item_id{};                                ///< type == BattleActionType::Item 时使用的物品目录 ID。
};

/// @brief 每次行动后返回给表现层的完整状态视图。
///
/// BattleScene 将快照作为唯一数据源，而不是维护领域状态的并行副本；
struct BattleSnapshot {
    std::vector<BattleUnit> units{};                         ///< 最近一次领域状态迁移后的完整单位状态。
    std::vector<BattleUnitId> turn_order{};                  ///< 按速度排序得到的稳定行动顺序，供表现层推导本轮队列。
    std::optional<BattleUnitId> current_actor_id{};           ///< 当前行动者；战斗不再进行时为 nullopt。
    std::uint32_t round_index{0};                             ///< 从 1 开始的轮次计数；0 表示没有可用的存活行动者。
    BattleOutcome outcome{BattleOutcome::Ongoing};            ///< 基于当前单位状态判定出的战斗结果。
};

/// @brief 已提交行动的处理结果。
///
/// 既包含动画/日志使用的精简结果字段，也包含用于重建 UI 状态的完整 BattleSnapshot。
struct BattleActionResult {
    BattleActionStatus status{BattleActionStatus::Rejected};    ///< 状态发生变更时为 Applied，否则为 Rejected。
    BattleActionType action_type{BattleActionType::EndTurn};
    BattleUnitId actor_id{0};
    std::optional<BattleUnitId> target_id{};                     ///< NOTE: 目前仅支持单目标；未来考虑扩展
    int damage{0};
    int hp_recovered{0};
    int mp_recovered{0};
    int mp_spent{0};
    bool missed{false};
    bool critical{false};
    bool target_guarded{false};
    bool target_defeated{false};
    bool escape_succeeded{false};                                ///< Escape 行动成功并强制进入 BattleOutcome::Escaped 时为 true。
    std::vector<std::string> states_added{};                     ///< 技能效果新施加的状态 ID 列表。
    std::vector<std::string> states_removed{};                   ///< 技能效果移除的状态 ID 列表。
    std::string failure_reason{};                                ///< status == BattleActionStatus::Rejected 时的原因可读字符串。
    BattleOutcome outcome_after{BattleOutcome::Ongoing};         ///< 行动应用或拒绝后的战斗结果。
    BattleSnapshot snapshot{};                                   ///< 行动处理步骤后的完整战斗状态 NOTE: 目前是拷贝，后续可优化为差分结果以降低动作提交开销。
};

} // namespace game::battle
