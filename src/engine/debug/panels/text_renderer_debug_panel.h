#pragma once

#include "engine/debug/debug_panel.h"
#include "engine/utils/defs.h"

namespace engine::render {
class TextRenderer;
}

namespace engine::debug {

class TextRendererDebugPanel final : public DebugPanel {
    engine::render::TextRenderer& text_renderer_;

public:
    explicit TextRendererDebugPanel(engine::render::TextRenderer& text_renderer);

    std::string_view name() const override;
    void draw(bool& is_open) override;

private:
    bool drawTextParamsSection(const char* label, engine::utils::TextRenderParams& params);
    bool drawColorOptions(engine::utils::ColorOptions& options);
    bool drawShadowOptions(engine::utils::ShadowOptions& options);
    bool drawLayoutOptions(engine::utils::LayoutOptions& options);
};

} // namespace engine::debug
