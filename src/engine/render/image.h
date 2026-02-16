#pragma once
#include "engine/utils/math.h"
#include "nine_slice.h"
#include <SDL3/SDL_rect.h>   // 用于 SDL_FRect
#include <optional>
#include <string>
#include <string_view>
#include <entt/core/hashed_string.hpp>
#include <entt/entity/entity.hpp>

namespace engine::render {

/**
 * @brief 表示要绘制的UI图片的数据。（只针对UI，与游戏中Sprite隔离）
 *
 * 包含纹理标识符、要绘制的纹理部分（源矩形，必须显式给出尺寸，否则不渲染）以及翻转状态。
 * 位置、缩放和旋转由外部（例如 UIImage）标识。
 * 渲染工作由 Renderer 类完成。（传入Image作为参数）
 */
class Image final{
private:
    std::string texture_path_;                          ///< @brief 纹理资源的文件路径
    entt::id_type texture_id_{entt::null};              ///< @brief 纹理资源的标识符 (entt::null是推荐的初始化方式，表示无效的ID)
    engine::utils::Rect source_rect_{};                 ///< @brief 要绘制的纹理部分（必须指定尺寸，为空则跳过绘制）
    bool is_flipped_{false};                            ///< @brief 是否水平翻转
    std::optional<NineSliceMargins> nine_slice_margins_{};      ///< @brief 可选九宫格边距
    mutable std::optional<NineSlice> nine_slice_cache_{};       ///< @brief 九宫格缓存
    mutable bool nine_slice_dirty_{false};                      ///< @brief 九宫格缓存是否需要重建

public:
    /**
     * @brief 默认构造函数（创建一个空的/无效的精灵）
     */
    Image() = default;
    
    /**
     * @brief 构造一个精灵 （通过纹理路径构造）
     *
     * @param texture_path 纹理资源的文件路径。不应为空。
     * @param source_rect 要绘制的纹理部分。若尺寸为 0，则不渲染。
     * @param is_flipped 是否水平翻转
     */
    Image(std::string_view texture_path, engine::utils::Rect source_rect, bool is_flipped = false);

    Image(std::string_view texture_path, entt::id_type texture_id, engine::utils::Rect source_rect, bool is_flipped = false);

    /**
     * @brief 构造一个精灵 （通过纹理ID构造）
     *
     * @param texture_id 纹理资源的标识符。不应为空。
     * @param source_rect 要绘制的纹理部分。若尺寸为 0，则不渲染。
     * @param is_flipped 是否水平翻转
     * @note 用此方法，需确保对应ID的纹理已经加载到ResourceManager中，因此不需要再提供纹理路径。
     */
    Image(entt::id_type texture_id, engine::utils::Rect source_rect, bool is_flipped = false);

    // --- getters and setters ---
    std::string_view getTexturePath() const { return texture_path_; }                           ///< @brief 获取纹理路径
    entt::id_type getTextureId() const { return texture_id_; }                                  ///< @brief 获取纹理 ID
    const engine::utils::Rect& getSourceRect() const { return source_rect_; }                   ///< @brief 获取源矩形（尺寸为0则不绘制）
    bool isFlipped() const { return is_flipped_; }                                              ///< @brief 获取是否水平翻转

    /**
     * @brief 设置纹理路径同时更新纹理ID
     * @param texture_path 纹理资源的文件路径。不应为空。
     */
    void setTexture(std::string_view texture_path);

    void setTexture(std::string_view texture_path, entt::id_type texture_id);

    void setTextureId(entt::id_type texture_id);   ///< @brief 设置纹理ID (需确保已载入)

    /**
     * @brief 设置源矩形
     * @param source_rect 源矩形。若尺寸为 0，则不渲染。
     */
    void setSourceRect(const engine::utils::Rect& source_rect);

    /**
     * @brief 设置是否水平翻转
     * @param flipped 是否水平翻转
     */
    void setFlipped(bool flipped);

    // --- 九宫格支持 ---
    void setNineSliceMargins(std::optional<NineSliceMargins> margins);

    [[nodiscard]] const std::optional<NineSliceMargins>& getNineSliceMargins() const { return nine_slice_margins_; }

    [[nodiscard]] bool hasNineSlice() const;

    void markNineSliceDirty() const;

    [[nodiscard]] const NineSlice* ensureNineSlice() const;

};

} // namespace engine::render
