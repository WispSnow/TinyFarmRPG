#pragma once
#include <memory> // 用于 std::unique_ptr
#include <string_view> // 用于 std::string_view
#include <vector>
#include <glm/glm.hpp>
#include <entt/core/fwd.hpp>
#include <entt/signal/fwd.hpp>
#include <nlohmann/json_fwd.hpp>
#include "resource_debug_info.h"
#include "audio_manager.h"

// 前向声明 SDL 类型
struct SDL_Renderer;
struct SDL_Texture;
namespace engine::utils {
struct GL_Texture;
}

namespace engine::ui {
class UIPresetManager;
}

namespace engine::resource {

// 前向声明内部管理器
class TextureManager;
class AudioManager;
class FontManager;
class AutoTileLibrary;
class Font;

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
    std::unique_ptr<AutoTileLibrary> auto_tile_library_;
    std::unique_ptr<engine::ui::UIPresetManager> ui_preset_manager_;
    entt::dispatcher* dispatcher_{nullptr};

public:
    [[nodiscard]] static std::unique_ptr<ResourceManager> create(entt::dispatcher* dispatcher);
    ~ResourceManager();  // 显式声明析构函数，这是为了能让智能指针正确管理仅有前向声明的类

    void clear();        ///< @brief 清空所有资源

    // -- Auto Tile Library --
    AutoTileLibrary& getAutoTileLibrary();
    const AutoTileLibrary& getAutoTileLibrary() const;

    // 当前设计中，我们只需要一个ResourceManager，所有权不变，所以不需要拷贝、移动相关构造及赋值运算符
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;
    ResourceManager(ResourceManager&&) = delete;
    ResourceManager& operator=(ResourceManager&&) = delete;

    // 加载资源
    void loadResources(std::string_view file_path);
    void loadUIButtonPresets(std::string_view file_path);
    void loadUIImagePresets(std::string_view file_path);

    // --- 统一资源访问接口 ---
    // -- Texture --
    engine::utils::GL_Texture* loadTexture(entt::id_type id, std::string_view file_path);         ///< @brief 载入纹理资源(通过id + 文件路径)
    /**
     * @brief 载入纹理资源（path-hash）。
     * @note 该 overload 把 `str_hs.data()` 当作 file_path 使用。
     *       若你传入的是“语义 key”（如 `"title-bg"`），请确保它已通过 `resource_mapping.json` 预加载；
     *       或改用 `loadTexture(id, file_path)`。
     */
    engine::utils::GL_Texture* loadTexture(entt::hashed_string str_hs);
    engine::utils::GL_Texture* getTexture(entt::id_type id, std::string_view file_path = "");     ///< @brief 尝试获取已加载纹理的指针，如果未加载则尝试加载(通过id + 文件路径)
    /**
     * @brief 尝试获取已加载纹理的指针，如果未加载则尝试加载（path-hash）。
     * @note 该 overload 把 `str_hs.data()` 当作 file_path 使用：未命中缓存时，会尝试加载该路径。
     *       若你传入的是“语义 key”（如 `"title-bg"`），请确保它已通过 `resource_mapping.json` 预加载；
     *       或改用 `getTexture(id, file_path)`。
     */
    engine::utils::GL_Texture* getTexture(entt::hashed_string str_hs);
    void unloadTexture(entt::id_type id);                                     ///< @brief 卸载指定的纹理资源
    glm::vec2 getTextureSize(entt::id_type id, std::string_view file_path = "");    ///< @brief 获取指定纹理的尺寸(通过id + 文件路径)
    glm::vec2 getTextureSize(entt::hashed_string str_hs);                           ///< @brief 获取指定纹理的尺寸(通过字符串哈希值)
    void clearTextures();                                                     ///< @brief 清空所有纹理资源

    // -- Sound Effects (Chunks) --
    AudioManager::AudioBufferHandle loadSound(entt::id_type id, std::string_view file_path);     ///< @brief 缓存音效资源(通过id + 文件路径)
    /**
     * @brief 缓存音效资源（path-hash）。
     * @note 该 overload 把 `str_hs.data()` 当作 file_path 使用。
     *       若你传入的是“语义 key”（如 `"ui_click"`），请确保它已通过 `resource_mapping.json` 预加载；
     *       或改用 `loadSound(id, file_path)`。
     */
    AudioManager::AudioBufferHandle loadSound(entt::hashed_string str_hs);
    AudioManager::AudioBufferHandle getSound(entt::id_type id, std::string_view file_path = ""); ///< @brief 获取音效缓存(通过id + 文件路径)
    /**
     * @brief 获取音效缓存（path-hash）。
     * @note 该 overload 把 `str_hs.data()` 当作 file_path 使用：未命中缓存时，会尝试加载该路径。
     *       若你传入的是“语义 key”（如 `"ui_click"`），请确保它已通过 `resource_mapping.json` 预加载；
     *       或改用 `getSound(id, file_path)`。
     */
    AudioManager::AudioBufferHandle getSound(entt::hashed_string str_hs);
    void unloadSound(entt::id_type id);                                             ///< @brief 卸载指定的音效资源
    void clearSounds();                                                             ///< @brief 清空所有音效资源

