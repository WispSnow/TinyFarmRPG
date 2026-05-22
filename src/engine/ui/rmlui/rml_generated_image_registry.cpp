#include "engine/ui/rmlui/rml_generated_image_registry.h"

#include <utility>

namespace engine::ui::rmlui {

namespace {

constexpr std::string_view GENERATED_SOURCE_PREFIX = "generated://";

} // namespace

RmlGeneratedImageRegistry::Registration::Registration(RmlGeneratedImageRegistry& registry, std::string source_uri)
    : registry_(&registry),
      source_uri_(std::move(source_uri)) {
}

RmlGeneratedImageRegistry::Registration::~Registration() {
    reset();
}

RmlGeneratedImageRegistry::Registration::Registration(Registration&& other) noexcept
    : registry_(other.registry_),
      source_uri_(std::move(other.source_uri_)) {
    other.registry_ = nullptr;
}

RmlGeneratedImageRegistry::Registration&
RmlGeneratedImageRegistry::Registration::operator=(Registration&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    reset();
    registry_ = other.registry_;
    source_uri_ = std::move(other.source_uri_);
    other.registry_ = nullptr;
    return *this;
}

void RmlGeneratedImageRegistry::Registration::reset() {
    if (registry_ && !source_uri_.empty()) {
        registry_->unregisterImage(source_uri_);
    }
    registry_ = nullptr;
    source_uri_.clear();
}

RmlGeneratedImageRegistry::Registration
RmlGeneratedImageRegistry::registerImage(
    std::string source_uri,
    engine::resource::DecodedImage image,
    std::optional<RmlUiTextureFilterMode> texture_filter_override) {
    if (source_uri.empty() || !image.valid()) {
        return {};
    }

    const std::string handle_source = source_uri;
    images_[std::move(source_uri)] = ImageEntry{
        .image = std::move(image),
        .texture_filter_override = texture_filter_override,
    };
    return Registration{*this, handle_source};
}

void RmlGeneratedImageRegistry::unregisterImage(std::string_view source_uri) {
    if (source_uri.empty()) {
        return;
    }
    images_.erase(std::string{source_uri});
}

const engine::resource::DecodedImage* RmlGeneratedImageRegistry::find(std::string_view source_uri) const {
    if (source_uri.empty()) {
        return nullptr;
    }
    const auto it = images_.find(std::string{source_uri});
    return it == images_.end() ? nullptr : &it->second.image;
}

std::optional<RmlUiTextureFilterMode>
RmlGeneratedImageRegistry::textureFilterOverrideFor(std::string_view source_uri) const {
    if (source_uri.empty()) {
        return std::nullopt;
    }
    const auto it = images_.find(std::string{source_uri});
    return it == images_.end() ? std::nullopt : it->second.texture_filter_override;
}

bool RmlGeneratedImageRegistry::isGeneratedSource(std::string_view source_uri) {
    return source_uri.rfind(GENERATED_SOURCE_PREFIX, 0) == 0;
}

} // namespace engine::ui::rmlui
