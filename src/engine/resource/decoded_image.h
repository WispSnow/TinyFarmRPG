#pragma once

#include <cstdint>
#include <vector>

namespace engine::resource {

struct DecodedImage {
    int width{0};
    int height{0};
    int channels{0};
    std::vector<std::uint8_t> pixels{};

    [[nodiscard]] bool valid() const noexcept {
        return width > 0 && height > 0 && channels > 0 && !pixels.empty();
    }
};

} // namespace engine::resource

