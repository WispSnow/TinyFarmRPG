#pragma once
#include "engine/utils/math.h"
#include <entt/core/hashed_string.hpp>
#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/common.hpp>
#include <string>
#include <string_view>

namespace engine::component {

/**
 * @brief 精灵数据结构
 * 
 * 包含纹理名称、源矩形和是否翻转。
 */
struct Sprite{
    entt::id_type texture_id_{entt::null};  ///< @brief 纹理ID
    std::string texture_path_;              ///< @brief 纹理路径
    engine::utils::Rect src_rect_{};        ///< @brief 源矩形(为了保证效率，不再使用std::optional，构造时必须提供)
    bool is_flipped_{false};                ///< @brief 是否翻转

    Sprite() = default;     ///< @brief 空的构造函数

    /**
     * @brief 构造函数 (通过纹理路径构造)
     * @param texture_path 纹理路径
     * @param source_rect 源矩形
     * @param is_flipped 是否翻转，默认false
     */
    Sprite(std::string_view texture_path, const engine::utils::Rect& source_rect, bool is_flipped = false)
        : texture_path_(texture_path), src_rect_(source_rect), is_flipped_(is_flipped) {
              texture_id_ = entt::hashed_string(texture_path_.c_str());
        }

    /**
     * @brief 构造函数 (通过纹理ID构造)
     * @param texture_id 纹理ID
     * @param source_rect 源矩形
     * @param is_flipped 是否翻转，默认false
     * @note 用此方法，需确保对应ID的纹理已经加载到ResourceManager中，因此不需要再提供纹理路径。
     */
    Sprite(entt::id_type texture_id, const engine::utils::Rect& source_rect, bool is_flipped = false)
        : texture_id_(texture_id), src_rect_(source_rect), is_flipped_(is_flipped) {}
};

/**
 * @brief 精灵组件
 * 
 * 包含精灵、大小与锚点（pivot）。
 */
struct SpriteComponent {
    Sprite sprite_;                ///< @brief 精灵
    glm::vec2 size_{0.0f};         ///< @brief 大小
    glm::vec2 pivot_{0.0f};        ///< @brief 锚点（建议范围 0..1；(0,0)=左上，(0.5,0.5)=中心，(0.5,1)=底部中心）

    SpriteComponent() = default;

    /**
     * @brief 构造函数
     * @param sprite 精灵
     * @param size 大小
     * @param pivot 锚点，默认(0,0)
     */
    SpriteComponent(Sprite sprite, glm::vec2 size = glm::vec2(0.0f, 0.0f), glm::vec2 pivot = glm::vec2(0.0f, 0.0f))
        : sprite_(std::move(sprite)), size_(size), pivot_(pivot) {
            // 如果size为0（未提供），则使用精灵的源矩形大小
            if (glm::all(glm::equal(size, glm::vec2(0.0f)))) {
                size_ = glm::vec2(sprite_.src_rect_.size.x, sprite_.src_rect_.size.y);
            }
        }
};

} // namespace engine::component
