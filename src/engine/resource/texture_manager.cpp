#include "texture_manager.h"
#include "engine/render/opengl/gl_helper.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <spdlog/spdlog.h>
#include <entt/core/hashed_string.hpp>
#include <vector>

namespace engine::resource {

engine::utils::GL_Texture* TextureManager::loadTexture(entt::id_type id, std::string_view file_path) {
    // 检查是否已加载
    auto it = textures_.find(id);
    if (it != textures_.end()) {
        return it->second.texture.get();
    }

    // 如果没加载则尝试加载纹理
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* data = stbi_load(file_path.data(), &width, &height, &channels, STBI_rgb_alpha);
    if (!data) {
        spdlog::error("加载纹理失败: '{}': {}", file_path.data(), stbi_failure_reason());
        return nullptr;
    }

    GLuint texture_id = 0;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    {
        render::opengl::ScopedGLUnpackAlignment unpack_alignment(1);
        // 以 sRGB 格式上传纹理，这样合成着色器可以在线性空间工作，
        // 同时最终输出仍然能够正确处理伽马校正。
        glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    render::opengl::logGlErrors("TextureManager::loadTexture");
    stbi_image_free(data);

    // 使用带有自定义删除器的 unique_ptr 存储加载的纹理
    TextureResource resource{};
    resource.texture = TextureManager::TexturePtr(new engine::utils::GL_Texture(texture_id, width, height));
    resource.source_path = std::string(file_path);
    resource.memory_bytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u;

    auto [inserted_it, _] = textures_.emplace(id, std::move(resource));
    spdlog::debug("成功加载并缓存纹理: {}", file_path.data());

    return inserted_it->second.texture.get();
}

engine::utils::GL_Texture* TextureManager::loadTexture(entt::hashed_string str_hs) {
    return loadTexture(str_hs.value(), str_hs.data());
}

engine::utils::GL_Texture* TextureManager::getTexture(entt::id_type id, std::string_view file_path) {
    // 查找现有纹理
    auto it = textures_.find(id);
    if (it != textures_.end()) {
        return it->second.texture.get();
    }

    // 如果未找到，判断是否提供了file_path
    if (file_path.empty()) {
        spdlog::error("纹理 '{}' 未找到缓存，且未提供文件路径，返回nullptr。", id);
        return nullptr;
    }

    spdlog::info("纹理 '{}' 未找到缓存，尝试从文件路径加载。", id);
    return loadTexture(id, file_path);
}

engine::utils::GL_Texture* TextureManager::getTexture(entt::hashed_string str_hs) {
    return getTexture(str_hs.value(), str_hs.data());
}

glm::vec2 TextureManager::getTextureSize(entt::id_type id, std::string_view file_path) {
    // 获取纹理
    engine::utils::GL_Texture* texture = getTexture(id, file_path);
    if (!texture) {
        spdlog::error("无法获取纹理: {}", file_path.data());
        return glm::vec2(0.0f, 0.0f);
    }
    return glm::vec2(texture->width, texture->height);
}

glm::vec2 TextureManager::getTextureSize(entt::hashed_string str_hs) {
    return getTextureSize(str_hs.value(), str_hs.data());
}

void TextureManager::unloadTexture(entt::id_type id) {
    auto it = textures_.find(id);
    if (it != textures_.end()) {
        spdlog::debug("卸载纹理: id = {}", id);
        textures_.erase(it); // unique_ptr 通过自定义删除器处理删除
    } else {
        spdlog::warn("尝试卸载不存在的纹理: id = {}", id);
    }
}

void TextureManager::clearTextures() {
    if (!textures_.empty()) {
        spdlog::debug("正在清除所有 {} 个缓存的纹理。", textures_.size());
        textures_.clear(); // unique_ptr 处理所有元素的删除
    }
}

void TextureManager::collectDebugInfo(std::vector<TextureDebugInfo>& out) const {
    out.clear();
    out.reserve(textures_.size());
    for (const auto& [id, resource] : textures_) {
        if (!resource.texture) {
            continue;
        }
        TextureDebugInfo info{};
        info.id = id;
        info.texture = resource.texture->texture;
        info.width = resource.texture->width;
        info.height = resource.texture->height;
        info.source = resource.source_path;
        info.memory_bytes = resource.memory_bytes;
        out.push_back(info);
    }
}

} // namespace engine::resource
