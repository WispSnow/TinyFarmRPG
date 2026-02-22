#pragma once

#include "save_data.h"

#include <atomic>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

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
public:
    struct AsyncSaveResult final {
        std::filesystem::path file_path{};
        bool success{false};
        std::string error{};
    };

private:
    engine::core::Context& context_;
    entt::registry& registry_;
    game::world::WorldState& world_state_;
    game::world::MapManager& map_manager_;
    game::factory::BlueprintManager& blueprint_manager_;
    std::atomic<bool> save_in_progress_{false};
    std::mutex async_result_mutex_{};
    std::optional<AsyncSaveResult> async_save_result_{};
    std::optional<std::jthread> async_save_thread_{};

public:
    SaveService(engine::core::Context& context,
                entt::registry& registry,
                game::world::WorldState& world_state,
                game::world::MapManager& map_manager,
                game::factory::BlueprintManager& blueprint_manager);
    ~SaveService() = default;

    [[nodiscard]] bool saveToFile(const std::filesystem::path& file_path, std::string& out_error);
    [[nodiscard]] bool saveToFileAsync(const std::filesystem::path& file_path, std::string& out_error);
    [[nodiscard]] bool isSaving() const { return save_in_progress_.load(std::memory_order_acquire); }
    [[nodiscard]] std::optional<AsyncSaveResult> consumeAsyncSaveResult();
    [[nodiscard]] bool loadFromFile(const std::filesystem::path& file_path, std::string& out_error);

    [[nodiscard]] static std::filesystem::path slotPath(int slot);

private:
    [[nodiscard]] static bool writeSaveFile(const SaveData& data,
                                            const std::filesystem::path& file_path,
                                            std::string& out_error);
    void cleanupCompletedSaveThread();
    [[nodiscard]] SaveData capture(std::string& out_error) const;
    [[nodiscard]] bool apply(const SaveData& data, std::string& out_error);
};

} // namespace game::save
