#include "texture_manager.h"

#include <spdlog/spdlog.h>
#include <entt/core/hashed_string.hpp>

#include <cassert>

namespace engine::resource {

TextureHandle TextureManager::loadTexture(entt::id_type id, std::string_view file_path) {
    auto [it, _] = texture_cache_.load(id, file_path);
    TextureHandle handle = it->second;
    if (!handle) {
        texture_cache_.erase(id);
        spdlog::error("TextureManager: 纹理加载失败，已移除无效缓存条目: id={}, path='{}'", id, file_path);
    }
    return handle;
}

TextureHandle TextureManager::loadTexture(entt::hashed_string str_hs) {
    return loadTexture(str_hs.value(), str_hs.data());
}

TextureHandle TextureManager::findTexture(entt::id_type id) {
    return texture_cache_[id];
}

void TextureManager::unloadTexture(entt::id_type id) {
    if (texture_cache_.erase(id) > 0U) {
        spdlog::debug("卸载纹理: id = {}", id);
    } else {
        spdlog::warn("尝试卸载不存在的纹理: id = {}", id);
    }
}

void TextureManager::clearTextures() {
#ifndef NDEBUG
    for (const auto [id, handle] : texture_cache_) {
        if (!handle) {
            continue;
        }
        const long observed_use_count = handle.handle().use_count();
        // EnTT 迭代器按值返回 resource 句柄，当前 probe 会额外引入 1 次 shared_ptr 引用。
        constexpr long EXPECTED_USE_COUNT_DURING_PROBE = 2;
        if (observed_use_count > EXPECTED_USE_COUNT_DURING_PROBE) {
            spdlog::error(
                "TextureManager::clearTextures: 检测到外部纹理句柄仍被持有 id={} gl_handle={} use_count={} external_refs={}",
                id,
                handle->texture,
                observed_use_count,
                observed_use_count - EXPECTED_USE_COUNT_DURING_PROBE
            );
            assert(observed_use_count <= EXPECTED_USE_COUNT_DURING_PROBE && "TextureHandle leaked across clear boundary");
        }
    }
#endif

    if (!texture_cache_.empty()) {
        spdlog::debug("正在清除所有 {} 个缓存的纹理。", texture_cache_.size());
        texture_cache_.clear();
    }
}

void TextureManager::collectDebugInfo(std::vector<TextureDebugInfo>& out) const {
    out.clear();
    out.reserve(texture_cache_.size());
    for (const auto [id, handle] : texture_cache_) {
        if (!handle) {
            continue;
        }
        TextureDebugInfo info{};
        info.id = id;
        info.texture = handle->texture;
        info.width = handle->width;
        info.height = handle->height;
        out.push_back(info);
    }
}

} // namespace engine::resource
