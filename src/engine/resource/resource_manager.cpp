#include "resource_manager.h"
#include "asset_registry.h"
#include "texture_manager.h"
#include "audio_manager.h"
#include "font_manager.h" 
#include "engine/ui/ui_defaults.h"
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
    auto asset_registry = std::make_unique<AssetRegistry>();

    return std::unique_ptr<ResourceManager>(new ResourceManager(dispatcher,
                                                                std::move(texture_manager),
                                                                std::move(audio_manager),
                                                                std::move(font_manager),
                                                                std::move(asset_registry)));
}

ResourceManager::ResourceManager(entt::dispatcher* dispatcher,
                                 std::unique_ptr<TextureManager> texture_manager,
                                 std::unique_ptr<AudioManager> audio_manager,
                                 std::unique_ptr<FontManager> font_manager,
                                 std::unique_ptr<AssetRegistry> asset_registry)
    : texture_manager_(std::move(texture_manager)),
      audio_manager_(std::move(audio_manager)),
      font_manager_(std::move(font_manager)),
      asset_registry_(std::move(asset_registry)),
      dispatcher_(dispatcher) {
    spdlog::trace("ResourceManager 构造成功。");
    // RAII: 构造成功即代表资源管理器可以正常工作，无需再初始化，无需检查指针是否为空
}

void ResourceManager::clear() {
    clearFonts();
    audio_manager_->clearAudio();
    texture_manager_->clearTextures();
    spdlog::trace("ResourceManager 中的资源通过 clear() 清空。");
}

AssetRegistry& ResourceManager::getAssetRegistry() {
    return *asset_registry_;
}

const AssetRegistry& ResourceManager::getAssetRegistry() const {
    return *asset_registry_;
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

    constexpr std::string_view CIRCLE_TEXTURE_PATH{"assets/textures/UI/circle.png"};
    const entt::id_type circle_texture_id = entt::hashed_string{CIRCLE_TEXTURE_PATH.data(), CIRCLE_TEXTURE_PATH.size()};
    asset_registry_->registerTexture(circle_texture_id, CIRCLE_TEXTURE_PATH);

    const entt::id_type default_font_id =
        entt::hashed_string{engine::ui::DEFAULT_UI_FONT_PATH.data(), engine::ui::DEFAULT_UI_FONT_PATH.size()};
    asset_registry_->registerFont(default_font_id, engine::ui::DEFAULT_UI_FONT_SIZE_PX, engine::ui::DEFAULT_UI_FONT_PATH);
}

// --- 纹理接口实现 ---
TextureHandle ResourceManager::loadTexture(entt::id_type id, std::string_view file_path) {
    // 构造函数已经确保了 texture_manager_ 不为空，因此不需要再进行if检查，以免性能浪费
    asset_registry_->registerTexture(id, file_path);
    return texture_manager_->loadTexture(id, file_path);
}

TextureHandle ResourceManager::loadTexture(entt::hashed_string str_hs) {
    return loadTexture(str_hs.value(), str_hs.data());
}

TextureHandle ResourceManager::getTexture(entt::id_type id, std::string_view file_path) {
    if (auto cached = texture_manager_->findTexture(id)) {
        return cached;
    }

    std::string_view resolved_path = file_path;
    if (resolved_path.empty()) {
        resolved_path = asset_registry_->findTexturePath(id);
    }

    if (resolved_path.empty()) {
#ifndef NDEBUG
        spdlog::error("ResourceManager::getTexture: id={} 未命中缓存且 AssetRegistry 未注册路径。", id);
#endif
        return {};
    }

    return loadTexture(id, resolved_path);
}

TextureHandle ResourceManager::getTexture(entt::hashed_string str_hs) {
    return getTexture(str_hs.value(), str_hs.data());
}

glm::vec2 ResourceManager::getTextureSize(entt::id_type id, std::string_view file_path) {
    if (const auto texture = getTexture(id, file_path)) {
        return glm::vec2(texture->width, texture->height);
    }
    return glm::vec2(0.0f, 0.0f);
}

glm::vec2 ResourceManager::getTextureSize(entt::hashed_string str_hs) {
    return getTextureSize(str_hs.value(), str_hs.data());
}

void ResourceManager::unloadTexture(entt::id_type id) {
    texture_manager_->unloadTexture(id);
}

void ResourceManager::clearTextures() {
    texture_manager_->clearTextures();
}

