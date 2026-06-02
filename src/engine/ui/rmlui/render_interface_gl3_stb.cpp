#include "engine/ui/rmlui/render_interface_gl3_stb.h"

#include "engine/resource/stb_image_mutex.h"
#include "engine/ui/rmlui/rml_generated_image_registry.h"

#include "engine/platform/gl_platform.h"
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/FileInterface.h>
#include <RmlUi/Core/Log.h>

#include <stb_image.h>

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace engine::ui::rmlui {

namespace {

[[nodiscard]] GLuint toTextureId(Rml::TextureHandle texture_handle) {
    return static_cast<GLuint>(texture_handle);
}

[[nodiscard]] GLint toGlTextureFilter(RmlUiTextureFilterMode mode) {
    switch (mode) {
    case RmlUiTextureFilterMode::Nearest:
        return GL_NEAREST;
    case RmlUiTextureFilterMode::Linear:
        return GL_LINEAR;
    }
    return GL_NEAREST;
}

[[nodiscard]] bool readFileToBuffer(const Rml::String& source, std::vector<std::uint8_t>& out_buffer) {
    Rml::FileInterface* file_interface = Rml::GetFileInterface();
    if (!file_interface) {
        Rml::Log::Message(Rml::Log::LT_ERROR, "RmlUi file interface is null.");
        return false;
    }

    const Rml::FileHandle file_handle = file_interface->Open(source);
    if (!file_handle) {
        Rml::Log::Message(Rml::Log::LT_ERROR, "Failed to open texture file: %s", source.c_str());
        return false;
    }

    file_interface->Seek(file_handle, 0, SEEK_END);
    const size_t buffer_size = file_interface->Tell(file_handle);
    file_interface->Seek(file_handle, 0, SEEK_SET);

    if (buffer_size == 0) {
        file_interface->Close(file_handle);
        Rml::Log::Message(Rml::Log::LT_ERROR, "Texture file is empty: %s", source.c_str());
        return false;
    }

    out_buffer.resize(buffer_size);
    const size_t read_size = file_interface->Read(out_buffer.data(), buffer_size, file_handle);
    file_interface->Close(file_handle);

    if (read_size != buffer_size) {
        Rml::Log::Message(Rml::Log::LT_ERROR, "Failed to read texture file: %s", source.c_str());
        return false;
    }

    return true;
}

void premultiplyAlpha(unsigned char* pixels, int width, int height) {
    if (!pixels || width <= 0 || height <= 0) {
        return;
    }

    const size_t pixel_count = static_cast<size_t>(width) * static_cast<size_t>(height);
    for (size_t i = 0; i < pixel_count; ++i) {
        const size_t base = i * 4;
        const unsigned char alpha = pixels[base + 3];
        pixels[base + 0] = static_cast<unsigned char>((static_cast<int>(pixels[base + 0]) * static_cast<int>(alpha)) / 255);
        pixels[base + 1] = static_cast<unsigned char>((static_cast<int>(pixels[base + 1]) * static_cast<int>(alpha)) / 255);
        pixels[base + 2] = static_cast<unsigned char>((static_cast<int>(pixels[base + 2]) * static_cast<int>(alpha)) / 255);
    }
}

} // namespace

void RenderInterface_GL3_STB::setTextureFilterMode(RmlUiTextureFilterMode mode) {
    if (texture_filter_mode_ == mode) {
        return;
    }

    texture_filter_mode_ = mode;
    for (const Rml::TextureHandle texture_handle : tracked_texture_handles_) {
        applyTextureSampling(texture_handle);
    }
}

Rml::TextureHandle RenderInterface_GL3_STB::GenerateTexture(Rml::Span<const Rml::byte> source_data,
                                                            Rml::Vector2i source_dimensions) {
    const Rml::TextureHandle texture_handle = RenderInterface_GL3::GenerateTexture(source_data, source_dimensions);
    if (texture_handle) {
        tracked_texture_handles_.insert(texture_handle);
        applyTextureSampling(texture_handle);
    }
    return texture_handle;
}

void RenderInterface_GL3_STB::ReleaseTexture(Rml::TextureHandle texture_handle) {
    tracked_texture_handles_.erase(texture_handle);
    texture_filter_overrides_.erase(texture_handle);
    RenderInterface_GL3::ReleaseTexture(texture_handle);
}

RmlUiTextureFilterMode RenderInterface_GL3_STB::textureFilterModeFor(Rml::TextureHandle texture_handle) const {
    const auto override_it = texture_filter_overrides_.find(texture_handle);
    return override_it == texture_filter_overrides_.end() ? texture_filter_mode_ : override_it->second;
}

void RenderInterface_GL3_STB::applyTextureSampling(Rml::TextureHandle texture_handle) const {
    applyTextureSampling(texture_handle, textureFilterModeFor(texture_handle));
}

void RenderInterface_GL3_STB::applyTextureSampling(Rml::TextureHandle texture_handle,
                                                   RmlUiTextureFilterMode filter_mode) const {
    const GLuint texture_id = toTextureId(texture_handle);
    if (texture_id == 0) {
        return;
    }

    GLint previous_texture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);

    const GLint gl_filter = toGlTextureFilter(filter_mode);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous_texture));
}

