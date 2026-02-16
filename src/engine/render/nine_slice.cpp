#include "nine_slice.h"
#include <algorithm>
#include <spdlog/spdlog.h>

namespace engine::render {

namespace {

[[nodiscard]] constexpr std::size_t toIndex(NineSliceSection section) {
    return static_cast<std::size_t>(section);
}

[[nodiscard]] constexpr bool isNonNegative(float value) {
    return value >= 0.0f;
}

}

NineSlice::NineSlice(engine::utils::Rect source_rect,
                     NineSliceMargins margins)
    : source_rect_(std::move(source_rect)),
      margins_(std::move(margins)) {
    buildSlices();
}

engine::utils::Rect NineSlice::getSliceRect(NineSliceSection section) const {
    return slices_.at(toIndex(section));
}

void NineSlice::buildSlices() {
    valid_ = false;

    if (!margins_.isValid()) {
        spdlog::error("NineSlice: 边框厚度无效 (left: {}, top: {}, right: {}, bottom: {})",
                      margins_.left, margins_.top, margins_.right, margins_.bottom);
        return;
    }

    const float width = source_rect_.size.x;
    const float height = source_rect_.size.y;

    if (!isNonNegative(width) || !isNonNegative(height) || width == 0.0f || height == 0.0f) {
        spdlog::error("NineSlice: 源矩形尺寸无效 ({}, {})。", width, height);
        return;
    }

    const float left = margins_.left;
    const float right = margins_.right;
    const float top = margins_.top;
    const float bottom = margins_.bottom;

    if (left + right > width) {
        spdlog::error("NineSlice: 水平方向的边框厚度超过纹理宽度 (L+R={}, width={}).", left + right, width);
        return;
    }

    if (top + bottom > height) {
        spdlog::error("NineSlice: 垂直方向的边框厚度超过纹理高度 (T+B={}, height={}).", top + bottom, height);
        return;
    }

    const float center_width = std::max(0.0f, width - left - right);
    const float center_height = std::max(0.0f, height - top - bottom);

    const float x = source_rect_.pos.x;
    const float y = source_rect_.pos.y;

    slices_[toIndex(NineSliceSection::TopLeft)] = {{x, y}, {left, top}};
    slices_[toIndex(NineSliceSection::TopCenter)] = {{x + left, y}, {center_width, top}};
    slices_[toIndex(NineSliceSection::TopRight)] = {{x + width - right, y}, {right, top}};

    slices_[toIndex(NineSliceSection::MiddleLeft)] = {{x, y + top}, {left, center_height}};
    slices_[toIndex(NineSliceSection::MiddleCenter)] = {{x + left, y + top}, {center_width, center_height}};
    slices_[toIndex(NineSliceSection::MiddleRight)] = {{x + width - right, y + top}, {right, center_height}};

    slices_[toIndex(NineSliceSection::BottomLeft)] = {{x, y + height - bottom}, {left, bottom}};
    slices_[toIndex(NineSliceSection::BottomCenter)] = {{x + left, y + height - bottom}, {center_width, bottom}};
    slices_[toIndex(NineSliceSection::BottomRight)] = {{x + width - right, y + height - bottom}, {right, bottom}};

    min_size_ = {left + right, top + bottom};
    valid_ = true;
}

} // namespace engine::render


