#include "texture_loader.h"

#include "engine/render/opengl/gl_helper.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#endif
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#include <glad/glad.h>
#include <spdlog/spdlog.h>

#include <string>

namespace engine::resource {

TextureLoader::result_type TextureLoader::operator()(std::string_view file_path) const {
    if (file_path.empty()) {
        spdlog::error("TextureLoader: 文件路径为空。");
        return {};
    }

    const std::string path(file_path);

    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!data) {
        spdlog::error("加载纹理失败: '{}': {}", file_path, stbi_failure_reason());
        return {};
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

    render::opengl::logGlErrors("TextureLoader::operator()");
    stbi_image_free(data);

    if (texture_id == 0) {
        spdlog::error("TextureLoader: OpenGL 纹理创建失败: '{}'", file_path);
        return {};
    }

    auto texture_deleter = [](engine::utils::GL_Texture* texture) noexcept {
        if (!texture) {
            return;
        }
        if (texture->texture != 0) {
            glDeleteTextures(1, &texture->texture);
        }
        delete texture;
    };

    spdlog::debug("成功加载并缓存纹理: {}", file_path);
    return TextureLoader::result_type(new engine::utils::GL_Texture(texture_id, width, height), texture_deleter);
}

} // namespace engine::resource