// --- 音频接口实现 ---
AudioManager::AudioBufferHandle ResourceManager::loadSound(entt::id_type id, std::string_view file_path) {
    asset_registry_->registerSound(id, file_path);
    return audio_manager_->loadSound(id, file_path);
}

AudioManager::AudioBufferHandle ResourceManager::loadSound(entt::hashed_string str_hs) {
    return loadSound(str_hs.value(), str_hs.data());
}

AudioManager::AudioBufferHandle ResourceManager::getSound(entt::id_type id, std::string_view file_path) {
    if (auto cached = audio_manager_->findSound(id)) {
        return cached;
    }

    std::string_view resolved_path = file_path;
    if (resolved_path.empty()) {
        resolved_path = asset_registry_->findSoundPath(id);
    }

    if (resolved_path.empty()) {
#ifndef NDEBUG
        spdlog::error("ResourceManager::getSound: id={} 未命中缓存且 AssetRegistry 未注册路径。", id);
#endif
        return {};
    }

    return loadSound(id, resolved_path);
}

AudioManager::AudioBufferHandle ResourceManager::getSound(entt::hashed_string str_hs) {
    return getSound(str_hs.value(), str_hs.data());
}

void ResourceManager::unloadSound(entt::id_type id) {
    audio_manager_->unloadSound(id);
}

void ResourceManager::clearSounds() {
    audio_manager_->clearSounds();
}

AudioManager::AudioBufferHandle ResourceManager::loadMusic(entt::id_type id, std::string_view file_path) {
    asset_registry_->registerMusic(id, file_path);
    return audio_manager_->loadMusic(id, file_path);
}

AudioManager::AudioBufferHandle ResourceManager::loadMusic(entt::hashed_string str_hs) {
    return loadMusic(str_hs.value(), str_hs.data());
}

AudioManager::AudioBufferHandle ResourceManager::getMusic(entt::id_type id, std::string_view file_path) {
    if (auto cached = audio_manager_->findMusic(id)) {
        return cached;
    }

    std::string_view resolved_path = file_path;
    if (resolved_path.empty()) {
        resolved_path = asset_registry_->findMusicPath(id);
    }

    if (resolved_path.empty()) {
#ifndef NDEBUG
        spdlog::error("ResourceManager::getMusic: id={} 未命中缓存且 AssetRegistry 未注册路径。", id);
#endif
        return {};
    }

    return loadMusic(id, resolved_path);
}

AudioManager::AudioBufferHandle ResourceManager::getMusic(entt::hashed_string str_hs) {
    return getMusic(str_hs.value(), str_hs.data());
}

void ResourceManager::unloadMusic(entt::id_type id) {
    audio_manager_->unloadMusic(id);
}

void ResourceManager::clearMusic() {
    audio_manager_->clearMusic();
}

// --- 字体接口实现 ---
Font* ResourceManager::loadFont(entt::id_type id, int pixel_size, std::string_view file_path) {
    asset_registry_->registerFont(id, pixel_size, file_path);
    return font_manager_->loadFont(id, pixel_size, file_path);
}

Font* ResourceManager::loadFont(entt::hashed_string str_hs, int pixel_size) {
    return loadFont(str_hs.value(), pixel_size, str_hs.data());
}

Font* ResourceManager::getFont(entt::id_type id, int pixel_size, std::string_view file_path) {
    if (auto* cached = font_manager_->findFont(id, pixel_size)) {
        return cached;
    }

    std::string_view resolved_path = file_path;
    if (resolved_path.empty()) {
        resolved_path = asset_registry_->findFontPath(id, pixel_size);
    }

    if (resolved_path.empty()) {
#ifndef NDEBUG
        spdlog::error(
            "ResourceManager::getFont: id={} size={} 未命中缓存且 AssetRegistry 未注册路径。",
            id,
            pixel_size
        );
#endif
        return nullptr;
    }

    return loadFont(id, pixel_size, resolved_path);
}

Font* ResourceManager::getFont(entt::hashed_string str_hs, int pixel_size) {
    return getFont(str_hs.value(), pixel_size, str_hs.data());
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
        for (auto& info : result) {
            const std::string_view source_path = asset_registry_ ? asset_registry_->findTexturePath(info.id) : std::string_view{};
            info.source = source_path.empty() ? std::string{} : std::string(source_path);
            if (info.width > 0 && info.height > 0) {
                info.memory_bytes = static_cast<std::size_t>(info.width) * static_cast<std::size_t>(info.height) * 4u;
            } else {
                info.memory_bytes = 0;
            }
        }
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

} // namespace engine::resource
