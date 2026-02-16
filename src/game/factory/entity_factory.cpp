#include "entity_factory.h"
#include "blueprint_manager.h"
#include "game/component/state_component.h"
#include "game/component/action_sound_component.h"
#include "game/component/actor_component.h"
#include "game/component/crop_component.h"
#include "game/component/map_component.h"
#include "game/component/tags.h"
#include "game/component/inventory_component.h"
#include "game/component/hotbar_component.h"
#include "game/component/npc_component.h"
#include "game/component/pickup_component.h"
#include "game/world/world_state.h"
#include "engine/utils/math.h"
#include "game/defs/constants.h"
#include "game/defs/render_layers.h"
#include "game/defs/spatial_layers.h"
#include "engine/component/transform_component.h"
#include "engine/component/sprite_component.h"
#include "engine/component/animation_component.h"
#include "engine/component/collider_component.h"
#include "engine/component/audio_component.h"
#include "engine/component/velocity_component.h"
#include "engine/component/render_component.h"
#include "engine/component/name_component.h"
#include "engine/component/tags.h"
#include "engine/component/auto_tile_component.h"
#include "engine/resource/auto_tile_library.h"
#include "engine/spatial/spatial_index_manager.h"
#include <entt/entity/registry.hpp>
#include <spdlog/spdlog.h>

using namespace entt::literals;

