#pragma once

#include "RmlUi_Renderer_GL3.h"
#include "engine/ui/rmlui/rml_ui_texture_filter_mode.h"

#include <unordered_set>

namespace engine::ui::rmlui {

class RmlGeneratedImageRegistry;

class RenderInterface_GL3_STB final : public RenderInterface_GL3 {
public:
    RenderInterface_GL3_STB() = default;

    void setTextureFilterMode(RmlUiTextureFilterMode mode);
    [[nodiscard]] RmlUiTextureFilterMode getTextureFilterMode() const { return texture_filter_mode_; }
    void setGeneratedImageRegistry(RmlGeneratedImageRegistry* registry) { generated_image_registry_ = registry; }

    Rml::TextureHandle LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) override;
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source_data,
                                       Rml::Vector2i source_dimensions) override;
    void ReleaseTexture(Rml::TextureHandle texture_handle) override;

private:
    void applyTextureSampling(Rml::TextureHandle texture_handle) const;

    RmlUiTextureFilterMode texture_filter_mode_{RmlUiTextureFilterMode::Nearest};
    RmlGeneratedImageRegistry* generated_image_registry_{nullptr};
    std::unordered_set<Rml::TextureHandle> tracked_texture_handles_{};
};

} // namespace engine::ui::rmlui
