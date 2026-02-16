#include "map_snapshot_serializer.h"

#include "game/world/world_state.h"

#include "game/defs/crop_defs.h"
#include "game/defs/constants.h"
#include "game/defs/render_layers.h"
#include "game/defs/spatial_layers.h"
#include "game/component/crop_component.h"
#include "game/component/map_component.h"
#include "game/component/resource_node_component.h"
#include "game/factory/blueprint_manager.h"

#include "engine/component/animation_component.h"
#include "engine/component/auto_tile_component.h"
#include "engine/component/collider_component.h"
#include "engine/component/render_component.h"
#include "engine/component/sprite_component.h"
#include "engine/component/tags.h"
#include "engine/component/transform_component.h"
#include "engine/utils/math.h"

#include <entt/entity/registry.hpp>
#include <entt/entity/snapshot.hpp>
#include <spdlog/spdlog.h>
#include <cmath>
#include <unordered_set>
#include <vector>

using namespace entt::literals;

namespace game::world {

namespace {

constexpr entt::id_type RULE_SOIL_TILLED = game::defs::auto_tile_rule::SOIL_TILLED;
constexpr entt::id_type RULE_SOIL_WET    = game::defs::auto_tile_rule::SOIL_WET;

glm::ivec2 worldToTileCoord(glm::vec2 world_pos, glm::ivec2 tile_size) {
    if (tile_size.x <= 0 || tile_size.y <= 0) {
        return {-1, -1};
    }
    return glm::ivec2(
        static_cast<int>(std::floor(world_pos.x / static_cast<float>(tile_size.x))),
        static_cast<int>(std::floor(world_pos.y / static_cast<float>(tile_size.y)))
    );
}

void updateCropSprite(entt::registry& registry,
                      const game::factory::BlueprintManager& blueprint_manager,
                      entt::entity crop_entity,
                      entt::id_type crop_id,
                      game::defs::GrowthStage stage) {
    if (crop_id == entt::null || !blueprint_manager.hasCropBlueprint(crop_id)) {
        return;
    }
    const auto& blueprint = blueprint_manager.getCropBlueprint(crop_id);
    const game::factory::SpriteBlueprint* sprite_bp = nullptr;
    for (const auto& stage_bp : blueprint.stages_) {
        if (stage_bp.stage_ == stage) {
            sprite_bp = &stage_bp.sprite_;
            break;
        }
    }
    if (!sprite_bp) {
        return;
    }

    if (auto* sprite_comp = registry.try_get<engine::component::SpriteComponent>(crop_entity)) {
        sprite_comp->sprite_ = engine::component::Sprite(sprite_bp->path_, sprite_bp->src_rect_, sprite_bp->flip_horizontal_);
        sprite_comp->size_ = sprite_bp->dst_size_;
        sprite_comp->pivot_ = sprite_bp->pivot_;
    }
}

void advanceCropOneDay(entt::registry& registry,
                       const game::factory::BlueprintManager& blueprint_manager,
                       entt::entity crop_entity,
                       bool watered) {
    auto* crop = registry.try_get<game::component::CropComponent>(crop_entity);
    if (!crop) {
        return;
    }

    if (crop->current_stage_ == game::defs::GrowthStage::Mature) {
        return;
    }

    crop->stage_countdown_ -= watered ? 2 : 1;
    if (crop->stage_countdown_ > 0) {
        return;
    }

    const entt::id_type crop_id = game::defs::cropTypeToId(crop->type_);
    if (crop_id == entt::null || !blueprint_manager.hasCropBlueprint(crop_id)) {
        crop->current_stage_ = game::defs::GrowthStage::Mature;
        crop->stage_countdown_ = 0;
        return;
    }
    const auto& blueprint = blueprint_manager.getCropBlueprint(crop_id);

    int current_index = -1;
    for (std::size_t i = 0; i < blueprint.stages_.size(); ++i) {
        if (blueprint.stages_[i].stage_ == crop->current_stage_) {
            current_index = static_cast<int>(i);
            break;
        }
    }
    if (current_index < 0) {
        return;
    }

    const int next_index = current_index + 1;
    if (next_index >= static_cast<int>(blueprint.stages_.size())) {
        crop->current_stage_ = game::defs::GrowthStage::Mature;
        crop->stage_countdown_ = 0;
    } else {
        const auto& next_stage = blueprint.stages_[next_index];
        crop->current_stage_ = next_stage.stage_;
        crop->stage_countdown_ = next_stage.days_required_;
    }

    updateCropSprite(registry, blueprint_manager, crop_entity, crop_id, crop->current_stage_);

    if (auto* render = registry.try_get<engine::component::RenderComponent>(crop_entity)) {
        render->layer_ = game::defs::render_layer::ACTOR;
    }
}

} // namespace

bool peekDynamicSnapshotFlags(const RegistrySnapshot& snapshot, DynamicSnapshotFlags& out_flags) {
    if (!snapshot.valid || snapshot.data.empty()) {
        return false;
    }

    auto tmp = snapshot;
    tmp.resetRead();

    std::uint32_t version{};
    tmp(version);
    if (version == 1) {
        out_flags = DynamicSnapshotFlags::IncludeResources;
        return true;
    }
    if (version != MAP_DYNAMIC_SNAPSHOT_VERSION) {
        return false;
    }

    std::uint32_t flags{};
    tmp(flags);
    out_flags = static_cast<DynamicSnapshotFlags>(flags);
    return true;
}

void writeDynamicSnapshot(entt::registry& registry,
                          entt::id_type map_id,
                          RegistrySnapshot& out,
                          DynamicSnapshotFlags flags) {
    out.clear();
    out.valid = true;

    out(std::uint32_t{MAP_DYNAMIC_SNAPSHOT_VERSION});
    out(static_cast<std::uint32_t>(flags));

    const auto tilled_view = registry.view<
        engine::component::AutoTileComponent,
        game::component::MapId,
        engine::component::TransformComponent,
        engine::component::SpriteComponent,
        engine::component::RenderComponent
    >(entt::exclude<engine::component::NeedRemoveTag>);

    std::vector<entt::entity> tilled_entities;
    for (auto entity : tilled_view) {
        const auto& map = tilled_view.get<game::component::MapId>(entity);
        if (map.id_ != map_id) {
            continue;
        }
        const auto& auto_tile = tilled_view.get<engine::component::AutoTileComponent>(entity);
        if (auto_tile.rule_id_ != RULE_SOIL_TILLED) {
            continue;
        }
        tilled_entities.push_back(entity);
    }

    entt::snapshot{registry}
        .get<game::component::MapId>(out, tilled_entities.begin(), tilled_entities.end())
        .get<engine::component::TransformComponent>(out, tilled_entities.begin(), tilled_entities.end())
        .get<engine::component::SpriteComponent>(out, tilled_entities.begin(), tilled_entities.end())
        .get<engine::component::RenderComponent>(out, tilled_entities.begin(), tilled_entities.end())
        .get<engine::component::AutoTileComponent>(out, tilled_entities.begin(), tilled_entities.end());

    const auto wet_view = registry.view<
        engine::component::AutoTileComponent,
        game::component::MapId,
        engine::component::TransformComponent,
        engine::component::SpriteComponent,
        engine::component::RenderComponent
    >(entt::exclude<engine::component::NeedRemoveTag>);

    std::vector<entt::entity> wet_entities;
    for (auto entity : wet_view) {
        const auto& map = wet_view.get<game::component::MapId>(entity);
        if (map.id_ != map_id) {
            continue;
        }
        const auto& auto_tile = wet_view.get<engine::component::AutoTileComponent>(entity);
        if (auto_tile.rule_id_ != RULE_SOIL_WET) {
            continue;
        }
        wet_entities.push_back(entity);
    }

    entt::snapshot{registry}
        .get<game::component::MapId>(out, wet_entities.begin(), wet_entities.end())
        .get<engine::component::TransformComponent>(out, wet_entities.begin(), wet_entities.end())
        .get<engine::component::SpriteComponent>(out, wet_entities.begin(), wet_entities.end())
        .get<engine::component::RenderComponent>(out, wet_entities.begin(), wet_entities.end())
        .get<engine::component::AutoTileComponent>(out, wet_entities.begin(), wet_entities.end());

    const auto crop_view = registry.view<
        game::component::CropComponent,
        game::component::MapId,
        engine::component::TransformComponent,
        engine::component::SpriteComponent,
        engine::component::RenderComponent
    >(entt::exclude<engine::component::NeedRemoveTag>);

    std::vector<entt::entity> crop_entities;
    for (auto entity : crop_view) {
        const auto& map = crop_view.get<game::component::MapId>(entity);
        if (map.id_ != map_id) {
            continue;
        }
        crop_entities.push_back(entity);
    }

    entt::snapshot{registry}
        .get<game::component::MapId>(out, crop_entities.begin(), crop_entities.end())
        .get<engine::component::TransformComponent>(out, crop_entities.begin(), crop_entities.end())
        .get<engine::component::SpriteComponent>(out, crop_entities.begin(), crop_entities.end())
        .get<engine::component::RenderComponent>(out, crop_entities.begin(), crop_entities.end())
        .get<game::component::CropComponent>(out, crop_entities.begin(), crop_entities.end());

    if (!hasFlag(flags, DynamicSnapshotFlags::IncludeResources)) {
        return;
    }

    const auto resource_view = registry.view<
        game::component::ResourceNodeComponent,
        game::component::MapId,
        engine::component::TransformComponent,
        engine::component::SpriteComponent,
        engine::component::RenderComponent
    >(entt::exclude<engine::component::NeedRemoveTag>);

    std::vector<entt::entity> resource_entities;
    for (auto entity : resource_view) {
        const auto& map = resource_view.get<game::component::MapId>(entity);
        if (map.id_ != map_id) {
            continue;
        }
        resource_entities.push_back(entity);
    }

    entt::snapshot{registry}
        .get<game::component::MapId>(out, resource_entities.begin(), resource_entities.end())
        .get<engine::component::TransformComponent>(out, resource_entities.begin(), resource_entities.end())
        .get<engine::component::SpriteComponent>(out, resource_entities.begin(), resource_entities.end())
        .get<engine::component::RenderComponent>(out, resource_entities.begin(), resource_entities.end())
        .get<game::component::ResourceNodeComponent>(out, resource_entities.begin(), resource_entities.end())
        .get<engine::component::AnimationComponent>(out, resource_entities.begin(), resource_entities.end())
        .get<engine::component::AABBCollider>(out, resource_entities.begin(), resource_entities.end())
        .get<engine::component::CircleCollider>(out, resource_entities.begin(), resource_entities.end())
        .get<engine::component::SpatialIndexTag>(out, resource_entities.begin(), resource_entities.end());
}

bool readDynamicSnapshot(entt::registry& registry, RegistrySnapshot& snapshot, DynamicSnapshotFlags* out_flags) {
    if (!snapshot.valid) {
        return false;
    }

    snapshot.resetRead();
    std::uint32_t version{};
    snapshot(version);

    DynamicSnapshotFlags flags = DynamicSnapshotFlags::IncludeResources;
    if (version == 1) {
        flags = DynamicSnapshotFlags::IncludeResources;
    } else if (version == MAP_DYNAMIC_SNAPSHOT_VERSION) {
        std::uint32_t raw_flags{};
        snapshot(raw_flags);
        flags = static_cast<DynamicSnapshotFlags>(raw_flags);
    } else {
        spdlog::error("MapSnapshotSerializer: RegistrySnapshot 版本不匹配，期望 {} 实际 {}",
                      MAP_DYNAMIC_SNAPSHOT_VERSION,
                      version);
        return false;
    }

    if (out_flags) {
        *out_flags = flags;
    }

    {
        entt::continuous_loader loader{registry};
        loader.get<game::component::MapId>(snapshot);
        loader.get<engine::component::TransformComponent>(snapshot);
        loader.get<engine::component::SpriteComponent>(snapshot);
        loader.get<engine::component::RenderComponent>(snapshot);
        loader.get<engine::component::AutoTileComponent>(snapshot);
        loader.orphans();
    }

    {
        entt::continuous_loader loader{registry};
        loader.get<game::component::MapId>(snapshot);
        loader.get<engine::component::TransformComponent>(snapshot);
        loader.get<engine::component::SpriteComponent>(snapshot);
        loader.get<engine::component::RenderComponent>(snapshot);
        loader.get<engine::component::AutoTileComponent>(snapshot);
        loader.orphans();
    }

    {
        entt::continuous_loader loader{registry};
        loader.get<game::component::MapId>(snapshot);
        loader.get<engine::component::TransformComponent>(snapshot);
        loader.get<engine::component::SpriteComponent>(snapshot);
        loader.get<engine::component::RenderComponent>(snapshot);
        loader.get<game::component::CropComponent>(snapshot);
        loader.orphans();
    }

    if (hasFlag(flags, DynamicSnapshotFlags::IncludeResources)) {
        entt::continuous_loader loader{registry};
        loader.get<game::component::MapId>(snapshot);
        loader.get<engine::component::TransformComponent>(snapshot);
        loader.get<engine::component::SpriteComponent>(snapshot);
        loader.get<engine::component::RenderComponent>(snapshot);
        loader.get<game::component::ResourceNodeComponent>(snapshot);
        loader.get<engine::component::AnimationComponent>(snapshot);
        loader.get<engine::component::AABBCollider>(snapshot);
        loader.get<engine::component::CircleCollider>(snapshot);
        loader.get<engine::component::SpatialIndexTag>(snapshot);
        loader.orphans();
    }

    return true;
}

void simulateOfflineDays(entt::registry& registry,
                         const game::factory::BlueprintManager& blueprint_manager,
                         std::uint32_t days) {
    const glm::ivec2 tile_size{static_cast<int>(defs::TILE_SIZE), static_cast<int>(defs::TILE_SIZE)};

    for (std::uint32_t day = 0; day < days; ++day) {
        std::unordered_set<glm::ivec2> wet_set;
        auto wet_view = registry.view<engine::component::AutoTileComponent, engine::component::TransformComponent>();
        for (auto entity : wet_view) {
            const auto& auto_tile = wet_view.get<engine::component::AutoTileComponent>(entity);
            if (auto_tile.rule_id_ != RULE_SOIL_WET) {
                continue;
            }
            const auto& transform = wet_view.get<engine::component::TransformComponent>(entity);
            wet_set.insert(worldToTileCoord(transform.position_, tile_size));
        }

        auto crop_view = registry.view<game::component::CropComponent, engine::component::TransformComponent>();
        for (auto entity : crop_view) {
            const auto& transform = crop_view.get<engine::component::TransformComponent>(entity);
            const bool watered = wet_set.contains(worldToTileCoord(transform.position_, tile_size));
            advanceCropOneDay(registry, blueprint_manager, entity, watered);
        }

        // 每日浇水消失：移除所有 soil_wet 自动图块
        std::vector<entt::entity> to_remove;
        for (auto entity : wet_view) {
            const auto& auto_tile = wet_view.get<engine::component::AutoTileComponent>(entity);
            if (auto_tile.rule_id_ == RULE_SOIL_WET) {
                to_remove.push_back(entity);
            }
        }
        for (auto entity : to_remove) {
            registry.destroy(entity);
        }
    }
}

} // namespace game::world
