#pragma once

#include "engine/resource/decoded_image.h"

#include <optional>
#include <string_view>

namespace engine::resource {

class ImageDecodeService final {
public:
    [[nodiscard]] static std::optional<DecodedImage> decodeRGBA(std::string_view file_path);
};

} // namespace engine::resource

