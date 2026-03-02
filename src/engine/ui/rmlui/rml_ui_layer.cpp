#include "engine/ui/rmlui/rml_ui_layer.h"

#include "engine/ui/rmlui/render_interface_gl3_stb.h"

#include "RmlUi_Platform_SDL.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Log.h>

#include <SDL3/SDL.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace engine::ui::rmlui {

namespace {

constexpr char kDefaultFontPath[] = "assets/fonts/VonwaonBitmap-16px.ttf";

} // namespace

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
    system_interface_ = std::make_unique<SystemInterface_SDL>();
    render_interface_ = std::make_unique<RenderInterface_GL3_STB>();

    if (!system_interface_ || !render_interface_ || !static_cast<bool>(*render_interface_)) {
        spdlog::error("RmlUILayer::init failed: interface creation failed.");
        clean();
        return false;
    }

    system_interface_->SetWindow(window_);

    Rml::SetSystemInterface(system_interface_.get());
    Rml::SetRenderInterface(render_interface_.get());
    if (!Rml::Initialise()) {
        spdlog::error("RmlUILayer::init failed: Rml::Initialise failed.");
        clean();
        return false;
    }
    initialized_ = true;

    setViewport(viewport_width, viewport_height, viewport_offset_x, viewport_offset_y);

    context_ = Rml::CreateContext("main", Rml::Vector2i{viewport_width_, viewport_height_});
    if (!context_) {
        spdlog::error("RmlUILayer::init failed: Rml::CreateContext failed.");
        clean();
        return false;
    }

    const float display_scale = SDL_GetWindowDisplayScale(window_);
    if (display_scale > 0.0f) {
        context_->SetDensityIndependentPixelRatio(display_scale);
    }

    if (!Rml::LoadFontFace(kDefaultFontPath)) {
        spdlog::warn("RmlUILayer: failed to load default font {}.", kDefaultFontPath);
    }

    spdlog::trace("RmlUILayer initialized.");
    return true;
}

void RmlUILayer::clean() {
    // 关闭所有已跟踪的文档
    for (auto& entry : documents_) {
        if (entry.doc) {
            entry.doc->Close();
        }
    }
    documents_.clear();
    active_scene_id_ = 0;

    if (context_) {
        const Rml::String context_name = context_->GetName();
        (void)Rml::RemoveContext(context_name);
        context_ = nullptr;
    }

    if (initialized_) {
        Rml::Shutdown();
        initialized_ = false;
    }

    Rml::SetRenderInterface(nullptr);
    Rml::SetSystemInterface(nullptr);

    render_interface_.reset();
    system_interface_.reset();
    window_ = nullptr;
}

bool RmlUILayer::processEvent(SDL_Event& event) {
    if (!context_ || !window_) {
        return true;
    }

    SDL_Event adjusted_event = event;
    adjustEventForViewport(adjusted_event);
    return RmlSDL::InputEventHandler(context_, window_, adjusted_event);
}

void RmlUILayer::update() {
    if (context_) {
        context_->Update();
    }
}

void RmlUILayer::render() {
    if (!context_ || !render_interface_) {
        return;
    }

    render_interface_->BeginFrame();
    context_->Render();
    render_interface_->EndFrame();
}

void RmlUILayer::setViewport(int width, int height, int offset_x, int offset_y) {
    viewport_width_ = std::max(width, 1);
    viewport_height_ = std::max(height, 1);
    viewport_offset_x_ = offset_x;
    viewport_offset_y_ = offset_y;

    if (render_interface_) {
        render_interface_->SetViewport(viewport_width_, viewport_height_, viewport_offset_x_, viewport_offset_y_);
    }

    if (context_) {
        context_->SetDimensions(Rml::Vector2i{viewport_width_, viewport_height_});
        if (window_) {
            const float display_scale = SDL_GetWindowDisplayScale(window_);
            if (display_scale > 0.0f) {
                context_->SetDensityIndependentPixelRatio(display_scale);
            }
        }
    }
}

// --- 多文档管理 ---