namespace game::factory {

namespace {

/// @brief 将动画蓝图转换为运行时动画数据
engine::component::Animation toRuntimeAnimation(const AnimationBlueprint& animation_blueprint, bool loop = true) {
    const glm::vec2 pivot = animation_blueprint.pivot_;
    const glm::vec2 position = animation_blueprint.position_;
    const glm::vec2 src_size = animation_blueprint.src_size_;
    const glm::vec2 dst_size = animation_blueprint.dst_size_;
    const auto& frame_indices = animation_blueprint.frames_;
    std::vector<engine::component::AnimationFrame> frames;
    for (const auto& frame_index : frame_indices) {
        engine::utils::Rect source_rect = engine::utils::Rect{position, src_size};
        source_rect.pos.x += frame_index * src_size.x;
        frames.emplace_back(source_rect, animation_blueprint.ms_per_frame_);
    }
    engine::component::Animation animation{};
    animation.name_ = animation_blueprint.name_;
    animation.texture_id_ = animation_blueprint.texture_id_;
    animation.texture_path_ = animation_blueprint.texture_path_;
    animation.pivot_ = pivot;
    animation.dst_size_ = dst_size;
    animation.frames_ = std::move(frames);
    animation.events_ = animation_blueprint.events_;
    animation.loop_ = loop;
    animation.flip_horizontal_ = animation_blueprint.flip_horizontal_;
    return animation;
}

void attachCurrentMapId(entt::registry& registry, entt::entity entity) {
    auto** world_state_ptr = registry.ctx().find<game::world::WorldState*>();
    if (!world_state_ptr || !*world_state_ptr) {
        return;
    }

    const entt::id_type current_map = (*world_state_ptr)->getCurrentMap();
    if (current_map == entt::null) {
        return;
    }

    registry.emplace_or_replace<game::component::MapId>(entity, current_map);
}

}   // anonymous namespace

entt::entity EntityFactory::createMobBase(entt::id_type name_id,
                                         const std::string& display_name,
                                         float speed,
                                         const SpriteBlueprint& sprite,
                                         const SoundBlueprint& sounds,
                                         const std::unordered_map<entt::id_type, AnimationBlueprint>& animations,
                                         const glm::vec2& position) {
    entt::entity entity = registry_.create();

    addTransformComponent(entity, position);
    addSpriteComponent(entity, sprite);
    addAnimationComponent(entity, animations, "idle_down"_hs);
    addAudioComponent(entity, sounds);

    registry_.emplace<game::component::ActorComponent>(entity, speed, game::defs::Tool::None);
    registry_.emplace<engine::component::CircleCollider>(entity, game::defs::ACTOR_COLLIDER_RADIUS, glm::vec2(0.0f, -5.0f));
    registry_.emplace<engine::component::VelocityComponent>(entity, glm::vec2(0.0f, 0.0f));
    registry_.emplace<engine::component::RenderComponent>(entity);
    registry_.emplace<engine::component::NameComponent>(entity, name_id, display_name);
    registry_.emplace<game::component::StateComponent>(entity);
    registry_.emplace<game::component::StateDirtyTag>(entity);
    registry_.emplace<engine::component::SpatialIndexTag>(entity);

    return entity;
}

entt::entity EntityFactory::createActor(const entt::id_type actor_name_id, const glm::vec2& position) {
    if (!blueprint_manager_.hasActorBlueprint(actor_name_id)) {
        spdlog::error("未找到角色蓝图: {}", actor_name_id);
        return entt::null;
    }
    const auto& blueprint = blueprint_manager_.getActorBlueprint(actor_name_id);
    entt::entity entity = createMobBase(actor_name_id,
                                        blueprint.name_,
                                        blueprint.speed_,
                                        blueprint.sprite_,
                                        blueprint.sounds_,
                                        blueprint.animations_,
                                        position);
    if (actor_name_id == "player"_hs) {
        registry_.emplace<game::component::PlayerTag>(entity);
        // 为玩家添加物品栏和快捷栏组件
        auto& inventory = registry_.emplace<game::component::InventoryComponent>(entity);
        auto& hotbar = registry_.emplace<game::component::HotbarComponent>(entity);

        // 初始化起始工具
        const std::vector<entt::id_type> start_tools = {
            "tool_hoe"_hs,
            "tool_watering_can"_hs,
            "tool_pickaxe"_hs,
            "tool_axe"_hs,
            "tool_sickle"_hs
        };

        for (std::size_t i = 0; i < start_tools.size(); ++i) {
            // Fill inventory
            if (i < inventory.slots_.size()) {
                inventory.slots_[i].item_id_ = start_tools[i];
                inventory.slots_[i].count_ = 1;
            }
            // Map to hotbar
            if (i < game::component::HotbarComponent::SLOT_COUNT) {
                hotbar.setSlotMapping(static_cast<int>(i), static_cast<int>(i));
            }
        }
    }
    else {
        registry_.emplace<game::component::NPCTag>(entity);
        if (blueprint.wander_radius_ > 0.0f) {
            game::component::WanderComponent wander{};
            wander.home_position_ = position;
            wander.radius_ = blueprint.wander_radius_;
            registry_.emplace<game::component::WanderComponent>(entity, wander);
        }
        if (blueprint.dialogue_id_ != entt::null) {
            registry_.emplace<game::component::DialogueComponent>(entity, game::component::DialogueComponent{
                blueprint.dialogue_id_,
                blueprint.interact_distance_
            });
        }
    }
    attachCurrentMapId(registry_, entity);
    return entity;
}

entt::entity EntityFactory::createAnimal(const entt::id_type animal_name_id, const glm::vec2& position) {
    if (!blueprint_manager_.hasAnimalBlueprint(animal_name_id)) {
        spdlog::error("未找到动物蓝图: {}", animal_name_id);
        return entt::null;
    }
    const auto& blueprint = blueprint_manager_.getAnimalBlueprint(animal_name_id);
    entt::entity entity = createMobBase(animal_name_id,
                                        blueprint.name_,
                                        blueprint.speed_,
                                        blueprint.sprite_,
                                        blueprint.sounds_,
                                        blueprint.animations_,
                                        position);
    auto& state_component = registry_.get<game::component::StateComponent>(entity);
    registry_.emplace<game::component::NPCTag>(entity);
    registry_.emplace<game::component::AnimalTag>(entity);

    if (!blueprint.sounds_.triggers_.empty()) {
        game::component::ActionSoundComponent action_sounds{};
        action_sounds.last_action_ = state_component.action_;
        action_sounds.triggers_.reserve(blueprint.sounds_.triggers_.size());
        for (const auto& [trigger_id, trigger] : blueprint.sounds_.triggers_) {
            action_sounds.triggers_.emplace(trigger_id,
                                            game::component::ActionSoundTriggerConfig{
                                                trigger.probability_,
                                                trigger.cooldown_seconds_});
        }
        registry_.emplace<game::component::ActionSoundComponent>(entity, std::move(action_sounds));
    }

    game::component::WanderComponent wander{};
    wander.home_position_ = position;
    wander.radius_ = blueprint.wander_radius_;
    registry_.emplace<game::component::WanderComponent>(entity, wander);

    registry_.emplace<game::component::SleepRoutine>(entity, game::component::SleepRoutine{blueprint.sleep_at_night_, false});

    // 初始化动物行为定时器
    game::component::AnimalBehaviorState behavior{};
    behavior.eat_cooldown_timer_ = engine::utils::randomFloat(4.0f, 8.0f);
    behavior.eat_duration_ = engine::utils::randomFloat(1.2f, 2.0f);
    behavior.eat_interval_min_ = 4.0f;
    behavior.eat_interval_max_ = 8.0f;
    registry_.emplace<game::component::AnimalBehaviorState>(entity, behavior);

    if (blueprint.dialogue_id_ != entt::null) {
        registry_.emplace<game::component::DialogueComponent>(entity, game::component::DialogueComponent{
            blueprint.dialogue_id_,
            blueprint.interact_distance_
        });
    }

    attachCurrentMapId(registry_, entity);
    return entity;
}

// --- 组件创建函数 ---

void EntityFactory::addTransformComponent(entt::entity entity, glm::vec2 position, glm::vec2 scale, float rotation){
    registry_.emplace<engine::component::TransformComponent>(entity, position, scale, rotation);
    registry_.emplace<engine::component::TransformDirtyTag>(entity);
}

void EntityFactory::addSpriteComponent(entt::entity entity, const SpriteBlueprint& sprite) {
    registry_.emplace<engine::component::SpriteComponent>(entity,
        engine::component::Sprite{sprite.path_, 
            sprite.src_rect_,
            sprite.flip_horizontal_
        },
        sprite.dst_size_,
        sprite.pivot_
    );
}

/// @brief 单个动画组件添加（组件中只包含一个动画），用于创建特效
void EntityFactory::addOneAnimationComponent(entt::entity entity,      
    const AnimationBlueprint& animation_blueprint, 
    entt::id_type animation_id,
    bool loop) {

    // 通过工具函数解析动画蓝图，创建动画对象
    auto animation = toRuntimeAnimation(animation_blueprint, loop);
    // 创建动画map容器
    std::unordered_map<entt::id_type, engine::component::Animation> animations;
    animations.emplace(animation_id, std::move(animation));
    // 通过动画map容器创建动画组件
    registry_.emplace<engine::component::AnimationComponent>(entity, std::move(animations), animation_id);
}

/// @brief 正常动画组件添加（多个动画）
void EntityFactory::addAnimationComponent(entt::entity entity,
    const std::unordered_map<entt::id_type, AnimationBlueprint>& animation_blueprints,
    entt::id_type default_animation_id){
    std::unordered_map<entt::id_type, engine::component::Animation> animations;
    for (const auto& [animation_id, animation_blueprint] : animation_blueprints) {
        auto animation = toRuntimeAnimation(animation_blueprint, true);
        animations.emplace(animation_id, std::move(animation));
    }
    registry_.emplace<engine::component::AnimationComponent>(entity, std::move(animations), default_animation_id);
}

/// @brief 音频组件添加
void EntityFactory::addAudioComponent(entt::entity entity, const SoundBlueprint& sounds) {
    if (sounds.triggers_.empty()) {
        return;
    }

    std::unordered_map<entt::id_type, entt::id_type> sound_map;
    sound_map.reserve(sounds.triggers_.size());
    for (const auto& [trigger_id, trigger] : sounds.triggers_) {
        if (trigger.sound_id_ == entt::null) {
            continue;
        }
        sound_map.emplace(trigger_id, trigger.sound_id_);
    }

    if (sound_map.empty()) {
        return;
    }

    registry_.emplace<engine::component::AudioComponent>(entity, std::move(sound_map));
}

/// @brief 作物组件添加
void EntityFactory::addCropComponent(entt::entity entity, game::defs::CropType crop_type, std::uint32_t planted_day, int initial_countdown) {
    // 添加CropComponent（初始化stage_countdown_为第一个阶段的days_required_）
    registry_.emplace<game::component::CropComponent>(entity,
        crop_type,
        planted_day,
        initial_countdown
    );
}

entt::entity EntityFactory::createCrop(const entt::id_type crop_type_id, const glm::vec2& position, std::uint32_t planted_day) {
    if (!blueprint_manager_.hasCropBlueprint(crop_type_id)) {
        spdlog::error("未找到作物蓝图: {}", crop_type_id);
        return entt::null;
    }
    
    const auto& crop_blueprint = blueprint_manager_.getCropBlueprint(crop_type_id);
    
    if (crop_blueprint.stages_.empty()) {
        spdlog::error("作物蓝图 {} 没有生长阶段", crop_type_id);
        return entt::null;
    }
    
    // 获取第一个阶段（种子阶段）的配置
    const auto& seed_stage = crop_blueprint.stages_[0];
    if (seed_stage.stage_ != game::defs::GrowthStage::Seed) {
        spdlog::warn("作物蓝图 {} 的第一个阶段不是种子阶段", crop_type_id);
    }
    
    // 创建作物实体
    entt::entity crop_entity = registry_.create();
    
    // 添加TransformComponent
    addTransformComponent(crop_entity, position);
    
    // 添加SpriteComponent（使用种子阶段的贴图）
    addSpriteComponent(crop_entity, seed_stage.sprite_);
    
    // 添加CropComponent
    addCropComponent(crop_entity, crop_blueprint.type_, planted_day, seed_stage.days_required_);
    
    // 添加RenderComponent（种子的图层在角色下方，生长后再调整为与角色同图层）
    registry_.emplace<engine::component::RenderComponent>(crop_entity, game::defs::render_layer::CROP_SEED);
    
    // 将作物实体添加到空间索引（如果空间索引管理器可用）
    if (spatial_index_manager_) {
        spatial_index_manager_->addTileEntityAtWorldPos(position, crop_entity, game::defs::spatial_layer::CROP);
    }
    
    spdlog::info("成功创建作物实体: {}，类型: {}，种植天数: {}", 
        static_cast<std::uint32_t>(crop_entity), crop_type_id, planted_day);
    
    attachCurrentMapId(registry_, crop_entity);
    return crop_entity;
}

entt::entity EntityFactory::createResourcePickup(entt::id_type item_id,
                                                 const engine::component::Sprite& sprite,
                                                 const glm::vec2& start_pos,
                                                 const glm::vec2& target_pos,
                                                 float flight_time,
                                                 float arc_height) {
    auto entity = registry_.create();
    constexpr float SCALE = 0.75f;      // 缩放比例，避免地图上的资源体积太大
    addTransformComponent(entity, start_pos, glm::vec2(SCALE, SCALE), 0.0f);
    registry_.emplace<engine::component::SpriteComponent>(entity,
                                                          sprite,
                                                          glm::vec2(0.0f, 0.0f),
                                                          glm::vec2(0.5f, 0.5f));
    registry_.emplace<engine::component::AABBCollider>(entity, sprite.src_rect_.size * SCALE, glm::vec2(0.0f, 0.0f));
    registry_.emplace<engine::component::RenderComponent>(entity);

    game::component::PickupComponent pickup{};
    pickup.item_id_ = item_id;
    pickup.count_ = 1;
    pickup.pickup_delay_ = flight_time;
    registry_.emplace<game::component::PickupComponent>(entity, pickup);

    game::component::ScatterMotionComponent scatter{};
    scatter.start_pos_ = start_pos;
    scatter.target_pos_ = target_pos;
    scatter.duration_ = flight_time;
    scatter.arc_height_ = arc_height;
    registry_.emplace<game::component::ScatterMotionComponent>(entity, scatter);

    attachCurrentMapId(registry_, entity);
    return entity;
}

entt::entity EntityFactory::createSoilTile(const glm::vec2& position, entt::id_type rule_id, entt::id_type layer_id) {
    if (!auto_tile_library_) {
        spdlog::error("AutoTileLibrary 未初始化，无法创建自动图块: {}", rule_id);
        return entt::null;
    }
    const auto* rule = auto_tile_library_->getRule(rule_id);
    if (!rule) {
        spdlog::error("自动图块规则未找到: {}", rule_id);
        return entt::null;
    }

    // 选择默认 mask（优先 0，否则取第一个已定义的 mask）
    uint8_t mask = 0;
    if (!rule->mask_defined_.test(0)) {
        for (uint16_t i = 1; i < rule->mask_defined_.size(); ++i) {
            if (rule->mask_defined_.test(i)) {
                mask = static_cast<uint8_t>(i);
                break;
            }
        }
    }
    const auto rect = rule->lookup_table_[mask];
    engine::component::Sprite sprite(rule->texture_id_, rect);

    auto entity = registry_.create();
    addTransformComponent(entity, position);
    registry_.emplace<engine::component::SpriteComponent>(entity, sprite);
    registry_.emplace<engine::component::AutoTileComponent>(entity, rule_id, mask);
    registry_.emplace<engine::component::RenderComponent>(entity);

    if (spatial_index_manager_) {
        spatial_index_manager_->addTileEntityAtWorldPos(position, entity, layer_id);
    }

    attachCurrentMapId(registry_, entity);
    return entity;
}

} // namespace game::factory
