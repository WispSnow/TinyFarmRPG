#include "image.h"
#include <utility>

namespace engine::render {

Image::Image(std::string_view texture_path, engine::utils::Rect source_rect, bool is_flipped)
    : texture_path_(texture_path),
      texture_id_(entt::hashed_string{texture_path.data(), texture_path.size()}),
      source_rect_(source_rect),
      is_flipped_(is_flipped) {}

Image::Image(std::string_view texture_path, entt::id_type texture_id, engine::utils::Rect source_rect, bool is_flipped)
    : texture_path_(texture_path),
      texture_id_(texture_id),
      source_rect_(source_rect),
      is_flipped_(is_flipped) {}

Image::Image(entt::id_type texture_id, engine::utils::Rect source_rect, bool is_flipped)
    : texture_id_(texture_id),
      source_rect_(source_rect),
      is_flipped_(is_flipped) {}

void Image::setTexture(std::string_view texture_path) {
    texture_path_ = texture_path;
    texture_id_ = entt::hashed_string{texture_path.data(), texture_path.size()};
    nine_slice_dirty_ = true;
}

void Image::setTexture(std::string_view texture_path, entt::id_type texture_id) {
    texture_path_ = texture_path;
    texture_id_ = texture_id;
    nine_slice_dirty_ = true;
}

void Image::setTextureId(entt::id_type texture_id) {
    texture_id_ = texture_id;
    nine_slice_dirty_ = true;
}

void Image::setSourceRect(const engine::utils::Rect& source_rect) {
    source_rect_ = source_rect;
    nine_slice_dirty_ = true;
}

void Image::setFlipped(bool flipped) {
    is_flipped_ = flipped;
}

void Image::setNineSliceMargins(std::optional<NineSliceMargins> margins) {
    nine_slice_margins_ = std::move(margins);
    nine_slice_dirty_ = true;
}

bool Image::hasNineSlice() const {
    return nine_slice_margins_.has_value() && nine_slice_margins_->isValid();
}

void Image::markNineSliceDirty() const {
    if (hasNineSlice()) {
        nine_slice_dirty_ = true;
    }
}

const NineSlice* Image::ensureNineSlice() const {
    if (!hasNineSlice()) {
        nine_slice_cache_.reset();
        nine_slice_dirty_ = false;
        return nullptr;
    }
    if (!nine_slice_dirty_ && nine_slice_cache_ && nine_slice_cache_->isValid()) {
        return &*nine_slice_cache_;
    }
    NineSlice slice(source_rect_, *nine_slice_margins_);
    if (slice.isValid()) {
        nine_slice_cache_ = std::move(slice);
    } else {
        nine_slice_cache_.reset();
    }
    nine_slice_dirty_ = false;
    return nine_slice_cache_ && nine_slice_cache_->isValid() ? &*nine_slice_cache_ : nullptr;
}

} // namespace engine::render

