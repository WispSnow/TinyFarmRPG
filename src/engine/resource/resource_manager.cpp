#include "resource_manager.h"
#include "asset_registry.h"
#include "default_resource_ids.h"
#include "texture_manager.h"
#include "audio_manager.h"
#include "font_manager.h"
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
#include <vector>

namespace engine::resource {
namespace {

[[nodiscard]] bool shouldSkipMissingWebPreloadedAudio(const std::string& path) {
#if defined(__EMSCRIPTEN__)
    std::error_code ec{};
    const bool exists = std::filesystem::exists(std::filesystem::path{path}, ec);
    if (!exists || ec) {
        spdlog::debug("ResourceManager: Web 音频资源未在当前包中，已注册但暂不解码: '{}'", path);
        return true;
    }
#else
    (void)path;
#endif
    return false;
}

} // namespace

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
    clearSounds();
    clearMusic();
    clearTextures();
    spdlog::trace("ResourceManager 中的资源通过 clear() 清空。");
}

AssetRegistry& ResourceManager::getAssetRegistry() {
    return *asset_registry_;
}

const AssetRegistry& ResourceManager::getAssetRegistry() const {
    return *asset_registry_;
}

void ResourceManager::preloadRegisteredResources() {
    if (!asset_registry_) {
        return;
    }

    asset_registry_->forEachTexture([this](entt::id_type id, std::string_view path) {
        (void)texture_manager_->loadTexture(id, path);
    });
    asset_registry_->forEachSound([this](entt::id_type id, std::string_view path) {
        const std::string path_string{path};
        if (!shouldSkipMissingWebPreloadedAudio(path_string)) {
            (void)audio_manager_->loadSound(id, path);
        }
    });
    asset_registry_->forEachMusic([this](entt::id_type id, std::string_view path) {
        const std::string path_string{path};
        if (!shouldSkipMissingWebPreloadedAudio(path_string)) {
            (void)audio_manager_->loadMusic(id, path);
        }
    });
    asset_registry_->forEachFont([this](entt::id_type id, int pixel_size, std::string_view path) {
        (void)font_manager_->loadFont(id, pixel_size, path);
    });
}

