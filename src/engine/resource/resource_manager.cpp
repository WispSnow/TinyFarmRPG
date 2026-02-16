#include "resource_manager.h"
#include "texture_manager.h"
#include "audio_manager.h"
#include "font_manager.h" 
#include "auto_tile_library.h"
#include "engine/ui/ui_preset_manager.h"
#include <fstream>
#include <filesystem>
#include <system_error>
#include <glm/glm.hpp>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <entt/core/hashed_string.hpp>
#include <entt/signal/dispatcher.hpp>
#include "engine/utils/events.h"
#include <algorithm>
#include <tuple>
#include <ranges>
 
namespace engine::resource {

ResourceManager::~ResourceManager() = default;

std::unique_ptr<ResourceManager> ResourceManager::create(entt::dispatcher* dispatcher) {
    if (!dispatcher) {
        spdlog::error("ResourceManager::create: dispatcher 不能为空。");
        return nullptr;
    }
    auto texture_manager = std::make_unique<TextureManager>();
    auto audio_manager = std::make_unique<AudioManager>();
    auto font_manager = FontManager::create();
    if (!font_manager) {
        spdlog::error("ResourceManager::create: FontManager 初始化失败。");
        return nullptr;
    }
    auto auto_tile_library = std::make_unique<AutoTileLibrary>();
    auto ui_preset_manager = std::make_unique<engine::ui::UIPresetManager>();

    return std::unique_ptr<ResourceManager>(new ResourceManager(dispatcher,
                                                                std::move(texture_manager),
                                                                std::move(audio_manager),
                                                                std::move(font_manager),
                                                                std::move(auto_tile_library),
                                                                std::move(ui_preset_manager)));
}

ResourceManager::ResourceManager(entt::dispatcher* dispatcher,
                                 std::unique_ptr<TextureManager> texture_manager,
                                 std::unique_ptr<AudioManager> audio_manager,
                                 std::unique_ptr<FontManager> font_manager,
                                 std::unique_ptr<AutoTileLibrary> auto_tile_library,
                                 std::unique_ptr<engine::ui::UIPresetManager> ui_preset_manager)
    : texture_manager_(std::move(texture_manager)),
      audio_manager_(std::move(audio_manager)),
      font_manager_(std::move(font_manager)),
      auto_tile_library_(std::move(auto_tile_library)),
      ui_preset_manager_(std::move(ui_preset_manager)),
      dispatcher_(dispatcher) {
    spdlog::trace("ResourceManager 构造成功。");
    // RAII: 构造成功即代表资源管理器可以正常工作，无需再初始化，无需检查指针是否为空
}

void ResourceManager::clear() {
    clearFonts();
    audio_manager_->clearAudio();
    texture_manager_->clearTextures();
    auto_tile_library_->clearRules();
    if (ui_preset_manager_) {
        ui_preset_manager_->clearButtonPresets();
        ui_preset_manager_->clearImagePresets();
    }
    spdlog::trace("ResourceManager 中的资源通过 clear() 清空。");
}

AutoTileLibrary& ResourceManager::getAutoTileLibrary() {
    return *auto_tile_library_;
}

const AutoTileLibrary& ResourceManager::getAutoTileLibrary() const {
    return *auto_tile_library_;
}

void ResourceManager::loadResources(std::string_view file_path) {
    std::filesystem::path path(file_path);
    std::error_code error_code;
    const bool exists = std::filesystem::exists(path, error_code);
    if (error_code || !exists) {
        spdlog::warn("资源映射文件不存在: {}", file_path);
        return;
    }
    std::ifstream file(path);
    if (!file.is_open()) {
        spdlog::warn("资源映射文件无法打开: {}", file_path);
        return;
    }

    const std::string file_content(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

    const nlohmann::json json = nlohmann::json::parse(file_content, nullptr, false);
    if (json.is_discarded() || !json.is_object()) {
        spdlog::error("加载资源文件失败: '{}' 不是有效的 JSON 对象。", file_path);
        return;
    }

    auto loadStringMap = [&](std::string_view section, auto&& loader) {
        const auto it = json.find(std::string(section));
        if (it == json.end()) {
            return;
        }
        if (!it->is_object()) {
            spdlog::warn("资源映射文件 '{}' 的 '{}' 字段格式无效 (应为对象)。", file_path, section);
            return;
        }

        for (const auto& [key, value] : it->items()) {
            if (!value.is_string()) {
                spdlog::warn("资源映射文件 '{}' 的 '{}.{}' 格式无效 (应为字符串)。", file_path, section, key);
                continue;
            }
            const entt::id_type id = entt::hashed_string{key.c_str(), key.size()};
            loader(id, value.get<std::string>());
        }
    };

    loadStringMap("sound", [this](entt::id_type id, const std::string& path_value) { loadSound(id, path_value); });
    loadStringMap("music", [this](entt::id_type id, const std::string& path_value) { loadMusic(id, path_value); });
    loadStringMap("texture", [this](entt::id_type id, const std::string& path_value) { loadTexture(id, path_value); });

    if (const auto it = json.find("font"); it != json.end()) {
        if (!it->is_object()) {
            spdlog::warn("资源映射文件 '{}' 的 'font' 字段格式无效 (应为对象)。", file_path);
        } else {
            for (const auto& [key, value] : it->items()) {
                const entt::id_type id = entt::hashed_string{key.c_str(), key.size()};

                int point_size = 0;
                std::string font_path{};

                if (value.is_object()) {
                    if (const auto path_it = value.find("path"); path_it != value.end() && path_it->is_string()) {
                        font_path = path_it->get<std::string>();
                    }
                    if (font_path.empty()) {
                        if (const auto path_it = value.find("file_path"); path_it != value.end() && path_it->is_string()) {
                            font_path = path_it->get<std::string>();
                        }
                    }

                    if (const auto size_it = value.find("size"); size_it != value.end()) {
                        if (const auto* v = size_it->get_ptr<const nlohmann::json::number_integer_t*>()) {
                            point_size = (*v > 0) ? static_cast<int>(*v) : 0;
                        } else if (const auto* v = size_it->get_ptr<const nlohmann::json::number_unsigned_t*>()) {
                            point_size = (*v > 0) ? static_cast<int>(*v) : 0;
                        }
                    }
                    if (point_size == 0) {
                        if (const auto size_it = value.find("point_size"); size_it != value.end()) {
                            if (const auto* v = size_it->get_ptr<const nlohmann::json::number_integer_t*>()) {
                                point_size = (*v > 0) ? static_cast<int>(*v) : 0;
                            } else if (const auto* v = size_it->get_ptr<const nlohmann::json::number_unsigned_t*>()) {
                                point_size = (*v > 0) ? static_cast<int>(*v) : 0;
                            }
                        }
                    }
                } else if (value.is_array() && value.size() >= 2) {
                    const auto& first = value[0];
                    const auto& second = value[1];
                    if (first.is_string()) {
                        font_path = first.get<std::string>();
                        if (const auto* v = second.get_ptr<const nlohmann::json::number_integer_t*>()) {
                            point_size = (*v > 0) ? static_cast<int>(*v) : 0;
                        } else if (const auto* v = second.get_ptr<const nlohmann::json::number_unsigned_t*>()) {
                            point_size = (*v > 0) ? static_cast<int>(*v) : 0;
                        }
                    } else if (second.is_string()) {
                        font_path = second.get<std::string>();
                        if (const auto* v = first.get_ptr<const nlohmann::json::number_integer_t*>()) {
                            point_size = (*v > 0) ? static_cast<int>(*v) : 0;
                        } else if (const auto* v = first.get_ptr<const nlohmann::json::number_unsigned_t*>()) {
                            point_size = (*v > 0) ? static_cast<int>(*v) : 0;
                        }
                    }
                }

                if (font_path.empty() || point_size <= 0) {
                    spdlog::warn("资源映射文件 '{}' 的 'font.{}' 缺少有效的 path/size，已跳过。", file_path, key);
                    continue;
                }
                loadFont(id, point_size, font_path);
            }
        }
    }

    auto loadPresetList = [&](std::string_view field, auto&& loader) {
        const auto it = json.find(std::string(field));
        if (it == json.end()) {
            return;
        }

        if (it->is_string()) {
            loader(it->get<std::string>());
            return;
        }

        if (it->is_array()) {
            for (const auto& entry : *it) {
                if (entry.is_string()) {
                    loader(entry.get<std::string>());
                } else {
                    spdlog::warn("资源映射文件 '{}' 的 '{}' 数组包含无效条目 (应为 string)。", file_path, field);
                }
            }
            return;
        }

        spdlog::warn("资源映射文件 '{}' 的 '{}' 字段格式无效 (应为 string 或 array)。", file_path, field);
    };

    loadPresetList("ui_button_presets", [this](const std::string& preset_path) { loadUIButtonPresets(preset_path); });
    loadPresetList("ui_image_presets", [this](const std::string& preset_path) { loadUIImagePresets(preset_path); });
}

void ResourceManager::loadUIButtonPresets(std::string_view file_path) {
    if (!ui_preset_manager_) {
        spdlog::warn("ResourceManager: UIPresetManager 未初始化，无法加载预设。");
        return;
    }

    if (!ui_preset_manager_->loadButtonPresets(file_path)) {
        spdlog::warn("ResourceManager: 加载按钮预设失败: {}", file_path);
    }
}

void ResourceManager::loadUIImagePresets(std::string_view file_path) {
    if (!ui_preset_manager_) {
        spdlog::warn("ResourceManager: UIPresetManager 未初始化，无法加载图片预设。");
        return;
    }

    if (!ui_preset_manager_->loadImagePresets(file_path)) {
        spdlog::warn("ResourceManager: 加载图片预设失败: {}", file_path);
    }
}

// --- 纹理接口实现 ---
engine::utils::GL_Texture* ResourceManager::loadTexture(entt::id_type id, std::string_view file_path) {
    // 构造函数已经确保了 texture_manager_ 不为空，因此不需要再进行if检查，以免性能浪费
    return texture_manager_->loadTexture(id, file_path);
}

engine::utils::GL_Texture* ResourceManager::loadTexture(entt::hashed_string str_hs) {
    return texture_manager_->loadTexture(str_hs);
}

engine::utils::GL_Texture* ResourceManager::getTexture(entt::id_type id, std::string_view file_path) {
    return texture_manager_->getTexture(id, file_path);
}

engine::utils::GL_Texture* ResourceManager::getTexture(entt::hashed_string str_hs) {
    return texture_manager_->getTexture(str_hs);
}

glm::vec2 ResourceManager::getTextureSize(entt::id_type id, std::string_view file_path) {
    return texture_manager_->getTextureSize(id, file_path);
}

glm::vec2 ResourceManager::getTextureSize(entt::hashed_string str_hs) {
    return texture_manager_->getTextureSize(str_hs);
}

void ResourceManager::unloadTexture(entt::id_type id) {
    texture_manager_->unloadTexture(id);
}

void ResourceManager::clearTextures() {
    texture_manager_->clearTextures();
}

// --- 音频接口实现 ---
AudioManager::AudioBufferHandle ResourceManager::loadSound(entt::id_type id, std::string_view file_path) {
    return audio_manager_->loadSound(id, file_path);
}

AudioManager::AudioBufferHandle ResourceManager::loadSound(entt::hashed_string str_hs) {
    return audio_manager_->loadSound(str_hs);
}

AudioManager::AudioBufferHandle ResourceManager::getSound(entt::id_type id, std::string_view file_path) {
    return audio_manager_->getSound(id, file_path);
}

AudioManager::AudioBufferHandle ResourceManager::getSound(entt::hashed_string str_hs) {
    return audio_manager_->getSound(str_hs);
}

void ResourceManager::unloadSound(entt::id_type id) {
    audio_manager_->unloadSound(id);
}

void ResourceManager::clearSounds() {
    audio_manager_->clearSounds();
}

AudioManager::AudioBufferHandle ResourceManager::loadMusic(entt::id_type id, std::string_view file_path) {
    return audio_manager_->loadMusic(id, file_path);
}

AudioManager::AudioBufferHandle ResourceManager::loadMusic(entt::hashed_string str_hs) {
    return audio_manager_->loadMusic(str_hs);
}

AudioManager::AudioBufferHandle ResourceManager::getMusic(entt::id_type id, std::string_view file_path) {
    return audio_manager_->getMusic(id, file_path);
}

AudioManager::AudioBufferHandle ResourceManager::getMusic(entt::hashed_string str_hs) {
    return audio_manager_->getMusic(str_hs);
}

void ResourceManager::unloadMusic(entt::id_type id) {
    audio_manager_->unloadMusic(id);
}

void ResourceManager::clearMusic() {
    audio_manager_->clearMusic();
}

// --- 字体接口实现 ---
Font* ResourceManager::loadFont(entt::id_type id, int pixel_size, std::string_view file_path) {
    return font_manager_->loadFont(id, pixel_size, file_path);
}

Font* ResourceManager::loadFont(entt::hashed_string str_hs, int pixel_size) {
    return font_manager_->loadFont(str_hs, pixel_size);
}

Font* ResourceManager::getFont(entt::id_type id, int pixel_size, std::string_view file_path) {
    return font_manager_->getFont(id, pixel_size, file_path);
}

Font* ResourceManager::getFont(entt::hashed_string str_hs, int pixel_size) {
    return font_manager_->getFont(str_hs, pixel_size);
}

void ResourceManager::unloadFont(entt::id_type id, int pixel_size) {
    font_manager_->unloadFont(id, pixel_size);
    dispatcher_->enqueue<engine::utils::FontUnloadedEvent>(engine::utils::FontUnloadedEvent{id, pixel_size});
    spdlog::trace("卸载字体: {} ({}px)", id, pixel_size);
}

void ResourceManager::clearFonts() {
    font_manager_->clearFonts();
    dispatcher_->enqueue<engine::utils::FontsClearedEvent>();

}

std::vector<TextureDebugInfo> ResourceManager::getTextureDebugInfo() const {
    std::vector<TextureDebugInfo> result;
    if (texture_manager_) {
        texture_manager_->collectDebugInfo(result);
        // 仅按 id 排序的 Ranges 写法。参数含义: (容器, 比较器(默认less), 投影(取成员变量))
        std::ranges::sort(result, {}, &TextureDebugInfo::id);
    }
    return result;
}

std::vector<FontDebugInfo> ResourceManager::getFontDebugInfo() const {
    std::vector<FontDebugInfo> result;
    if (font_manager_) {
        font_manager_->collectDebugInfo(result);
        // 使用投影(Projection)生成元组进行字典序比较，代码更具声明性
        std::ranges::sort(result, std::less{}, [](const FontDebugInfo& info) {
            return std::tie(info.id, info.pixel_size);
        });
    }
    return result;
}

std::vector<AudioDebugInfo> ResourceManager::getSoundDebugInfo() const {
    std::vector<AudioDebugInfo> result;
    if (audio_manager_) {
        audio_manager_->collectSoundDebugInfo(result);
        std::ranges::sort(result, {}, &AudioDebugInfo::id);
    }
    return result;
}

std::vector<AudioDebugInfo> ResourceManager::getMusicDebugInfo() const {
    std::vector<AudioDebugInfo> result;
    if (audio_manager_) {
        audio_manager_->collectMusicDebugInfo(result);
        std::ranges::sort(result, {}, &AudioDebugInfo::id);
    }
    return result;
}

std::vector<AutoTileRuleDebugInfo> ResourceManager::getAutoTileDebugInfo() const {
    if (!auto_tile_library_) {
        return {};
    }
    auto result = auto_tile_library_->getDebugInfo();
    std::ranges::sort(result, std::less{}, [](const AutoTileRuleDebugInfo& info) {
        return std::tie(info.name, info.rule_id);
    });
    return result;
}

engine::ui::UIPresetManager& ResourceManager::getUIPresetManager() {
    return *ui_preset_manager_;
}

const engine::ui::UIPresetManager& ResourceManager::getUIPresetManager() const {
    return *ui_preset_manager_;
}

} // namespace engine::resource
