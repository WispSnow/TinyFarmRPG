#include "crop_system.h"
#include "game/component/crop_component.h"
#include "game/defs/render_layers.h"
#include "game/defs/spatial_layers.h"
#include "game/factory/blueprint_manager.h"
#include "engine/utils/events.h"
#include "engine/spatial/spatial_index_manager.h"
#include "engine/component/auto_tile_component.h"
#include "engine/component/transform_component.h"
#include "engine/component/sprite_component.h"
#include "engine/component/render_component.h"
#include "engine/component/tags.h"
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <entt/core/hashed_string.hpp>
#include <spdlog/spdlog.h>
#include <glm/vec2.hpp>

using namespace entt::literals;

namespace game::system {

namespace {
    constexpr entt::id_type RULE_SOIL_WET = game::defs::auto_tile_rule::SOIL_WET;

    [[nodiscard]] int findStageIndex(const std::vector<game::factory::CropStageBlueprint>& stages,
                                     game::defs::GrowthStage stage) {
        for (std::size_t i = 0; i < stages.size(); ++i) {
            if (stages[i].stage_ == stage) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }
}

CropSystem::CropSystem(entt::registry& registry,
                       entt::dispatcher& dispatcher,
                       engine::spatial::SpatialIndexManager& spatial_index,
                       const game::factory::BlueprintManager& blueprint_manager)
    : registry_(registry),
      dispatcher_(dispatcher),
      spatial_index_(spatial_index),
      blueprint_manager_(blueprint_manager) {
    dispatcher_.sink<engine::utils::DayChangedEvent>().connect<&CropSystem::onDayChanged>(this);
}

CropSystem::~CropSystem() {
    dispatcher_.disconnect(this);
}

void CropSystem::onDayChanged(const engine::utils::DayChangedEvent& event) {
    spdlog::info("新的一天开始，更新所有作物生长状态，Day {}", event.day_);
    
    // 遍历所有有 CropComponent 的实体
    auto view = registry_.view<game::component::CropComponent>();
    for (auto entity : view) {
        updateCropGrowth(entity);
    }
    // 移除所有湿润耕地自动图块
    auto wet_view = registry_.view<engine::component::AutoTileComponent, engine::component::TransformComponent>();
    for (auto entity : wet_view) {
        const auto& auto_tile = wet_view.get<engine::component::AutoTileComponent>(entity);
        if (auto_tile.rule_id_ != RULE_SOIL_WET) {
            continue;
        }
        const auto& transform = wet_view.get<engine::component::TransformComponent>(entity);
        // 先从空间索引中移除实体
        spatial_index_.removeTileEntityAtWorldPos(transform.position_, entity);
        // 然后标记删除
        registry_.emplace_or_replace<engine::component::NeedRemoveTag>(entity);
        spdlog::info("移除湿润水自动图块，世界位置: {}, {}", transform.position_.x, transform.position_.y);
    }
}

void CropSystem::updateCropGrowth(entt::entity crop_entity) {
    auto& crop_component = registry_.get<game::component::CropComponent>(crop_entity);
    
    // 如果已经成熟，不再更新
    if (crop_component.current_stage_ == game::defs::GrowthStage::Mature) {
        return;
    }
    
    // 获取作物位置
    const auto& transform = registry_.get<engine::component::TransformComponent>(crop_entity);
    
    // 检查是否已浇水
    bool watered = isWatered(transform.position_);
    
    // 更新倒计时：未浇水时 -1，已浇水时 -2
    if (watered) {
        crop_component.stage_countdown_ -= 2;
        spdlog::trace("作物实体 {} 已浇水，倒计时 -2，剩余: {}", 
                     static_cast<std::uint32_t>(crop_entity), crop_component.stage_countdown_);
    } else {
        crop_component.stage_countdown_ -= 1;
        spdlog::trace("作物实体 {} 未浇水，倒计时 -1，剩余: {}", 
                     static_cast<std::uint32_t>(crop_entity), crop_component.stage_countdown_);
    }
    
    // 检查是否需要推进到下一阶段
    if (crop_component.stage_countdown_ <= 0) {
        advanceToNextStage(crop_entity, crop_component);
    }
}

bool CropSystem::isWatered(const glm::vec2& world_pos) const {
    return spatial_index_.getTileEntityAtWorldPos(world_pos, game::defs::spatial_layer::WET) != entt::null;
}

void CropSystem::advanceToNextStage(entt::entity crop_entity, game::component::CropComponent& crop_component) {
    // 获取作物蓝图
    auto crop_type_id = game::defs::cropTypeToId(crop_component.type_);
    if (crop_type_id == entt::null) {
        spdlog::error("作物实体 {} 的作物类型无效: {}", 
                     static_cast<std::uint32_t>(crop_entity), 
                     static_cast<int>(crop_component.type_));
        return;
    }
    
    if (!blueprint_manager_.hasCropBlueprint(crop_type_id)) {
        spdlog::error("未找到作物蓝图: {}", crop_type_id);
        return;
    }
    
    const auto& crop_blueprint = blueprint_manager_.getCropBlueprint(crop_type_id);
    
    // 确定当前阶段的索引
    const int current_stage_index = findStageIndex(crop_blueprint.stages_, crop_component.current_stage_);
    
    if (current_stage_index == -1) {
        spdlog::error("作物实体 {} 的当前阶段 {} 在蓝图中未找到", 
                     static_cast<std::uint32_t>(crop_entity),
                     static_cast<int>(crop_component.current_stage_));
        return;
    }
    
    // 推进到下一阶段
    int next_stage_index = current_stage_index + 1;
    if (next_stage_index >= static_cast<int>(crop_blueprint.stages_.size())) {
        // 已经是最后一个阶段，标记为成熟
        crop_component.current_stage_ = game::defs::GrowthStage::Mature;
        spdlog::info("作物实体 {} 已成熟", static_cast<std::uint32_t>(crop_entity));
    } else {
        // 推进到下一阶段
        const auto& next_stage = crop_blueprint.stages_[next_stage_index];
        crop_component.current_stage_ = next_stage.stage_;
        crop_component.stage_countdown_ = next_stage.days_required_;
        spdlog::info("作物实体 {} 推进到下一阶段: {}，倒计时重置为: {}", 
                    static_cast<std::uint32_t>(crop_entity),
                    static_cast<int>(next_stage.stage_),
                    next_stage.days_required_);
    }
    
    // 更新贴图
    updateCropSprite(crop_entity, crop_component, crop_blueprint);

    // 设置RenderComponent的图层为与角色同图层
    auto& render_component = registry_.get<engine::component::RenderComponent>(crop_entity);
    render_component.layer_ = game::defs::render_layer::ACTOR;
}

void CropSystem::updateCropSprite(entt::entity crop_entity,
                                  const game::component::CropComponent& crop_component,
                                  const game::factory::CropBlueprint& crop_blueprint) {
    // 查找当前阶段对应的贴图
    const int stage_index = findStageIndex(crop_blueprint.stages_, crop_component.current_stage_);
    
    if (stage_index < 0) {
        spdlog::error("作物实体 {} 的当前阶段 {} 没有对应的贴图配置", 
                     static_cast<std::uint32_t>(crop_entity),
                     static_cast<int>(crop_component.current_stage_));
        return;
    }
    const auto* sprite_blueprint = &crop_blueprint.stages_[static_cast<std::size_t>(stage_index)].sprite_;
    
    // 更新 SpriteComponent
    auto& sprite_component = registry_.get<engine::component::SpriteComponent>(crop_entity);
    
    // 创建新的 Sprite
    engine::component::Sprite new_sprite(
        sprite_blueprint->path_,
        sprite_blueprint->src_rect_,
        sprite_blueprint->flip_horizontal_
    );
    
    // 更新 sprite 和 size
    sprite_component.sprite_ = new_sprite;
    sprite_component.size_ = sprite_blueprint->dst_size_;
    sprite_component.pivot_ = sprite_blueprint->pivot_;
    
    spdlog::trace("作物实体 {} 的贴图已更新，阶段: {}", 
                 static_cast<std::uint32_t>(crop_entity),
                 static_cast<int>(crop_component.current_stage_));
}

} // namespace game::system
