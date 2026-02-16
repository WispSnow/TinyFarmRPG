#pragma once
#include "engine/utils/math.h"
#include "game/defs/crop_defs.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <entt/entity/entity.hpp>

namespace game::factory {

/// @brief 精灵蓝图, 用于创建精灵组件
struct SpriteBlueprint {
    entt::id_type id_{entt::null};
    std::string path_;
    engine::utils::Rect src_rect_{};
    glm::vec2 dst_size_{0.0f};
    glm::vec2 pivot_{0.0f};
    bool flip_horizontal_{false};
};

/// @brief 声音蓝图, 用于创建声音组件
struct SoundTriggerBlueprint {
    entt::id_type sound_id_{entt::null};
    float probability_{1.0f};
    float cooldown_seconds_{0.0f};
};

struct SoundBlueprint {
    std::unordered_map<entt::id_type, SoundTriggerBlueprint> triggers_;
};

/// @brief 单一动画的蓝图，多个蓝图构成的关联容器即可用于创建动画组件
struct AnimationBlueprint {
    std::string name_;
    entt::id_type texture_id_{entt::null};
    std::string texture_path_;
    float ms_per_frame_{0.0f};
    glm::vec2 position_{0.0f};
    glm::vec2 src_size_{0.0f};
    glm::vec2 dst_size_{0.0f};
    glm::vec2 pivot_{0.0f};
    std::vector<int> frames_;   ///< @brief 动画帧索引数组
    std::unordered_map<int, entt::id_type> events_;   ///< @brief 动画事件，键为帧索引，值为事件ID
    bool flip_horizontal_{false};
};

struct ActorBlueprint {
    std::string name_;
    std::string description_;
    float speed_{0.0f};
    SpriteBlueprint sprite_;
    SoundBlueprint sounds_;
    std::unordered_map<entt::id_type, AnimationBlueprint> animations_;
    float wander_radius_{0.0f};                  ///< @brief 随机游走半径（0表示不游走）
    entt::id_type dialogue_id_{entt::null};      ///< @brief 对话ID
    float interact_distance_{80.0f};             ///< @brief 触发对话距离
};

struct AnimalBlueprint {
    std::string name_;
    std::string description_;
    float speed_{0.0f};
    SpriteBlueprint sprite_;
    SoundBlueprint sounds_;
    std::unordered_map<entt::id_type, AnimationBlueprint> animations_;
    float wander_radius_{0.0f};
    entt::id_type dialogue_id_{entt::null};
    float interact_distance_{80.0f};
    bool sleep_at_night_{true};
};

/// @brief 作物生长阶段蓝图
struct CropStageBlueprint {
    game::defs::GrowthStage stage_{game::defs::GrowthStage::Unknown};
    int days_required_{0};
    SpriteBlueprint sprite_;
};

/// @brief 作物蓝图，用于创建作物实体
struct CropBlueprint {
    game::defs::CropType type_{game::defs::CropType::Unknown};
    std::vector<CropStageBlueprint> stages_;
    entt::id_type harvest_item_id_{entt::null};
};

}   // namespace game::factory
