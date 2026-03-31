#pragma once

#include "engine/ui/rmlui/rml_ui_texture_filter_mode.h"
#include "engine/ui/rmlui/rml_ui_viewport.h"

#include <memory>

namespace Rml {
class Context;
}

namespace engine::ui::rmlui {

class RenderInterface_GL3_STB;

class RmlUiRenderBackendGl final {
public:
    [[nodiscard]] static std::unique_ptr<RmlUiRenderBackendGl> create();
    ~RmlUiRenderBackendGl();

    RmlUiRenderBackendGl(const RmlUiRenderBackendGl&) = delete;
    RmlUiRenderBackendGl& operator=(const RmlUiRenderBackendGl&) = delete;
    RmlUiRenderBackendGl(RmlUiRenderBackendGl&&) = delete;
    RmlUiRenderBackendGl& operator=(RmlUiRenderBackendGl&&) = delete;

    [[nodiscard]] RenderInterface_GL3_STB* getRenderInterface() const { return render_interface_.get(); }
    void setLogicalSize(int width, int height);
    void setTextureFilterMode(RmlUiTextureFilterMode mode);
    [[nodiscard]] RmlUiTextureFilterMode getTextureFilterMode() const;
    void render(Rml::Context& context, const RmlUiViewport& viewport);

private:
    RmlUiRenderBackendGl() = default;
    [[nodiscard]] bool init();

    std::unique_ptr<RenderInterface_GL3_STB> render_interface_;
    int logical_width_{0};
    int logical_height_{0};
};

} // namespace engine::ui::rmlui
