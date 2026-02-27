#pragma once

#include "game/data/rpg_data.h"

#include <entt/core/fwd.hpp>

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace game::data {

class ItemCatalog;

class RpgCatalog final {
public:
    [[nodiscard]] static entt::id_type hashId(std::string_view id);

    [[nodiscard]] bool loadManifest(std::string_view file_path);
    [[nodiscard]] bool loadClasses(std::string_view file_path);
    [[nodiscard]] bool loadActors(std::string_view file_path);
    [[nodiscard]] bool loadSkills(std::string_view file_path);
    [[nodiscard]] bool loadStates(std::string_view file_path);
    [[nodiscard]] bool loadEnemies(std::string_view file_path);
    [[nodiscard]] bool loadTroops(std::string_view file_path);

    [[nodiscard]] bool validateReferences(std::string& out_error, const ItemCatalog* item_catalog = nullptr) const;

    [[nodiscard]] const RpgManifest* manifest() const { return has_manifest_ ? &manifest_ : nullptr; }
    [[nodiscard]] const ClassData* findClass(entt::id_type id_hash) const;
    [[nodiscard]] const ClassData* findClass(std::string_view id) const;
    [[nodiscard]] const ActorData* findActor(entt::id_type id_hash) const;
    [[nodiscard]] const ActorData* findActor(std::string_view id) const;
    [[nodiscard]] const SkillData* findSkill(entt::id_type id_hash) const;
    [[nodiscard]] const SkillData* findSkill(std::string_view id) const;
    [[nodiscard]] const StateData* findState(entt::id_type id_hash) const;
    [[nodiscard]] const StateData* findState(std::string_view id) const;
    [[nodiscard]] const EnemyData* findEnemy(entt::id_type id_hash) const;
    [[nodiscard]] const EnemyData* findEnemy(std::string_view id) const;
    [[nodiscard]] const TroopData* findTroop(entt::id_type id_hash) const;
    [[nodiscard]] const TroopData* findTroop(std::string_view id) const;

    [[nodiscard]] std::vector<const ActorData*> listActors() const;
    [[nodiscard]] std::vector<const TroopData*> listTroops() const;

private:
    bool has_manifest_{false};
    RpgManifest manifest_{};
    std::unordered_map<entt::id_type, ClassData> classes_{};
    std::unordered_map<entt::id_type, ActorData> actors_{};
    std::unordered_map<entt::id_type, SkillData> skills_{};
    std::unordered_map<entt::id_type, StateData> states_{};
    std::unordered_map<entt::id_type, EnemyData> enemies_{};
    std::unordered_map<entt::id_type, TroopData> troops_{};
};

} // namespace game::data