void ResourceManager::preloadRegisteredAudioResources() {
    if (!asset_registry_) {
        return;
    }

    std::size_t loaded_sounds = 0;
    std::size_t loaded_music = 0;
    std::size_t already_loaded = 0;
    std::size_t skipped_missing = 0;
    std::size_t failed = 0;

    asset_registry_->forEachSound([&](entt::id_type id, std::string_view path) {
        if (audio_manager_->findSound(id)) {
            ++already_loaded;
            return;
        }
        const std::string path_string{path};
        if (shouldSkipMissingWebPreloadedAudio(path_string)) {
            ++skipped_missing;
            return;
        }
        if (audio_manager_->loadSound(id, path)) {
            ++loaded_sounds;
        } else {
            ++failed;
        }
    });

    asset_registry_->forEachMusic([&](entt::id_type id, std::string_view path) {
        if (audio_manager_->findMusic(id)) {
            ++already_loaded;
            return;
        }
        const std::string path_string{path};
        if (shouldSkipMissingWebPreloadedAudio(path_string)) {
            ++skipped_missing;
            return;
        }
        if (audio_manager_->loadMusic(id, path)) {
            ++loaded_music;
        } else {
            ++failed;
        }
    });

    spdlog::info(
        "ResourceManager: registered audio preload complete (sounds={}, music={}, already_loaded={}, skipped_missing={}, failed={}).",
        loaded_sounds,
        loaded_music,
        already_loaded,
        skipped_missing,
        failed);
#if defined(__EMSCRIPTEN__)
    if (skipped_missing > 0 && failed == 0) {
        spdlog::info(
            "ResourceManager: Web audio release policy deferred {} registered audio assets outside audio-core; failed=0.",
            skipped_missing);
    }
#endif
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

    loadStringMap("sound", [this](entt::id_type id, const std::string& path_value) {
        asset_registry_->registerSound(id, path_value);
        if (!shouldSkipMissingWebPreloadedAudio(path_value)) {
            (void)audio_manager_->loadSound(id, path_value);
        }
    });
    loadStringMap("music", [this](entt::id_type id, const std::string& path_value) {
        asset_registry_->registerMusic(id, path_value);
        if (!shouldSkipMissingWebPreloadedAudio(path_value)) {
            (void)audio_manager_->loadMusic(id, path_value);
        }
    });
    loadStringMap("texture", [this](entt::id_type id, const std::string& path_value) { loadTexture(id, path_value); });

    if (const auto it = json.find("font"); it != json.end()) {
        if (!it->is_object()) {
            spdlog::warn("资源映射文件 '{}' 的 'font' 字段格式无效 (应为对象)。", file_path);
        } else {
            const auto parse_positive_size = [](const nlohmann::json& size_value) -> int {
                if (const auto* v = size_value.get_ptr<const nlohmann::json::number_integer_t*>()) {
                    return (*v > 0) ? static_cast<int>(*v) : 0;
                }
                if (const auto* v = size_value.get_ptr<const nlohmann::json::number_unsigned_t*>()) {
                    return (*v > 0) ? static_cast<int>(*v) : 0;
                }
                return 0;
            };

            for (const auto& [key, value] : it->items()) {
                const entt::id_type id = entt::hashed_string{key.c_str(), key.size()};
                std::string font_path{};
                std::vector<int> point_sizes{};

                const auto add_point_size = [&point_sizes](const int candidate) {
                    if (candidate <= 0) {
                        return;
                    }
                    if (std::find(point_sizes.begin(), point_sizes.end(), candidate) == point_sizes.end()) {
                        point_sizes.push_back(candidate);
                    }
                };

                if (value.is_object()) {
                    if (const auto path_it = value.find("path"); path_it != value.end() && path_it->is_string()) {
                        font_path = path_it->get<std::string>();
                    } else if (
                        const auto legacy_path_it = value.find("file_path");
                        legacy_path_it != value.end() && legacy_path_it->is_string()
                    ) {
                        font_path = legacy_path_it->get<std::string>();
                    }

                    if (const auto size_it = value.find("size"); size_it != value.end()) {
                        add_point_size(parse_positive_size(*size_it));
                    }
                    if (const auto legacy_size_it = value.find("point_size"); legacy_size_it != value.end()) {
                        add_point_size(parse_positive_size(*legacy_size_it));
                    }
                    if (const auto sizes_it = value.find("sizes"); sizes_it != value.end()) {
                        if (!sizes_it->is_array()) {
                            spdlog::warn(
                                "资源映射文件 '{}' 的 'font.{}.sizes' 格式无效 (应为数组)。",
                                file_path,
                                key
                            );
                        } else {
                            for (const auto& size_item : *sizes_it) {
                                add_point_size(parse_positive_size(size_item));
                            }
                        }
                    }
                } else if (value.is_array() && value.size() >= 2) {
                    const auto& first = value[0];
                    const auto& second = value[1];
                    if (first.is_string()) {
                        font_path = first.get<std::string>();
                        add_point_size(parse_positive_size(second));
                    } else if (second.is_string()) {
                        font_path = second.get<std::string>();
                        add_point_size(parse_positive_size(first));
                    }
                }

                if (font_path.empty() || point_sizes.empty()) {
                    spdlog::warn("资源映射文件 '{}' 的 'font.{}' 缺少有效的 path/size(s)，已跳过。", file_path, key);
                    continue;
                }

                for (const int point_size : point_sizes) {
                    loadFont(id, point_size, font_path);
                }
            }
        }
    }

    // 核心引擎资源兜底：优先使用 mapping 中的注册条目，仅在缺失时回落到内置默认值。
    if (asset_registry_->findTexturePath(engine::resource::defaults::CIRCLE_TEXTURE_ID).empty()) {
        spdlog::warn(
            "资源映射文件 '{}' 未配置默认圆形纹理 '{}'(id={})，回退到内置路径 '{}'",
            file_path,
            engine::resource::defaults::CIRCLE_TEXTURE_KEY,
            engine::resource::defaults::CIRCLE_TEXTURE_ID,
            engine::resource::defaults::CIRCLE_TEXTURE_PATH
        );
        loadTexture(engine::resource::defaults::CIRCLE_TEXTURE_ID, engine::resource::defaults::CIRCLE_TEXTURE_PATH);
    }

    if (asset_registry_->findFontPath(
            engine::resource::defaults::UI_DEFAULT_FONT_ID,
            engine::resource::defaults::UI_DEFAULT_FONT_SIZE_PX
        ).empty()) {
        spdlog::warn(
            "资源映射文件 '{}' 未配置默认UI字体 '{}'(id={}, size={})，回退到内置路径 '{}'",
            file_path,
            engine::resource::defaults::UI_DEFAULT_FONT_KEY,
            engine::resource::defaults::UI_DEFAULT_FONT_ID,
            engine::resource::defaults::UI_DEFAULT_FONT_SIZE_PX,
            engine::resource::defaults::UI_DEFAULT_FONT_PATH
        );
        loadFont(
            engine::resource::defaults::UI_DEFAULT_FONT_ID,
            engine::resource::defaults::UI_DEFAULT_FONT_SIZE_PX,
            engine::resource::defaults::UI_DEFAULT_FONT_PATH
        );
    }
}