Rml::TextureHandle RenderInterface_GL3_STB::LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) {
    if (RmlGeneratedImageRegistry::isGeneratedSource(source)) {
        if (!generated_image_registry_) {
            Rml::Log::Message(Rml::Log::LT_ERROR, "Generated image registry is null for texture: %s", source.c_str());
            return {};
        }

        const engine::resource::DecodedImage* image = generated_image_registry_->find(source);
        if (!image || image->channels != 4 || !image->valid()) {
            Rml::Log::Message(Rml::Log::LT_ERROR, "Generated texture not found or invalid: %s", source.c_str());
            return {};
        }

        std::vector<std::uint8_t> pixels = image->pixels;
        premultiplyAlpha(pixels.data(), image->width, image->height);

        texture_dimensions = {image->width, image->height};
        const Rml::TextureHandle texture_handle = this->GenerateTexture({pixels.data(), pixels.size()}, texture_dimensions);
        if (texture_handle) {
            if (const auto filter_override = generated_image_registry_->textureFilterOverrideFor(source)) {
                texture_filter_overrides_[texture_handle] = *filter_override;
                applyTextureSampling(texture_handle);
            }
        }
        return texture_handle;
    }

    std::vector<std::uint8_t> file_buffer;
    if (!readFileToBuffer(source, file_buffer)) {
        return {};
    }

    int width = 0;
    int height = 0;

    unsigned char* pixels = nullptr;
    std::string failure_reason;
    {
        std::lock_guard lock(engine::resource::detail::stbImageMutex());
        pixels = stbi_load_from_memory(
            file_buffer.data(),
            static_cast<int>(file_buffer.size()),
            &width,
            &height,
            nullptr,
            STBI_rgb_alpha);
        if (!pixels) {
            if (const char* reason = stbi_failure_reason(); reason) {
                failure_reason = reason;
            }
        }
    }

    if (!pixels || width <= 0 || height <= 0) {
        Rml::Log::Message(
            Rml::Log::LT_ERROR,
            "Failed to decode texture %s: %s",
            source.c_str(),
            failure_reason.empty() ? "unknown stb_image error" : failure_reason.c_str());
        return {};
    }

    premultiplyAlpha(pixels, width, height);

    texture_dimensions = {width, height};
    const size_t pixels_byte_size = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
    Rml::TextureHandle texture_handle = this->GenerateTexture({pixels, pixels_byte_size}, texture_dimensions);

    stbi_image_free(pixels);
    return texture_handle;
}

} // namespace engine::ui::rmlui
