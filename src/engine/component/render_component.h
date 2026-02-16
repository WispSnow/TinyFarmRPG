#pragma once
#include "engine/utils/math.h"

namespace engine::component {

/**
 * @brief 渲染组件, 包含图层ID和深度，
 * 颜色调整参数（调整后 = 原始颜色 * 调整颜色）
 */
struct RenderComponent {
    static constexpr int MAIN_LAYER{10};    ///< @brief 主图层ID，默认为10
    
    int layer_{};        ///< @brief 渲染层（越小越先绘制；常用于跨 tile/actor/UI 等大层级排序）
    float depth_{};      ///< @brief 层内深度（越小越先绘制；常用做 y-sort：depth = position.y）
                         /*  (注意：若世界坐标 y 越大越靠下，则 depth=y 会让“更靠下”的对象更晚绘制，从而呈现遮挡在前) */
    engine::utils::FColor color_{engine::utils::FColor::white()}; ///< @brief 颜色调整参数

    /**
     * @brief 构造函数
     * @param layer 图层ID，数字越小越先绘制(默认MAIN_LAYER)
     * @param depth 在同一图层内的深度，数字越小越先绘制 (默认0.0f)
     * @param color 颜色调整参数 (调整后 = 原始颜色 * 调整颜色) (默认白色)
     */
    explicit RenderComponent(int layer = MAIN_LAYER, float depth = 0.0f, 
        engine::utils::FColor color = engine::utils::FColor::white()) 
        : layer_(layer), depth_(depth), color_(color) {}

    /// 重载比较运算符，用于排序
    [[nodiscard]] constexpr bool operator<(const RenderComponent& other) const noexcept {
        if (layer_ == other.layer_) {     // 如果图层相同，则比较深度
            return depth_ < other.depth_;
        }
        return layer_ < other.layer_;    // 如果图层不同，则比较图层ID
    }
};

}   // namespace engine::component