// --- 纹理接口实现 ---
TextureHandle ResourceManager::loadTexture(entt::id_type id, std::string_view file_path) {
    // 构造函数已经确保了 texture_manager_ 不为空，因此不需要再进行if检查，以免性能浪费
    asset_registry_->registerTexture(id, file_path);
    return texture_manager_->loadTexture(id, file_path);
}

TextureHandle ResourceManager::loadTextureFromDecoded(entt::id_type id,
                                                      std::string_view file_path,
                                                      const DecodedImage& decoded) {
    asset_registry_->registerTexture(id, file_path);
    return texture_manager_->loadTextureFromDecoded(id, file_path, decoded);
}

TextureHandle ResourceManager::findLoadedTexture(entt::id_type id) {
    return texture_manager_->findTexture(id);
}

TextureHandle ResourceManager::getTexture(entt::id_type id) {
    if (auto cached = texture_manager_->findTexture(id)) {
        return cached;
    }

#ifndef NDEBUG
    const std::string_view registered_path = asset_registry_->findTexturePath(id);
    if (registered_path.empty()) {
        spdlog::error("ResourceManager::getTexture: id={} 未命中缓存且 AssetRegistry 未注册路径。", id);
    } else {
        spdlog::error(
            "ResourceManager::getTexture: id={} 未命中缓存，资源未预加载。registered_path='{}'",
            id,
            registered_path
        );
    }
#endif
    return {};
}

glm::vec2 ResourceManager::getTextureSize(entt::id_type id) {
    if (const auto texture = getTexture(id)) {
        return glm::vec2(texture->width, texture->height);
    }
    return glm::vec2(0.0f, 0.0f);
}

void ResourceManager::unloadTexture(entt::id_type id) {
    texture_manager_->unloadTexture(id);
}

void ResourceManager::clearTextures() {
    texture_manager_->clearTextures();
}

// --- 音频接口实现 ---
AudioBufferHandle ResourceManager::loadSound(entt::id_type id, std::string_view file_path) {
    asset_registry_->registerSound(id, file_path);
    return audio_manager_->loadSound(id, file_path);
}

AudioBufferHandle ResourceManager::getSound(entt::id_type id) {
    if (auto cached = audio_manager_->findSound(id)) {
        return cached;
    }

#ifndef NDEBUG
    const std::string_view registered_path = asset_registry_->findSoundPath(id);
    if (registered_path.empty()) {
        spdlog::error("ResourceManager::getSound: id={} 未命中缓存且 AssetRegistry 未注册路径。", id);
    } else {
        spdlog::error(
            "ResourceManager::getSound: id={} 未命中缓存，资源未预加载。registered_path='{}'",
            id,
            registered_path
        );
    }
#endif
    return {};
}

void ResourceManager::unloadSound(entt::id_type id) {
    audio_manager_->unloadSound(id);
}

void ResourceManager::clearSounds() {
    audio_manager_->clearSounds();
}

