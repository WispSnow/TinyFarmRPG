#pragma once

#include <cstdint>
#include <entt/entity/fwd.hpp>

namespace game::factory {
class BlueprintManager;
}

namespace game::world {

struct RegistrySnapshot;

constexpr std::uint32_t MAP_DYNAMIC_SNAPSHOT_VERSION = 3;

enum class DynamicSnapshotFlags : std::uint32_t {
    None = 0,
    IncludeResources = 1u << 0,
};

[[nodiscard]] constexpr DynamicSnapshotFlags operator|(DynamicSnapshotFlags a, DynamicSnapshotFlags b) {
    return static_cast<DynamicSnapshotFlags>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}

[[nodiscard]] constexpr bool hasFlag(DynamicSnapshotFlags value, DynamicSnapshotFlags flag) {
    return (static_cast<std::uint32_t>(value) & static_cast<std::uint32_t>(flag)) != 0u;
}

void writeDynamicSnapshot(entt::registry& registry,
                          entt::id_type map_id,
                          RegistrySnapshot& out,
                          DynamicSnapshotFlags flags = DynamicSnapshotFlags::IncludeResources);

[[nodiscard]] bool readDynamicSnapshot(entt::registry& registry,
                                       RegistrySnapshot& snapshot,
                                       DynamicSnapshotFlags* out_flags = nullptr);

void simulateOfflineDays(entt::registry& registry,
                         const game::factory::BlueprintManager& blueprint_manager,
                         std::uint32_t days);

[[nodiscard]] bool peekDynamicSnapshotFlags(const RegistrySnapshot& snapshot, DynamicSnapshotFlags& out_flags);

} // namespace game::world
