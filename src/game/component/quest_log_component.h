#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace game::component {

struct QuestLogComponent {
    std::vector<std::string> active_quests{};
    std::vector<std::string> completed_quests{};
    std::unordered_map<std::string, int> objective_progress{};
};

} // namespace game::component
