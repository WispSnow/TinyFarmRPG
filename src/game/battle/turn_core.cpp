#include "turn_core.h"

#include <algorithm>
#include <numeric>
#include <utility>

namespace game::battle {

TurnCore::TurnCore(std::vector<BattleUnit> units)
    : units_(std::move(units)) {
    buildIndex();
    buildTurnOrder();
    refresh();
    round_index_ = currentActorId().has_value() ? 1U : 0U;
}

const BattleUnit* TurnCore::findUnit(BattleUnitId id) const {
    const auto it = unit_index_by_id_.find(id);
    if (it == unit_index_by_id_.end()) {
        return nullptr;
    }
    return &units_[it->second];
}

BattleUnit* TurnCore::findUnitMutable(BattleUnitId id) {
    const auto it = unit_index_by_id_.find(id);
    if (it == unit_index_by_id_.end()) {
        return nullptr;
    }
    return &units_[it->second];
}

std::optional<BattleUnitId> TurnCore::currentActorId() const {
    if (outcome_ != BattleOutcome::Ongoing || turn_order_.empty()) {
        return std::nullopt;
    }

    if (current_turn_index_ >= turn_order_.size()) {
        return std::nullopt;
    }

    const BattleUnitId id = turn_order_[current_turn_index_];
    if (!isAlive(id)) {
        return std::nullopt;
    }
    return id;
}

bool TurnCore::advanceTurn() {
    evaluateOutcome();
    if (outcome_ != BattleOutcome::Ongoing || turn_order_.empty()) {
        return false;
    }

    const std::size_t order_size = turn_order_.size();
    const std::size_t previous_index = current_turn_index_;
    for (std::size_t offset = 0; offset < order_size; ++offset) {
        current_turn_index_ = (current_turn_index_ + 1) % order_size;
        if (isAlive(turn_order_[current_turn_index_])) {
            if (current_turn_index_ <= previous_index) {
                const std::uint32_t finished_round = round_index_ == 0U ? 1U : round_index_;
                if (on_round_end_) {
                    on_round_end_(finished_round);
                }
                round_index_ = finished_round + 1U;
                if (on_round_begin_) {
                    on_round_begin_(round_index_);
                }
            }
            return true;
        }
    }

    refresh();
    return false;
}

void TurnCore::setRoundHooks(RoundHook on_round_begin, RoundHook on_round_end) {
    on_round_begin_ = std::move(on_round_begin);
    on_round_end_ = std::move(on_round_end);
}

void TurnCore::refresh() {
    evaluateOutcome();
    alignCurrentActor();
}

void TurnCore::buildIndex() {
    unit_index_by_id_.clear();
    unit_index_by_id_.reserve(units_.size());

    for (std::size_t i = 0; i < units_.size(); ++i) {
        unit_index_by_id_.insert_or_assign(units_[i].id, i);
    }
}

void TurnCore::buildTurnOrder() {
    turn_order_.clear();
    turn_order_.reserve(units_.size());

    std::vector<std::size_t> indices(units_.size());
    std::iota(indices.begin(), indices.end(), static_cast<std::size_t>(0));

    std::stable_sort(indices.begin(), indices.end(), [this](const std::size_t lhs, const std::size_t rhs) {
        return units_[lhs].speed > units_[rhs].speed;
    });

    for (const std::size_t index : indices) {
        turn_order_.push_back(units_[index].id);
    }

    current_turn_index_ = 0;
}

void TurnCore::evaluateOutcome() {
    bool has_alive_player = false;
    bool has_alive_enemy = false;

    for (const auto& unit : units_) {
        if (!unit.isAlive()) {
            continue;
        }

        if (unit.side == BattleSide::Player) {
            has_alive_player = true;
        } else {
            has_alive_enemy = true;
        }
    }

    if (has_alive_player && has_alive_enemy) {
        outcome_ = BattleOutcome::Ongoing;
        return;
    }

    if (has_alive_player) {
        outcome_ = BattleOutcome::Victory;
        return;
    }

    outcome_ = BattleOutcome::Defeat;
}

void TurnCore::alignCurrentActor() {
    if (outcome_ != BattleOutcome::Ongoing || turn_order_.empty()) {
        return;
    }

    const std::size_t order_size = turn_order_.size();
    current_turn_index_ %= order_size;

    for (std::size_t offset = 0; offset < order_size; ++offset) {
        const std::size_t candidate_index = (current_turn_index_ + offset) % order_size;
        if (!isAlive(turn_order_[candidate_index])) {
            continue;
        }
        current_turn_index_ = candidate_index;
        return;
    }
}

bool TurnCore::isAlive(BattleUnitId id) const {
    const auto* unit = findUnit(id);
    return unit != nullptr && unit->isAlive();
}

} // namespace game::battle
