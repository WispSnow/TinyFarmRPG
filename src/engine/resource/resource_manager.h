#pragma once
#include <memory> // 用于 std::unique_ptr
#include <string_view> // 用于 std::string_view
#include <vector>
#include <glm/glm.hpp>
#include <entt/core/fwd.hpp>
#include <entt/signal/fwd.hpp>
#include <nlohmann/json_fwd.hpp>
#include "resource_debug_info.h"
#include "audio_loader.h"
#include "texture_loader.h"

// 前向声明 SDL 类型
struct SDL_Renderer;
struct SDL_Texture;

namespace engine::resource {

// 前向声明内部管理器
class TextureManager;
class AudioManager;
class FontManager;
class Font;
class AssetRegistry;

/**
 * @brief 作为访问各种资源管理器的中央控制点（外观模式 Facade）。
 * 在构造时初始化其管理的子系统。请使用 create() 创建实例。
 */
class ResourceManager final{
private:
    // 使用 unique_ptr 确保所有权和自动清理
    std::unique_ptr<TextureManager> texture_manager_;
    std::unique_ptr<AudioManager> audio_manager_;
    std::unique_ptr<FontManager> font_manager_;
    std::unique_ptr<AssetRegistry> asset_registry_;
    entt::dispatcher* dispatcher_{nullptr};

public:
    [[nodiscard]] static std::unique_ptr<ResourceManager> create(entt::dispatcher* dispatcher);
    ~ResourceManager();  // 显式声明析构函数，这是为了能让智能指针正确管理仅有前向声明的类

    void clear();        ///< @brief 清空所有资源
    void preloadRegisteredResources();   ///< @brief 预加载 AssetRegistry 中已注册的所有资源

    [[nodiscard]] AssetRegistry& getAssetRegistry();
    [[nodiscard]] const AssetRegistry& getAssetRegistry() const;

    // 当前设计中，我们只需要一个ResourceManager，所有权不变，所以不需要拷贝、移动相关构造及赋值运算符
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;
    ResourceManager(ResourceManager&&) = delete;
    ResourceManager& operator=(ResourceManager&&) = delete;

    // 加载资源
    void loadResources(std::string_view file_path);

    // --- 统一资源访问接口 ---
    // -- Texture --
    TextureHandle loadTexture(entt::id_type id, std::string_view file_path);         ///< @brief 载入纹理资源(通过id + 文件路径)
    TextureHandle getTexture(entt::id_type id);                                       ///< @brief 仅获取已加载纹理句柄(通过id)
    void unloadTexture(entt::id_type id);                                     ///< @brief 卸载指定的纹理资源
    glm::vec2 getTextureSize(entt::id_type id);                                 ///< @brief 获取指定纹理的尺寸(通过id)
    void clearTextures();                                                     ///< @brief 清空所有纹理资源

    // -- Sound Effects (Chunks) --
    AudioBufferHandle loadSound(entt::id_type id, std::string_view file_path);     ///< @brief 缓存音效资源(通过id + 文件路径)
    AudioBufferHandle getSound(entt::id_type id); ///< @brief 获取音效缓存(仅通过id)
    void unloadSound(entt::id_type id);                                             ///< @brief 卸载指定的音效资源
    void clearSounds();                                                             ///< @brief 清空所有音效资源

    // -- Music --
    AudioBufferHandle loadMusic(entt::id_type id, std::string_view file_path);     ///< @brief 缓存音乐资源(通过id + 文件路径)
    AudioBufferHandle getMusic(entt::id_type id); ///< @brief 获取音乐缓存(仅通过id)
    void unloadMusic(entt::id_type id);                                             ///< @brief 卸载指定的音乐资源
    void clearMusic();                                                              ///< @brief 清空所有音乐资源

    // -- Fonts --
    Font* loadFont(entt::id_type id, int pixel_size, std::string_view file_path);     ///< @brief 载入字体资源(通过id + 文件路径)
    Font* getFont(entt::id_type id, int pixel_size); ///< @brief 获取字体缓存（仅通过id + size）
    void unloadFont(entt::id_type id, int pixel_size);                              ///< @brief 卸载指定的字体资源
    void clearFonts();                                                              ///< @brief 清空所有字体资源

    // --- 调试接口 ---
    [[nodiscard]] std::vector<TextureDebugInfo> getTextureDebugInfo() const;      ///< @brief 获取所有纹理的调试信息
    [[nodiscard]] std::vector<FontDebugInfo> getFontDebugInfo() const;            ///< @brief 获取所有字体的调试信息
    [[nodiscard]] std::vector<AudioDebugInfo> getSoundDebugInfo() const;          ///< @brief 获取所有音效的调试信息
    [[nodiscard]] std::vector<AudioDebugInfo> getMusicDebugInfo() const;          ///< @brief 获取所有音乐的调试信息

private:
    ResourceManager(entt::dispatcher* dispatcher,
                    std::unique_ptr<TextureManager> texture_manager,
                    std::unique_ptr<AudioManager> audio_manager,
                    std::unique_ptr<FontManager> font_manager,
                    std::unique_ptr<AssetRegistry> asset_registry);
};

} // namespace engine::resource
