#pragma once

#include "save_data.h"

#include <atomic>
#include <filesystem>
#include <memory>
#if defined(TF_ENABLE_RUNTIME_THREADS)
#include <optional>
#include <thread>
#endif
#include <string>

#include <entt/entity/fwd.hpp>

namespace engine::core {
class Context;
}

namespace game::factory {
class BlueprintManager;
}

namespace game::data {
class RpgCatalog;
}

namespace game::world {
class WorldState;
class MapManager;
}

namespace game::save {

class SaveService final {
private:
    engine::core::Context& context_;
    entt::registry& registry_;
    game::world::WorldState& world_state_;
    game::world::MapManager& map_manager_;
    game::factory::BlueprintManager& blueprint_manager_;
    const game::data::RpgCatalog* rpg_catalog_{nullptr};
    std::shared_ptr<std::atomic<bool>> save_in_progress_{std::make_shared<std::atomic<bool>>(false)};
#if defined(TF_ENABLE_RUNTIME_THREADS)
    std::optional<std::jthread> async_save_thread_{};
#endif

public:
    SaveService(engine::core::Context& context,
                entt::registry& registry,
                game::world::WorldState& world_state,
                game::world::MapManager& map_manager,
                game::factory::BlueprintManager& blueprint_manager,
                const game::data::RpgCatalog* rpg_catalog = nullptr);
    ~SaveService() = default;

    [[nodiscard]] bool saveToFile(const std::filesystem::path& file_path, std::string& out_error);
    [[nodiscard]] bool saveToFileAsync(const std::filesystem::path& file_path, std::string& out_error);
    [[nodiscard]] bool isSaving() const { return save_in_progress_->load(std::memory_order_acquire); }
    [[nodiscard]] bool loadFromFile(const std::filesystem::path& file_path, std::string& out_error);

    [[nodiscard]] static std::filesystem::path slotPath(int slot);
    [[nodiscard]] static bool deleteSlot(int slot, std::string& out_error);

private:
    [[nodiscard]] static bool writeSaveFile(const SaveData& data,
                                            const std::filesystem::path& file_path,
                                            std::string& out_error);
    void cleanupCompletedSaveThread();
    [[nodiscard]] SaveData capture(std::string& out_error) const;
    [[nodiscard]] bool apply(const SaveData& data, std::string& out_error);
};

} // namespace game::save
