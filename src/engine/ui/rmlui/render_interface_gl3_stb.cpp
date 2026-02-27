#include "engine/ui/rmlui/render_interface_gl3_stb.h"

#include "engine/resource/stb_image_mutex.h"

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

Rml::TextureHandle RenderInterface_GL3_STB::LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) {
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
    Rml::TextureHandle texture_handle = RenderInterface_GL3::GenerateTexture({pixels, pixels_byte_size}, texture_dimensions);

    stbi_image_free(pixels);
    return texture_handle;
}

} // namespace engine::ui::rmlui