Rml::ElementDocument* RmlUILayer::loadDocument(std::string_view document_path,
                                                uint64_t owner_scene_id) {
    if (!context_) {
        return nullptr;
    }
    if (document_path.empty()) {
        spdlog::warn("RmlUILayer::loadDocument ignored empty path.");
        return nullptr;
    }

    const Rml::String path_string{document_path.data(), document_path.size()};
    auto* doc = context_->LoadDocument(path_string);
    if (!doc) {
        spdlog::error("RmlUILayer::loadDocument failed: {}", path_string);
        return nullptr;
    }

    doc->Show();
    documents_.push_back({doc, owner_scene_id, std::string(document_path)});
    spdlog::info("RmlUILayer loaded document: {} (owner: {})",
                 document_path, owner_scene_id == 0 ? "global" : std::to_string(owner_scene_id));

    // 如果当前有活跃场景，对新文档应用交互策略
    if (active_scene_id_ != 0) {
        applyInteractionPolicy();
    }

    return doc;
}

void RmlUILayer::unloadDocument(Rml::ElementDocument* doc) {
    if (!doc) {
        return;
    }

    auto it = std::find_if(documents_.begin(), documents_.end(),
                           [doc](const DocumentEntry& e) { return e.doc == doc; });
    if (it != documents_.end()) {
        spdlog::info("RmlUILayer unloading document: {}", it->path);
        documents_.erase(it);
    }

    doc->Close();
}

void RmlUILayer::unloadDocumentsByOwner(uint64_t owner_scene_id) {
    // 收集需要关闭的文档（避免在迭代中修改容器）
    std::vector<Rml::ElementDocument*> to_close;
    for (const auto& entry : documents_) {
        if (entry.owner == owner_scene_id) {
            to_close.push_back(entry.doc);
        }
    }

    // 从跟踪列表中移除
    std::erase_if(documents_, [owner_scene_id](const DocumentEntry& e) {
        return e.owner == owner_scene_id;
    });

    // 关闭文档
    for (auto* doc : to_close) {
        if (doc) {
            spdlog::info("RmlUILayer unloading document owned by scene {}.", owner_scene_id);
            doc->Close();
        }
    }
}

void RmlUILayer::showDocument(Rml::ElementDocument* doc) {
    if (doc) {
        doc->Show();
    }
}

void RmlUILayer::hideDocument(Rml::ElementDocument* doc) {
    if (doc) {
        doc->Hide();
    }
}

// --- 场景归属 ---

void RmlUILayer::setActiveScene(uint64_t scene_id) {
    if (active_scene_id_ == scene_id) {
        return;
    }
    active_scene_id_ = scene_id;
    applyInteractionPolicy();
    spdlog::debug("RmlUILayer active scene set to {}.", active_scene_id_);
}

void RmlUILayer::applyInteractionPolicy() {
    for (auto& entry : documents_) {
        if (!entry.doc) {
            continue;
        }

        // 全局文档（owner == 0）或活跃场景的文档可交互
        const bool interactive = (entry.owner == 0) || (entry.owner == active_scene_id_);

        if (interactive) {
            entry.doc->SetProperty("pointer-events", "auto");
        } else {
            entry.doc->SetProperty("pointer-events", "none");
            // 移除非活跃文档上的键盘焦点，防止按键事件泄漏
            entry.doc->Blur();
        }
    }
}

// --- 向后兼容 ---

bool RmlUILayer::reloadLastDocument() {
    if (documents_.empty()) {
        spdlog::warn("RmlUILayer::reloadLastDocument: no documents loaded.");
        return false;
    }

    auto& last = documents_.back();
    const std::string path = last.path;
    const uint64_t owner = last.owner;

    unloadDocument(last.doc);
    return loadDocument(path, owner) != nullptr;
}

void RmlUILayer::adjustEventForViewport(SDL_Event& event) const {
    if (!window_) {
        return;
    }

    const float pixel_density = std::max(SDL_GetWindowPixelDensity(window_), 1.0f);
    const float offset_x = static_cast<float>(viewport_offset_x_) / pixel_density;
    const float offset_y = static_cast<float>(viewport_offset_y_) / pixel_density;

    switch (event.type) {
        case SDL_EVENT_MOUSE_MOTION:
            event.motion.x -= offset_x;
            event.motion.y -= offset_y;
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            event.button.x -= offset_x;
            event.button.y -= offset_y;
            break;
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            event.window.data1 = viewport_width_;
            event.window.data2 = viewport_height_;
            break;
        default:
            break;
    }
}

} // namespace engine::ui::rmlui
