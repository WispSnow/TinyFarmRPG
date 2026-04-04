#include "engine/ui/rmlui/rml_ui_render_backend_gl.h"

#include "engine/ui/rmlui/render_interface_gl3_stb.h"

#include <RmlUi/Core/Context.h>
#include <spdlog/spdlog.h>

#include <algorithm>

namespace engine::ui::rmlui {

std::unique_ptr<RmlUiRenderBackendGl> RmlUiRenderBackendGl::create() {
    auto backend = std::unique_ptr<RmlUiRenderBackendGl>(new RmlUiRenderBackendGl());
    if (!backend->init()) {
        return nullptr;
    }
    return backend;
}

RmlUiRenderBackendGl::~RmlUiRenderBackendGl() = default;

bool RmlUiRenderBackendGl::init() {
    render_interface_ = std::make_unique<RenderInterface_GL3_STB>();
    if (!render_interface_ || !static_cast<bool>(*render_interface_)) {
        spdlog::error("RmlUiRenderBackendGl::init failed: render interface creation failed.");
        render_interface_.reset();
        return false;
    }
    return true;
}

void RmlUiRenderBackendGl::setLogicalSize(int width, int height) {
    logical_width_ = std::max(width, 0);
    logical_height_ = std::max(height, 0);
}

void RmlUiRenderBackendGl::setTextureFilterMode(RmlUiTextureFilterMode mode) {
    if (render_interface_) {
        render_interface_->setTextureFilterMode(mode);
    }
}

RmlUiTextureFilterMode RmlUiRenderBackendGl::getTextureFilterMode() const {
    if (!render_interface_) {
        return RmlUiTextureFilterMode::Nearest;
    }
    return render_interface_->getTextureFilterMode();
}

void RmlUiRenderBackendGl::render(Rml::Context& context, const RmlUiViewport& viewport) {
    if (!render_interface_) {
        return;
    }

    const int viewport_width = std::max(viewport.width, 1);
    const int viewport_height = std::max(viewport.height, 1);

    if (logical_width_ > 0 && logical_height_ > 0) {
        render_interface_->SetViewport(logical_width_, logical_height_, 0, 0);
    } else {
        render_interface_->SetViewport(viewport_width, viewport_height, viewport.offset_x, viewport.offset_y);
    }

    render_interface_->BeginFrame();
    context.Render();

    if (logical_width_ > 0 && logical_height_ > 0) {
        render_interface_->SetViewport(viewport_width, viewport_height, viewport.offset_x, viewport.offset_y);
        render_interface_->EndFrame();
        render_interface_->SetViewport(logical_width_, logical_height_, 0, 0);
    } else {
        render_interface_->EndFrame();
    }
}

} // namespace engine::ui::rmlui
