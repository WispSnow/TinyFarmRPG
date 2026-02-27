#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace game::battle {

using BattleUnitId = std::uint32_t;

enum class BattleSide {
    Player,
    Enemy
};

enum class BattleActionType {
    Attack,
    Skill,
    Item,
    Guard,
    Escape,
    EndTurn
};

enum class BattleOutcome {
    Ongoing,
    Victory,
    Defeat
};

[[nodiscard]] constexpr const char* toString(BattleSide side) {
    switch (side) {
        case BattleSide::Player:
            return "Player";
        case BattleSide::Enemy:
            return "Enemy";
    }

    return "Unknown";
}

[[nodiscard]] constexpr const char* toString(BattleOutcome outcome) {
    switch (outcome) {
        case BattleOutcome::Ongoing:
            return "Ongoing";
        case BattleOutcome::Victory:
            return "Victory";
        case BattleOutcome::Defeat:
            return "Defeat";
    }

    return "Unknown";
}

enum class BattleActionStatus {
    Applied,
    Rejected
};

struct BattleUnit {
    BattleUnitId id{0};
    std::string name{};
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

    [[nodiscard]] bool isAlive() const { return hp > 0; }
};

struct BattleAction {
    BattleActionType type{BattleActionType::EndTurn};
    BattleUnitId actor_id{0};
    std::optional<BattleUnitId> target_id{};
    std::string skill_id{};
    std::string item_id{};
};

struct BattleSnapshot {
    std::vector<BattleUnit> units{};
    std::optional<BattleUnitId> current_actor_id{};
    std::uint32_t round_index{0};
    BattleOutcome outcome{BattleOutcome::Ongoing};
};

struct BattleActionResult {
    BattleActionStatus status{BattleActionStatus::Rejected};
    BattleActionType action_type{BattleActionType::EndTurn};
    BattleUnitId actor_id{0};
    std::optional<BattleUnitId> target_id{};
    int damage{0};
    bool target_defeated{false};
    BattleOutcome outcome_after{BattleOutcome::Ongoing};
    // TODO(FND-010): snapshot 当前是全量拷贝；后续可优化为差分结果以降低动作提交开销。
    BattleSnapshot snapshot{};
};

} // namespace game::battle
