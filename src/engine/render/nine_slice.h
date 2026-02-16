#pragma once

#include <array>
#include <cstdint>
#include <glm/vec2.hpp>
#include "engine/utils/math.h"

namespace engine::render {

struct NineSliceMargins {
    float left{0.0f};
    float top{0.0f};
    float right{0.0f};
    float bottom{0.0f};

    [[nodiscard]] bool isValid() const {
        return left >= 0.0f && top >= 0.0f && right >= 0.0f && bottom >= 0.0f;
    }
};

enum class NineSliceSection : std::uint8_t {
    TopLeft = 0,
    TopCenter,
    TopRight,
    MiddleLeft,
    MiddleCenter,
    MiddleRight,
    BottomLeft,
    BottomCenter,
    BottomRight
};

class NineSlice {
    using SliceArray = std::array<engine::utils::Rect, 9>;

    engine::utils::Rect source_rect_{};
    NineSliceMargins margins_{};
    SliceArray slices_{};
    glm::vec2 min_size_{0.0f, 0.0f};
    bool valid_{false};
    
public:
    NineSlice() = default;
    NineSlice(engine::utils::Rect source_rect,
              NineSliceMargins margins);

    [[nodiscard]] bool isValid() const { return valid_; }
    [[nodiscard]] const engine::utils::Rect& getSourceRect() const { return source_rect_; }
    [[nodiscard]] const NineSliceMargins& getMargins() const { return margins_; }
    [[nodiscard]] const SliceArray& getSlices() const { return slices_; }
    [[nodiscard]] engine::utils::Rect getSliceRect(NineSliceSection section) const;
    [[nodiscard]] glm::vec2 getMinimumSize() const { return min_size_; }

private:
    void buildSlices();
};

} // namespace engine::render