    // -- Music --
    AudioManager::AudioBufferHandle loadMusic(entt::id_type id, std::string_view file_path);     ///< @brief 缓存音乐资源(通过id + 文件路径)
    /**
     * @brief 缓存音乐资源（path-hash）。
     * @note 该 overload 把 `str_hs.data()` 当作 file_path 使用。
     *       若你传入的是“语义 key”（如 `"title-bg-music"`），请确保它已通过 `resource_mapping.json` 预加载；
     *       或改用 `loadMusic(id, file_path)`。
     */
    AudioManager::AudioBufferHandle loadMusic(entt::hashed_string str_hs);
    AudioManager::AudioBufferHandle getMusic(entt::id_type id, std::string_view file_path = ""); ///< @brief 获取音乐缓存(通过id + 文件路径)
    /**
     * @brief 获取音乐缓存（path-hash）。
     * @note 该 overload 把 `str_hs.data()` 当作 file_path 使用：未命中缓存时，会尝试加载该路径。
     *       若你传入的是“语义 key”（如 `"title-bg-music"`），请确保它已通过 `resource_mapping.json` 预加载；
     *       或改用 `getMusic(id, file_path)`。
     */
    AudioManager::AudioBufferHandle getMusic(entt::hashed_string str_hs);
    void unloadMusic(entt::id_type id);                                             ///< @brief 卸载指定的音乐资源
    void clearMusic();                                                              ///< @brief 清空所有音乐资源

    // -- Fonts --
    Font* loadFont(entt::id_type id, int pixel_size, std::string_view file_path);     ///< @brief 载入字体资源(通过id + 文件路径)
    /**
     * @brief 载入字体资源（path-hash）。
     * @note 该 overload 把 `str_hs.data()` 当作 file_path 使用。
     *       若你传入的是“语义 key”，请确保它能被解析到真实路径（例如通过 `resource_mapping.json` 预加载）；
     *       或改用 `loadFont(id, pixel_size, file_path)`。
     */
    Font* loadFont(entt::hashed_string str_hs, int pixel_size);
    Font* getFont(entt::id_type id, int pixel_size, std::string_view file_path = ""); ///< @brief 尝试获取已加载字体的指针，如果未加载则尝试加载(通过id + 文件路径)
    /**
     * @brief 获取字体缓存（path-hash）。
     * @note 该 overload 把 `str_hs.data()` 当作 file_path 使用：未命中缓存时，会尝试加载该路径。
     *       若你传入的是“语义 key”，请确保它已通过 `resource_mapping.json` 预加载；
     *       或改用 `getFont(id, pixel_size, file_path)`。
     */
    Font* getFont(entt::hashed_string str_hs, int pixel_size);
    void unloadFont(entt::id_type id, int pixel_size);                              ///< @brief 卸载指定的字体资源
    void clearFonts();                                                              ///< @brief 清空所有字体资源

    // -- UI Presets --
    [[nodiscard]] engine::ui::UIPresetManager& getUIPresetManager();
    [[nodiscard]] const engine::ui::UIPresetManager& getUIPresetManager() const;

    // --- 调试接口 ---
    [[nodiscard]] std::vector<TextureDebugInfo> getTextureDebugInfo() const;      ///< @brief 获取所有纹理的调试信息
    [[nodiscard]] std::vector<FontDebugInfo> getFontDebugInfo() const;            ///< @brief 获取所有字体的调试信息
    [[nodiscard]] std::vector<AudioDebugInfo> getSoundDebugInfo() const;          ///< @brief 获取所有音效的调试信息
    [[nodiscard]] std::vector<AudioDebugInfo> getMusicDebugInfo() const;          ///< @brief 获取所有音乐的调试信息
    [[nodiscard]] std::vector<AutoTileRuleDebugInfo> getAutoTileDebugInfo() const;///< @brief 获取自动图块规则调试信息

private:
    ResourceManager(entt::dispatcher* dispatcher,
                    std::unique_ptr<TextureManager> texture_manager,
                    std::unique_ptr<AudioManager> audio_manager,
                    std::unique_ptr<FontManager> font_manager,
                    std::unique_ptr<AutoTileLibrary> auto_tile_library,
                    std::unique_ptr<engine::ui::UIPresetManager> ui_preset_manager);
};

} // namespace engine::resource
