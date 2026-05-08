#include "game/battle/battle_log_formatter.h"

#include "game/data/item_catalog.h"
#include "game/data/rpg_catalog.h"
#include "game/data/rpg_data.h"
#include "game/data/rpg_types.h"

#include <entt/entity/entity.hpp>

#include <algorithm>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace game::battle {
namespace {

constexpr std::size_t kMaxLogLinesPerAction = 3;

struct LogSubject {
    std::string label{};
    bool plural{false};
};

[[nodiscard]] const BattleUnit* findSnapshotUnit(const BattleActionResult& result, const BattleUnitId unit_id) {
    const auto it = std::find_if(
        result.snapshot.units.begin(),
        result.snapshot.units.end(),
        [unit_id](const BattleUnit& unit) {
            return unit.id == unit_id;
        });
    return it == result.snapshot.units.end() ? nullptr : &*it;
}

[[nodiscard]] std::string unitLabel(const BattleActionResult& result,
                                    const std::optional<BattleUnitId> unit_id,
                                    std::string_view fallback_prefix) {
    if (!unit_id.has_value()) {
        return std::string{fallback_prefix};
    }

    if (const auto* unit = findSnapshotUnit(result, *unit_id)) {
        return unit->name.empty() ? std::string{fallback_prefix} + " #" + std::to_string(*unit_id) : unit->name;
    }

    return std::string{fallback_prefix} + " #" + std::to_string(*unit_id);
}

[[nodiscard]] std::string actorLabel(const BattleActionResult& result) {
    return unitLabel(result, result.actor_id, "Actor");
}

[[nodiscard]] std::string targetLabel(const BattleActionResult& result) {
    return unitLabel(result, result.target_id, "Target");
}

[[nodiscard]] LogSubject effectSubject(const BattleActionResult& result,
                                       const BattleLogFormatterContext& context) {
    if (result.target_id.has_value()) {
        return LogSubject{.label = targetLabel(result), .plural = false};
    }

    if (result.action_type == BattleActionType::Skill && context.rpg_catalog) {
        if (const auto* skill = context.rpg_catalog->findSkill(result.skill_id)) {
            if (skill->scope_ == game::data::Scope::Self) {
                return LogSubject{.label = actorLabel(result), .plural = false};
            }
        }
    }

    if (result.action_type == BattleActionType::Item && context.item_catalog) {
        const entt::id_type item_id_hash = game::data::RpgCatalog::hashId(result.item_id);
        if (const auto* item = context.item_catalog->findItem(item_id_hash);
            item && item->battle_use_ && item->battle_use_->scope == game::data::Scope::Self) {
            return LogSubject{.label = actorLabel(result), .plural = false};
        }
    }

    // Until BattleActionResult carries per-target events, all-scope skill/item feedback is summarized.
    return LogSubject{.label = "Targets", .plural = true};
}

[[nodiscard]] std::string subjectVerb(const LogSubject& subject,
                                      std::string_view singular,
                                      std::string_view plural) {
    return std::string{subject.plural ? plural : singular};
}

[[nodiscard]] std::string skillLabel(const std::string& skill_id, const game::data::RpgCatalog* catalog) {
    if (skill_id.empty()) {
        return "skill";
    }

    if (catalog) {
        if (const auto* skill = catalog->findSkill(skill_id);
            skill && !skill->display_name_.empty()) {
            return skill->display_name_;
        }
    }

    return skill_id;
}

[[nodiscard]] std::string itemLabel(const std::string& item_id, const game::data::ItemCatalog* catalog) {
    if (item_id.empty()) {
        return "item";
    }

    if (catalog) {
        const entt::id_type item_id_hash = game::data::RpgCatalog::hashId(item_id);
        if (const auto* item = catalog->findItem(item_id_hash);
            item && !item->display_name_.empty()) {
            return item->display_name_;
        }
    }

    return item_id;
}

[[nodiscard]] std::string stateLabel(const std::string& state_id, const game::data::RpgCatalog* catalog) {
    if (state_id.empty()) {
        return "state";
    }

    if (catalog) {
        if (const auto* state = catalog->findState(state_id);
            state && !state->display_name_.empty()) {
            return state->display_name_;
        }
    }

    return state_id;
}

[[nodiscard]] std::string joinLabels(const std::vector<std::string>& labels) {
    std::ostringstream stream;
    for (std::size_t index = 0; index < labels.size(); ++index) {
        if (index > 0U) {
            stream << ", ";
        }
        stream << labels[index];
    }
    return stream.str();
}

[[nodiscard]] std::vector<std::string> stateLabels(const std::vector<std::string>& state_ids,
                                                   const game::data::RpgCatalog* catalog) {
    std::vector<std::string> labels;
    labels.reserve(state_ids.size());
    for (const auto& state_id : state_ids) {
        labels.push_back(stateLabel(state_id, catalog));
    }
    return labels;
}

void pushLogLine(std::vector<BattleLogLine>& out_lines,
                 std::string text,
                 const BattleLogTone tone,
                 const std::size_t line_limit = kMaxLogLinesPerAction) {
    if (text.empty() || out_lines.size() >= line_limit) {
        return;
    }

    out_lines.push_back(BattleLogLine{
        .text = std::move(text),
        .tone = tone
    });
}

[[nodiscard]] bool hasTerminalFeedback(const BattleActionResult& result) {
    return result.action_type == BattleActionType::Escape || result.target_defeated;
}

[[nodiscard]] std::size_t primaryFeedbackLineLimit(const BattleActionResult& result) {
    const std::size_t terminal_reserve = hasTerminalFeedback(result) ? 1U : 0U;
    return kMaxLogLinesPerAction - terminal_reserve;
}

[[nodiscard]] std::string recoveryText(const LogSubject& subject, const BattleActionResult& result) {
    std::vector<std::string> parts;
    if (result.hp_recovered > 0) {
        parts.push_back(std::to_string(result.hp_recovered) + " HP");
    }
    if (result.mp_recovered > 0) {
        parts.push_back(std::to_string(result.mp_recovered) + " MP");
    }

    if (parts.empty()) {
        return {};
    }

    return subject.label + subjectVerb(subject, " recovers ", " recover ") + joinLabels(parts) + ".";
}

void pushPrimaryFeedback(const BattleActionResult& result,
                         const BattleLogFormatterContext& context,
                         std::vector<BattleLogLine>& out_lines) {
    const std::string target = targetLabel(result);
    const LogSubject subject = effectSubject(result, context);
    const std::size_t line_limit = primaryFeedbackLineLimit(result);

    if (result.missed) {
        pushLogLine(
            out_lines,
            result.target_id.has_value() ? target + " evades the attack." : "Targets evade the attack.",
            BattleLogTone::System,
            line_limit);
        return;
    }

    if (result.damage > 0) {
        std::string damage_text;
        if (result.critical) {
            damage_text += "Critical hit! ";
        }
        if (result.target_guarded) {
            damage_text += "Guarded. ";
        }
        damage_text += subject.label + subjectVerb(subject, " takes ", " take ") + std::to_string(result.damage) + " damage.";
        pushLogLine(out_lines, std::move(damage_text), BattleLogTone::Damage, line_limit);
    } else if (result.target_guarded) {
        pushLogLine(
            out_lines,
            subject.label + subjectVerb(subject, " guards", " guard") + " against the blow.",
            BattleLogTone::System,
            line_limit);
    }

    if (const std::string recovery = recoveryText(subject, result);
        !recovery.empty()) {
        pushLogLine(out_lines, recovery, BattleLogTone::Recovery, line_limit);
    }
    if (!result.states_added.empty()) {
        pushLogLine(
            out_lines,
            subject.label + subjectVerb(subject, " gains ", " gain ") + joinLabels(stateLabels(result.states_added, context.rpg_catalog)) + ".",
            BattleLogTone::State,
            line_limit);
    }
    if (!result.states_removed.empty()) {
        pushLogLine(
            out_lines,
            subject.label + subjectVerb(subject, " loses ", " lose ") + joinLabels(stateLabels(result.states_removed, context.rpg_catalog)) + ".",
            BattleLogTone::State,
            line_limit);
    }
}

void pushTerminalFeedback(const BattleActionResult& result, std::vector<BattleLogLine>& out_lines) {
    if (result.action_type == BattleActionType::Escape) {
        pushLogLine(
            out_lines,
            result.escape_succeeded ? "The party escaped!" : "Escape failed!",
            result.escape_succeeded ? BattleLogTone::System : BattleLogTone::Error);
        return;
    }

    if (result.target_defeated) {
        pushLogLine(
            out_lines,
            result.target_id.has_value() ? targetLabel(result) + " was defeated!" : "A target was defeated!",
            BattleLogTone::System);
    }
}

[[nodiscard]] std::string actionStartText(const BattleActionResult& result,
                                          const BattleLogFormatterContext& context) {
    const std::string actor = actorLabel(result);
    switch (result.action_type) {
        case BattleActionType::Attack:
            return actor + " attacks " + targetLabel(result) + "!";
        case BattleActionType::Skill:
            return actor + " uses " + skillLabel(result.skill_id, context.rpg_catalog) + "!";
        case BattleActionType::Item:
            return actor + " uses " + itemLabel(result.item_id, context.item_catalog) + "!";
        case BattleActionType::Guard:
            return actor + " guards.";
        case BattleActionType::Escape:
            return actor + " tries to escape.";
        case BattleActionType::EndTurn:
            return actor + " waits.";
    }

    return actor + " acts.";
}

} // namespace

std::vector<BattleLogLine> formatBattleLogLines(const BattleActionResult& result,
                                                const BattleLogFormatterContext context) {
    std::vector<BattleLogLine> lines;
    lines.reserve(kMaxLogLinesPerAction);

    if (result.status == BattleActionStatus::Rejected) {
        pushLogLine(
            lines,
            result.failure_reason.empty() ? "Action failed." : "Action failed: " + result.failure_reason,
            BattleLogTone::Error);
        return lines;
    }

    pushLogLine(lines, actionStartText(result, context), BattleLogTone::Normal);
    pushPrimaryFeedback(result, context, lines);
    pushTerminalFeedback(result, lines);
    return lines;
}

} // namespace game::battle
