#pragma once

#include "save_data.h"

#include <filesystem>
#include <string>

#include <entt/entity/fwd.hpp>

namespace engine::core {
class Context;
}

namespace game::factory {
class BlueprintManager;
}

namespace game::world {
class WorldState;
class MapManager;
}

namespace game::save {

class SaveService final {
    engine::core::Context& context_;
    entt::registry& registry_;
    game::world::WorldState& world_state_;
    game::world::MapManager& map_manager_;
    game::factory::BlueprintManager& blueprint_manager_;
    
public:
    SaveService(engine::core::Context& context,
                entt::registry& registry,
                game::world::WorldState& world_state,
                game::world::MapManager& map_manager,
                game::factory::BlueprintManager& blueprint_manager);

    [[nodiscard]] bool saveToFile(const std::filesystem::path& file_path, std::string& out_error);
    [[nodiscard]] bool loadFromFile(const std::filesystem::path& file_path, std::string& out_error);

    [[nodiscard]] static std::filesystem::path slotPath(int slot);

private:
    [[nodiscard]] SaveData capture(std::string& out_error) const;
    [[nodiscard]] bool apply(const SaveData& data, std::string& out_error);
};

} // namespace game::save
