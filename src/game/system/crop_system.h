#pragma once
#include "game/component/crop_component.h"
#include "engine/utils/events.h"
#include <entt/entity/fwd.hpp>
#include <entt/signal/fwd.hpp>

namespace engine::spatial {
    class SpatialIndexManager;
}

namespace game::factory {
    class BlueprintManager;
    struct CropBlueprint;
}

namespace game::system {

/**
 * @brief 作物生长系统
 * 
 * 负责监听 DayChangedEvent，每日更新所有作物的生长阶段。
 * 根据浇水状态调整生长速度。
 */
class CropSystem {
    entt::registry& registry_;
    entt::dispatcher& dispatcher_;
    engine::spatial::SpatialIndexManager& spatial_index_;
    const game::factory::BlueprintManager& blueprint_manager_;

public:
    CropSystem(entt::registry& registry, 
               entt::dispatcher& dispatcher,
               engine::spatial::SpatialIndexManager& spatial_index,
               const game::factory::BlueprintManager& blueprint_manager);
    ~CropSystem();

private:
    /**
     * @brief 监听 DayChangedEvent，每日更新作物生长
     */
    void onDayChanged(const engine::utils::DayChangedEvent& event);

    /**
     * @brief 更新单个作物的生长状态
     * @param crop_entity 作物实体
     */
    void updateCropGrowth(entt::entity crop_entity);

    /**
     * @brief 检查指定位置是否已浇水
     * @param world_pos 世界位置
     * @return 如果该位置的湿润层实体存在，返回 true
     */
    bool isWatered(const glm::vec2& world_pos) const;

    /**
     * @brief 推进作物到下一生长阶段
     * @param crop_entity 作物实体
     * @param crop_component 作物组件引用
     */
    void advanceToNextStage(entt::entity crop_entity, game::component::CropComponent& crop_component);

    /**
     * @brief 更新作物的贴图
     * @param crop_entity 作物实体
     * @param crop_component 作物组件引用
     * @param crop_blueprint 对应的作物蓝图
     */
    void updateCropSprite(entt::entity crop_entity,
                          const game::component::CropComponent& crop_component,
                          const game::factory::CropBlueprint& crop_blueprint);
};

} // namespace game::system

