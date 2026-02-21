#include "engine/resource/image_decode_service.h"
#include "engine/resource/stb_image_mutex.h"

#include <stb_image.h>
#include <string>

namespace engine::resource {

std::optional<DecodedImage> ImageDecodeService::decodeRGBA(std::string_view file_path) {
    if (file_path.empty()) {
        return std::nullopt;
    }

    const std::string path(file_path);
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* data = nullptr;
    {
        std::lock_guard lock(detail::stbImageMutex());
        data = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    }
    if (!data) {
        return std::nullopt;
    }

    DecodedImage image{};
    image.width = width;
    image.height = height;
    image.channels = 4;
    image.pixels.assign(data, data + (static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U));

    stbi_image_free(data);
    if (!image.valid()) {
        return std::nullopt;
    }
    return image;
}

} // namespace engine::resource
