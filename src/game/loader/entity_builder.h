#pragma once
#include "engine/loader/basic_entity_builder.h"

#include <unordered_set>

namespace game::factory {
    class EntityFactory;
}

namespace game::loader {

class EntityBuilder : public engine::loader::BasicEntityBuilder {
    game::factory::EntityFactory& entity_factory_;
    entt::id_type map_id_{entt::null};
    bool reuse_player_if_exists_{false};
    std::unordered_set<int> seen_encounter_ids_{};

public:
    EntityBuilder(engine::loader::LevelLoader& level_loader, 
        engine::core::Context& context, 
        entt::registry& registry,
        game::factory::EntityFactory& entity_factory);

    [[nodiscard]] EntityBuilder* build() override;
    void decorateExternalEntity(entt::entity entity) override;

    void setMapId(entt::id_type map_id) noexcept {
        if (map_id_ != map_id) {
            seen_encounter_ids_.clear();
        }
        map_id_ = map_id;
    }
    void setReusePlayerIfExists(bool reuse) noexcept { reuse_player_if_exists_ = reuse; }

private:
    void buildActor(entt::id_type name_id);
    void buildAnimal(entt::id_type name_id);
    void buildFarmTag();
    void buildMapTrigger();
    void buildRestArea();
    void buildClosetArea();
    void attachMapId();
    void attachScriptMetadata();

    void buildPointLight();
    void buildSpotLight();
    void buildEmissiveRect();
    [[nodiscard]] bool isEncounterDefeated(int encounter_id) const;

};

} // namespace game::loader
