#pragma once

#include "RmlUi_Renderer_GL3.h"

namespace engine::ui::rmlui {

class RenderInterface_GL3_STB final : public RenderInterface_GL3 {
public:
    RenderInterface_GL3_STB() = default;

    Rml::TextureHandle LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) override;
};

} // namespace engine::ui::rmlui