AudioBufferHandle ResourceManager::loadMusic(entt::id_type id, std::string_view file_path) {
    asset_registry_->registerMusic(id, file_path);
    return audio_manager_->loadMusic(id, file_path);
}

AudioBufferHandle ResourceManager::getMusic(entt::id_type id) {
    if (auto cached = audio_manager_->findMusic(id)) {
        return cached;
    }

#ifndef NDEBUG
    const std::string_view registered_path = asset_registry_->findMusicPath(id);
    if (registered_path.empty()) {
        spdlog::error("ResourceManager::getMusic: id={} 未命中缓存且 AssetRegistry 未注册路径。", id);
    } else {
        spdlog::error(
            "ResourceManager::getMusic: id={} 未命中缓存，资源未预加载。registered_path='{}'",
            id,
            registered_path
        );
    }
#endif
    return {};
}

void ResourceManager::unloadMusic(entt::id_type id) {
    audio_manager_->unloadMusic(id);
}

void ResourceManager::clearMusic() {
    audio_manager_->clearMusic();
}

// --- 字体接口实现 ---
FontHandle ResourceManager::loadFont(entt::id_type id, int pixel_size, std::string_view file_path) {
    asset_registry_->registerFont(id, pixel_size, file_path);
    return font_manager_->loadFont(id, pixel_size, file_path);
}

FontHandle ResourceManager::getFont(entt::id_type id, int pixel_size) {
    if (auto* cached = font_manager_->findFont(id, pixel_size)) {
        return cached;
    }

#ifndef NDEBUG
    const std::string_view registered_path = asset_registry_->findFontPath(id, pixel_size);
    if (registered_path.empty()) {
        spdlog::error(
            "ResourceManager::getFont: id={} size={} 未命中缓存且 AssetRegistry 未注册路径。",
            id,
            pixel_size
        );
    } else {
        spdlog::error(
            "ResourceManager::getFont: id={} size={} 未命中缓存，资源未预加载。registered_path='{}'",
            id,
            pixel_size,
            registered_path
        );
    }
#endif
    return nullptr;
}

void ResourceManager::unloadFont(entt::id_type id, int pixel_size) {
    font_manager_->unloadFont(id, pixel_size);
    // 顺序约束：先释放字体，再同步通知监听者清理缓存（例如 TextRenderer 的布局缓存）。
    // 监听者不得在该回调链中重入 ResourceManager 的 load/unload/clear 系列接口，
    // 以避免 trigger 同步分发下的资源重入修改风险。
    dispatcher_->trigger<engine::utils::FontUnloadedEvent>(engine::utils::FontUnloadedEvent{id, pixel_size});
    spdlog::trace("卸载字体: {} ({}px)", id, pixel_size);
}

void ResourceManager::clearFonts() {
    font_manager_->clearFonts();
    // 与 unloadFont 保持一致：先清空缓存，再同步通知监听者立即失效相关布局状态。
    // 同样禁止监听者在回调链中重入 ResourceManager 的 load/unload/clear 接口。
    dispatcher_->trigger<engine::utils::FontsClearedEvent>();
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
        for (auto& info : result) {
            const std::string_view source_path = asset_registry_ ? asset_registry_->findSoundPath(info.id) : std::string_view{};
            info.source = source_path.empty() ? std::string{} : std::string(source_path);
            info.memory_bytes = info.sample_count * sizeof(float);
        }
        std::ranges::sort(result, {}, &AudioDebugInfo::id);
    }
    return result;
}

std::vector<AudioDebugInfo> ResourceManager::getMusicDebugInfo() const {
    std::vector<AudioDebugInfo> result;
    if (audio_manager_) {
        audio_manager_->collectMusicDebugInfo(result);
        for (auto& info : result) {
            const std::string_view source_path = asset_registry_ ? asset_registry_->findMusicPath(info.id) : std::string_view{};
            info.source = source_path.empty() ? std::string{} : std::string(source_path);
            info.memory_bytes = info.sample_count * sizeof(float);
        }
        std::ranges::sort(result, {}, &AudioDebugInfo::id);
    }
    return result;
}

} // namespace engine::resource
