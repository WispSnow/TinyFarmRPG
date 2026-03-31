#include "engine/ui/rmlui/rml_ui_layer.h"

#include "engine/ui/rmlui/rml_ui_render_backend_gl.h"
#include "engine/ui/rmlui/rml_ui_viewport.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>

namespace engine::ui::rmlui {

std::unique_ptr<RmlUILayer> RmlUILayer::create(SDL_Window* window,
                                               int viewport_width,
                                               int viewport_height,
                                               int viewport_offset_x,
                                               int viewport_offset_y) {
    auto layer = std::unique_ptr<RmlUILayer>(new RmlUILayer());
    if (!layer->init(window, viewport_width, viewport_height, viewport_offset_x, viewport_offset_y)) {
        return nullptr;
    }
    return layer;
}

RmlUILayer::~RmlUILayer() {
    clean();
}

bool RmlUILayer::init(SDL_Window* window,
                      int viewport_width,
                      int viewport_height,
                      int viewport_offset_x,
                      int viewport_offset_y) {
    clean();

    if (!window) {
        spdlog::error("RmlUILayer::init failed: window is null.");
        return false;
    }

    window_ = window;
    render_backend_ = RmlUiRenderBackendGl::create();
    if (!render_backend_) {
        spdlog::error("RmlUILayer::init failed: render backend creation failed.");
        clean();
        return false;
    }

    runtime_ = RmlUiRuntime::create(window_, *render_backend_->getRenderInterface(), RmlUiViewport{
        .width = viewport_width,
        .height = viewport_height,
        .offset_x = viewport_offset_x,
        .offset_y = viewport_offset_y,
    });
    if (!runtime_) {
        spdlog::error("RmlUILayer::init failed: runtime creation failed.");
        clean();
        return false;
    }

    if (logical_width_ > 0 && logical_height_ > 0) {
        runtime_->setLogicalSize(logical_width_, logical_height_);
        render_backend_->setLogicalSize(logical_width_, logical_height_);
    }

    spdlog::trace("RmlUILayer initialized.");
    return true;
}

void RmlUILayer::clean() {
    runtime_.reset();
    render_backend_.reset();
    logical_width_ = 0;
    logical_height_ = 0;
    window_ = nullptr;
}

bool RmlUILayer::loadFontFace(std::string_view path) const {
    if (!runtime_) {
        return false;
    }
    return runtime_->loadFontFace(path);
}

bool RmlUILayer::processEvent(SDL_Event& event) {
    if (!runtime_) {
        return true;
    }
    return runtime_->processEvent(event);
}

void RmlUILayer::update() {
    if (runtime_) {
        runtime_->update();
    }
}

void RmlUILayer::render() {
    if (!runtime_ || !render_backend_) {
        return;
    }
    if (auto* context = runtime_->getContext()) {
        render_backend_->render(*context, runtime_->getViewport());
    }
}

void RmlUILayer::setViewport(int width, int height, int offset_x, int offset_y) {
    if (!runtime_) {
        return;
    }
    runtime_->syncViewport(RmlUiViewport{
        .width = width,
        .height = height,
        .offset_x = offset_x,
        .offset_y = offset_y,
    });
}

void RmlUILayer::navigateUp() {
    if (runtime_) {
        runtime_->navigateUp();
    }
}

void RmlUILayer::navigateDown() {
    if (runtime_) {
        runtime_->navigateDown();
    }
}

void RmlUILayer::navigateLeft() {
    if (runtime_) {
        runtime_->navigateLeft();
    }
}

void RmlUILayer::navigateRight() {
    if (runtime_) {
        runtime_->navigateRight();
    }
}

void RmlUILayer::confirmFocusedElement() {
    if (runtime_) {
        runtime_->confirmFocusedElement();
    }
}

Rml::Element* RmlUILayer::getFocusedElement() const {
    if (!runtime_) {
        return nullptr;
    }
    return runtime_->getFocusedElement();
}

bool RmlUILayer::focusElement(Rml::Element* element) {
    if (!runtime_) {
        return false;
    }
    return runtime_->focusElement(element);
}

bool RmlUILayer::focusElementById(Rml::ElementDocument* document, std::string_view element_id) {
    if (!runtime_) {
        return false;
    }
    return runtime_->focusElementById(document, element_id);
}

bool RmlUILayer::focusFirstEnabledElementByClass(Rml::ElementDocument* document, std::string_view class_name) {
    if (!runtime_) {
        return false;
    }
    return runtime_->focusFirstEnabledElementByClass(document, class_name);
}

void RmlUILayer::queueFocusElement(Rml::Element* element) {
    if (runtime_) {
        runtime_->queueFocusElement(element);
    }
}

void RmlUILayer::queueFocusElementById(Rml::ElementDocument* document, std::string_view element_id) {
    if (runtime_) {
        runtime_->queueFocusElementById(document, element_id);
    }
}

void RmlUILayer::queueFocusFirstEnabledElementByClass(Rml::ElementDocument* document, std::string_view class_name) {
    if (runtime_) {
        runtime_->queueFocusFirstEnabledElementByClass(document, class_name);
    }
}

void RmlUILayer::setLogicalSize(int width, int height) {
    logical_width_ = std::max(width, 0);
    logical_height_ = std::max(height, 0);
    if (runtime_) {
        runtime_->setLogicalSize(logical_width_, logical_height_);
    }
    if (render_backend_) {
        render_backend_->setLogicalSize(logical_width_, logical_height_);
    }
}

void RmlUILayer::setTextureFilterMode(RmlUiTextureFilterMode mode) {
    if (render_backend_) {
        render_backend_->setTextureFilterMode(mode);
    }
}

RmlUiTextureFilterMode RmlUILayer::getTextureFilterMode() const {
    if (!render_backend_) {
        return RmlUiTextureFilterMode::Nearest;
    }
    return render_backend_->getTextureFilterMode();
}

Rml::ElementDocument* RmlUILayer::loadDocument(std::string_view document_path, uint64_t owner_scene_id) {
    if (!runtime_) {
        return nullptr;
    }
    return runtime_->loadDocument(document_path, owner_scene_id);
}

void RmlUILayer::unloadDocument(Rml::ElementDocument* doc) {
    if (runtime_) {
        runtime_->unloadDocument(doc);
    }
}

void RmlUILayer::unloadDocumentsByOwner(uint64_t owner_scene_id) {
    if (runtime_) {
        runtime_->unloadDocumentsByOwner(owner_scene_id);
    }
}

void RmlUILayer::showDocument(Rml::ElementDocument* doc) {
    if (runtime_) {
        runtime_->showDocument(doc);
    }
}

void RmlUILayer::hideDocument(Rml::ElementDocument* doc) {
    if (runtime_) {
        runtime_->hideDocument(doc);
    }
}

void RmlUILayer::setActiveScene(uint64_t scene_id) {
    if (runtime_) {
        runtime_->setActiveScene(scene_id);
    }
}

uint64_t RmlUILayer::getActiveSceneId() const {
    if (!runtime_) {
        return 0;
    }
    return runtime_->getActiveSceneId();
}

bool RmlUILayer::reloadLastDocument() {
    if (!runtime_) {
        return false;
    }
    return runtime_->reloadLastDocument();
}

Rml::Context* RmlUILayer::getContext() const {
    if (!runtime_) {
        return nullptr;
    }
    return runtime_->getContext();
}

size_t RmlUILayer::getDocumentCount() const {
    if (!runtime_) {
        return 0;
    }
    return runtime_->getDocumentCount();
}

} // namespace engine::ui::rmlui
