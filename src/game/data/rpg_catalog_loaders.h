#pragma once

#include "game/data/rpg_data.h"

#include <entt/core/fwd.hpp>

#include <string_view>
#include <unordered_map>

namespace game::data {

[[nodiscard]] bool loadRpgClassesFile(std::string_view file_path,
                                      std::unordered_map<entt::id_type, ClassData>& out_classes);
[[nodiscard]] bool loadRpgActorsFile(std::string_view file_path,
                                     std::unordered_map<entt::id_type, ActorData>& out_actors);
[[nodiscard]] bool loadRpgSkillsFile(std::string_view file_path,
                                     std::unordered_map<entt::id_type, SkillData>& out_skills);
[[nodiscard]] bool loadRpgStatesFile(std::string_view file_path,
                                     std::unordered_map<entt::id_type, StateData>& out_states);
[[nodiscard]] bool loadRpgEquipmentFile(std::string_view file_path,
                                        std::unordered_map<entt::id_type, EquipmentData>& out_equipment);
[[nodiscard]] bool loadRpgEnemiesFile(std::string_view file_path,
                                      std::unordered_map<entt::id_type, EnemyData>& out_enemies);
[[nodiscard]] bool loadRpgTroopsFile(std::string_view file_path,
                                     std::unordered_map<entt::id_type, TroopData>& out_troops);

} // namespace game::data
