#pragma once
#include "blueprint.h"
#include <entt/entity/entity.hpp>
#include <unordered_map>

namespace game::factory {

class BlueprintManager {
    std::unordered_map<entt::id_type, ActorBlueprint> actor_blueprints_;
    std::unordered_map<entt::id_type, AnimalBlueprint> animal_blueprints_;
    std::unordered_map<entt::id_type, CropBlueprint> crop_blueprints_;

public:
    BlueprintManager();
    ~BlueprintManager();

    bool loadActorBlueprints(std::string_view file_path);
    bool loadAnimalBlueprints(std::string_view file_path);
    bool loadCropBlueprints(std::string_view file_path);

    [[nodiscard]] const ActorBlueprint& getActorBlueprint(entt::id_type actor_name_id) const { return actor_blueprints_.at(actor_name_id); }
    [[nodiscard]] const AnimalBlueprint& getAnimalBlueprint(entt::id_type animal_name_id) const { return animal_blueprints_.at(animal_name_id); }
    [[nodiscard]] const CropBlueprint& getCropBlueprint(entt::id_type crop_type_id) const { return crop_blueprints_.at(crop_type_id); }

    [[nodiscard]] bool hasActorBlueprint(entt::id_type actor_name_id) const { return actor_blueprints_.contains(actor_name_id); }
    [[nodiscard]] bool hasAnimalBlueprint(entt::id_type animal_name_id) const { return animal_blueprints_.contains(animal_name_id); }
    [[nodiscard]] bool hasCropBlueprint(entt::id_type crop_type_id) const { return crop_blueprints_.contains(crop_type_id); }

    [[nodiscard]] std::size_t actorBlueprintCount() const { return actor_blueprints_.size(); }
    [[nodiscard]] std::size_t animalBlueprintCount() const { return animal_blueprints_.size(); }
    [[nodiscard]] std::size_t cropBlueprintCount() const { return crop_blueprints_.size(); }

    [[nodiscard]] const std::unordered_map<entt::id_type, ActorBlueprint>& actorBlueprints() const { return actor_blueprints_; }
    [[nodiscard]] const std::unordered_map<entt::id_type, AnimalBlueprint>& animalBlueprints() const { return animal_blueprints_; }
    [[nodiscard]] const std::unordered_map<entt::id_type, CropBlueprint>& cropBlueprints() const { return crop_blueprints_; }

};
}   // namespace game::factory
