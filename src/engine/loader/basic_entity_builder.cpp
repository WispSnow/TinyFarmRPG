#include "basic_entity_builder.h"
#include "level_loader.h"
#include "tiled_conventions.h"
#include "tiled_json_helpers.h"
#include "engine/core/context.h"
#include "engine/component/tilelayer_component.h"
#include "engine/component/name_component.h"
#include "engine/component/sprite_component.h"
#include "engine/component/auto_tile_component.h"
#include "engine/component/transform_component.h"
#include "engine/component/collider_component.h"
#include "engine/component/render_component.h"
#include "engine/component/tags.h"
#include "engine/resource/resource_manager.h"
#include <entt/entity/registry.hpp>
#include <spdlog/spdlog.h>

namespace engine::loader {

namespace {
using engine::loader::tiled::findMember;
using engine::loader::tiled::findStringMember;
using engine::loader::tiled::jsonFloatOr;
using engine::loader::tiled::jsonIntOr;
using engine::loader::tiled::jsonStringOr;

} // namespace

BasicEntityBuilder::BasicEntityBuilder(engine::loader::LevelLoader& level_loader, engine::core::Context& context, entt::registry& registry)
    : level_loader_(level_loader), context_(context), registry_(registry) {}

BasicEntityBuilder::~BasicEntityBuilder() = default;

void BasicEntityBuilder::reset() {
    object_json_ = nullptr;
    tile_info_ = nullptr;
    index_ = -1;
    entity_id_ = entt::null;
    position_ = glm::vec2(0.0f);
    dst_size_ = glm::vec2(0.0f);
    src_size_ = glm::vec2(0.0f);
}

BasicEntityBuilder* BasicEntityBuilder::configure(const nlohmann::json* object_json) {
    reset();
    if (!object_json) {
        spdlog::error("配置生成器时，object_json 不能为空");
        return this;
    }
    object_json_ = object_json;
    spdlog::trace("针对自定义形状配置生成器完成");
    return this;
}

BasicEntityBuilder* BasicEntityBuilder::configure(const nlohmann::json* object_json, const engine::component::TileInfo* tile_info) {
    reset();
    if (!object_json || !tile_info) {
        spdlog::error("配置生成器时，object_json 和 tile_info 不能为空");
        return this;
    }

    object_json_ = object_json;
    tile_info_ = tile_info;
    spdlog::trace("针对多图片集合的瓦片配置生成器完成");
    return this;
}

BasicEntityBuilder* BasicEntityBuilder::configure(int index, const engine::component::TileInfo* tile_info) {
    reset();
    if (!tile_info) {
        spdlog::error("配置生成器时，tile_info 不能为空");
        return this;
    }
    index_ = index;
    tile_info_ = tile_info;
    spdlog::trace("针对瓦片配置生成器完成");
    return this;
}

BasicEntityBuilder* BasicEntityBuilder::build() {
    if (!object_json_ && !tile_info_) {
        spdlog::error("object_json 和 tile_info 都为空，无法进行构建");
        return this;
    }

    // 按顺序构建各个组件
    buildBase();
    buildTransform();
    buildSprite();
    buildAutoTile();
    buildCollider();
    buildRender();
    buildAnimation();
    buildAudio();
    return this;
}

void BasicEntityBuilder::buildAutoTile() {
    spdlog::trace("构建AutoTile组件");
    if (!tile_info_ || !tile_info_->auto_tile_) {
        return;
    }
    const auto& auto_tile = tile_info_->auto_tile_.value();
    if (auto_tile.rule_id_ == entt::null) {
        spdlog::warn("AutoTile 数据缺少有效规则ID，跳过组件创建。");
        return;
    }
    registry_.emplace<engine::component::AutoTileComponent>(entity_id_, auto_tile.rule_id_, auto_tile.mask_);
}

entt::entity BasicEntityBuilder::getEntityID() {
    return entity_id_;
}

// --- 构建组件 ---
void BasicEntityBuilder::buildBase() {
    spdlog::trace("构建基础组件");
    // 创建一个实体并添加NameComponent组件
    entity_id_ = registry_.create();
    if (object_json_) {
        const std::string name = jsonStringOr(*object_json_, "name", "");
        if (name.empty()) {
            return;
        }
        entt::id_type name_id = entt::hashed_string(name.c_str());
        registry_.emplace<engine::component::NameComponent>(entity_id_, name_id, name);
        spdlog::trace("添加 NameComponent 组件，name: {}", name);
    }
}

void BasicEntityBuilder::buildTransform() {
    spdlog::trace("构建Transform组件");
    glm::vec2 scale = glm::vec2(1.0f);
    float rotation = 0.0f;
    
    // 对象层实体，位置、尺寸和旋转信息从 object_json_ 中获取
    if (object_json_) {
        position_ = glm::vec2(jsonFloatOr(*object_json_, "x", 0.0f), jsonFloatOr(*object_json_, "y", 0.0f));
        dst_size_ =
            glm::vec2(jsonFloatOr(*object_json_, "width", 0.0f), jsonFloatOr(*object_json_, "height", 0.0f));
        rotation = jsonFloatOr(*object_json_, "rotation", 0.0f);
    }

    // 瓦片层实体，通过index (Tiled瓦片层data数据的索引) 计算位置
    if (index_ >= 0) {
        auto map_size = level_loader_.getMapSize();
        auto tile_size = level_loader_.getTileSize();
        position_ = glm::vec2((index_ % map_size.x) * tile_size.x, 
                             (index_ / map_size.x) * tile_size.y);
    }

    // 添加 TransformComponent
    registry_.emplace<engine::component::TransformComponent>(entity_id_, position_, scale, rotation);
    registry_.emplace<engine::component::TransformDirtyTag>(entity_id_);
}


void BasicEntityBuilder::buildSprite() {
    spdlog::trace("构建Sprite组件");
    // 如果是自定义形状对象，或者位于碰撞层则不需要SpriteComponent
    if (!tile_info_ || level_loader_.getCurrentLayerName() == tiled::LAYER_NAME_COLLIDER ||
        level_loader_.getCurrentLayerName() == tiled::LAYER_NAME_SOLID) {
        return;
    }
    // 创建Sprite时候确保纹理加载
    auto& resource_manager = context_.getResourceManager();
    resource_manager.loadTexture(tile_info_->sprite_.texture_id_, tile_info_->sprite_.texture_path_);
    auto pivot = glm::vec2(0.0f, 0.0f);
    // 如果是对象层对象，则需要调整pivot
    if (object_json_) {
        pivot = glm::vec2(0.0f, 1.0f);
    }
    registry_.emplace<engine::component::SpriteComponent>(entity_id_, tile_info_->sprite_, dst_size_, pivot);
}

void BasicEntityBuilder::buildCollider() {
    spdlog::trace("构建Collider组件");
    glm::vec2 offset = glm::vec2(0.0f);
    engine::component::ColliderInfo collider_info{};
    // 如果是图片对象，从瓦片信息中获取碰撞器信息
    if (tile_info_ && tile_info_->collider_) {
        collider_info = tile_info_->collider_.value();
        offset = collider_info.rect_.pos + collider_info.rect_.size * 0.5f;
    // 如果非图片对象，则自定义形状中type为collider的物体属于碰撞器
    } else if (object_json_ && jsonStringOr(*object_json_, "type", "") == tiled::OBJECT_TYPE_COLLIDER) {
        collider_info.type_ = engine::component::ColliderType::RECT;
        collider_info.rect_ = engine::utils::Rect(glm::vec2(0.0f, 0.0f), dst_size_);
        offset = dst_size_ * 0.5f;
    } else {
        return;    // 没有碰撞器，直接返回
    }

    // 如果是对象层中的图片对象，则需要调整offset
    if (object_json_ && tile_info_) {
        offset -= glm::vec2(0.0f, dst_size_.y);
    }
    if (collider_info.type_ == engine::component::ColliderType::RECT) {
        registry_.emplace<engine::component::AABBCollider>(entity_id_, collider_info.rect_.size, offset);
    } else if (collider_info.type_ == engine::component::ColliderType::CIRCLE) {
        registry_.emplace<engine::component::CircleCollider>(entity_id_, collider_info.rect_.size.x * 0.5f, offset);
    }
    // 有碰撞盒的物体目前默认添加到空间索引标签
    registry_.emplace<engine::component::SpatialIndexTag>(entity_id_);
}

void BasicEntityBuilder::buildRender() {
    spdlog::trace("构建Render组件");
    if (level_loader_.getCurrentLayerName() == tiled::LAYER_NAME_COLLIDER ||
        level_loader_.getCurrentLayerName() == tiled::LAYER_NAME_SOLID) {
        return;
    }
    int layer = level_loader_.getCurrentLayer();    // 确定图层
    float depth = position_.y;                      // 确定深度（默认y坐标）
    registry_.emplace<engine::component::RenderComponent>(entity_id_, layer, depth);
}

void BasicEntityBuilder::buildAnimation() {
    spdlog::trace("构建Animation组件");
    // 如果存在动画，其信息已经解析并保存在tile_info_中
    if (tile_info_ && tile_info_->animation_) {
        // 创建动画map
        std::unordered_map<entt::id_type, engine::component::Animation> animations;
        entt::id_type animation_id = entt::hashed_string("tile");    // 图块动画名称默认为"tile"，自动循环播放
        auto animation_name = getStringProperty(tiled::TILE_PROPERTY_ANIM_ID); // 如果存在动画名称，则代表手动触发动画，不循环播放
        if (animation_name) {
            animation_id = entt::hashed_string(animation_name.value().c_str());
        }
        auto animation = tile_info_->animation_.value();    // 复制一份，避免修改tile_info_

        // 让动画和当前精灵保持一致的基础属性（pivot/size/纹理/翻转）
        if (auto sprite = registry_.try_get<engine::component::SpriteComponent>(entity_id_); sprite) {
            animation.pivot_ = sprite->pivot_;
            if (animation.dst_size_ == glm::vec2(0.0f)) {
                animation.dst_size_ = sprite->size_;
            }
            animation.texture_id_ = sprite->sprite_.texture_id_;
            animation.texture_path_ = sprite->sprite_.texture_path_;
            animation.flip_horizontal_ = sprite->sprite_.is_flipped_;
            animation.loop_ = animation_name ? false : true;
        }

        animations.emplace(animation_id, std::move(animation));
        // 通过动画map创建AnimationComponent，并添加(如果存在动画名称，则代表手动触发动画，不循环播放，所以动画ID为null)
        registry_.emplace<engine::component::AnimationComponent>(entity_id_, std::move(animations), animation_name ? entt::null : animation_id);
    }
}

void BasicEntityBuilder::buildAudio() {
    spdlog::trace("构建Audio组件");
    // 当前项目并未使用，未来可约定自定义属性并解析
}

const nlohmann::json* BasicEntityBuilder::findPropertyJson(std::string_view property_name, std::string_view type_name) {
    if (!tile_info_ || !tile_info_->properties_ || !tile_info_->properties_->is_array()) {
        return nullptr;
    }
    const auto& properties = *tile_info_->properties_;
    const std::string key(property_name);
    const std::string type_key(type_name);
    for (const auto& property : properties) {
        const auto* name = findStringMember(property, "name");
        const auto* type = findStringMember(property, "type");
        if (!name || *name != key || !type || *type != type_key) {
            continue;
        }
        return &property;
    }
    return nullptr;
}

std::optional<std::string> BasicEntityBuilder::getStringProperty(std::string_view property_name) {
    const auto* prop = findPropertyJson(property_name, "string");
    if (!prop) return std::nullopt;
    const auto* value = findStringMember(*prop, "value");
    if (value && !value->empty()) {
        return std::string(*value);
    }
    return std::nullopt;
}

std::optional<int> BasicEntityBuilder::getIntProperty(std::string_view property_name) {
    const auto* prop = findPropertyJson(property_name, "int");
    if (!prop) return std::nullopt;
    if (!findMember(*prop, "value")) return std::nullopt;
    return jsonIntOr(*prop, "value", 0);
}

} // namespace engine::loader
