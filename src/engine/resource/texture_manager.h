#pragma once
#include "texture_loader.h"

#include <string_view>
#include <entt/core/fwd.hpp>
#include <vector>
#include "resource_debug_info.h"

namespace engine::resource {
 

/**
 * @brief 管理 GL_Texture 资源的加载、存储和检索。
 *
 * 在构造时初始化。使用文件路径作为键，确保纹理只加载一次并正确释放。
 */
class TextureManager final{
    friend class ResourceManager;

private:
    TextureCache texture_cache_{};

public:
    TextureManager() = default;

    // 当前设计中，我们只需要一个TextureManager，所有权不变，所以不需要拷贝、移动相关构造及赋值运算符
    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;
    TextureManager(TextureManager&&) = delete;
    TextureManager& operator=(TextureManager&&) = delete;

private: // 仅供 ResourceManager 访问的方法

    /**
     * @brief 从文件路径加载纹理
     * @param id 纹理的唯一标识符, 通过entt::hashed_string生成
     * @param file_path 纹理文件的路径
     * @return 加载的纹理句柄
     * @note 如果纹理已经加载，则返回已加载的纹理句柄
     * @note 如果纹理未加载，则从文件路径加载纹理，并返回加载的纹理句柄
     */
    TextureHandle loadTexture(entt::id_type id, std::string_view file_path);
    TextureHandle findTexture(entt::id_type id);
    
    /**
     * @brief 从字符串哈希值加载纹理（path-hash）
     * @param str_hs entt::hashed_string类型
     * @return 加载的纹理句柄
     * @note 如果纹理已经加载，则返回已加载的纹理句柄
     * @note 如果纹理未加载，则从 `str_hs.data()` 指向的 file_path 加载纹理并缓存
     * @note 如果你传入的是“语义 key”（如 `"title-bg"`），请确保它已通过 `resource_mapping.json` 预加载，
     *       否则会把 key 当作路径而加载失败
     */
    TextureHandle loadTexture(entt::hashed_string str_hs);
    
    /**
     * @brief 卸载纹理
     * @param id 纹理的唯一标识符, 通过entt::hashed_string生成
     */
    void unloadTexture(entt::id_type id);

    /**
     * @brief 清空所有纹理资源
     */
    void clearTextures();

    void collectDebugInfo(std::vector<TextureDebugInfo>& out) const;
};

} // namespace engine::resource
