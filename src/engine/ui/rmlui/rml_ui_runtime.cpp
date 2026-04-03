#include "engine/ui/rmlui/rml_ui_runtime.h"

#include "engine/ui/rmlui/render_interface_gl3_stb.h"

#include "RmlUi_Platform_SDL.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Log.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>

namespace engine::ui::rmlui {

namespace {

[[nodiscard]] RmlUiViewport sanitizeViewport(RmlUiViewport viewport) {
    viewport.width = std::max(viewport.width, 1);
    viewport.height = std::max(viewport.height, 1);
    return viewport;
}

} // namespace

std::unique_ptr<RmlUiRuntime> RmlUiRuntime::create(SDL_Window* window,
                                                   RenderInterface_GL3_STB& render_interface,
                                                   const RmlUiViewport& viewport) {
    auto runtime = std::unique_ptr<RmlUiRuntime>(new RmlUiRuntime());
    if (!runtime->init(window, render_interface, viewport)) {
        return nullptr;
    }
    return runtime;
}

RmlUiRuntime::~RmlUiRuntime() {
    clean();
}

bool RmlUiRuntime::init(SDL_Window* window,
                        RenderInterface_GL3_STB& render_interface,
                        const RmlUiViewport& viewport) {
    clean();

    if (!window) {
        spdlog::error("RmlUiRuntime::init failed: window is null.");
        return false;
    }

    window_ = window;
    render_interface_ = &render_interface;
    viewport_ = sanitizeViewport(viewport);
    system_interface_ = std::make_unique<SystemInterface_SDL>();

    if (!system_interface_) {
        spdlog::error("RmlUiRuntime::init failed: system interface creation failed.");
        clean();
        return false;
    }

    system_interface_->SetWindow(window_);

    Rml::SetSystemInterface(system_interface_.get());
    Rml::SetRenderInterface(render_interface_);
    if (!Rml::Initialise()) {
        spdlog::error("RmlUiRuntime::init failed: Rml::Initialise failed.");
        clean();
        return false;
    }
    initialized_ = true;

    context_ = Rml::CreateContext("main", Rml::Vector2i{viewport_.width, viewport_.height});
    if (!context_) {
        spdlog::error("RmlUiRuntime::init failed: Rml::CreateContext failed.");
        clean();
        return false;
    }

    applyContextDimensions();
    spdlog::trace("RmlUiRuntime initialized.");
    return true;
}

void RmlUiRuntime::clean() {
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

    system_interface_.reset();
    render_interface_ = nullptr;
    viewport_ = {};
    logical_width_ = 0;
    logical_height_ = 0;
    window_ = nullptr;
}

bool RmlUiRuntime::loadFontFace(std::string_view path) const {
    if (path.empty()) {
        return false;
    }
    const Rml::String font_path{path.data(), path.size()};
    return Rml::LoadFontFace(font_path);
}

bool RmlUiRuntime::processEvent(SDL_Event& event) {
    if (!context_ || !window_) {
        return false;
    }

    SDL_Event adjusted_event = event;
    adjustEventForViewport(adjusted_event);
    return RmlSDL::InputEventHandler(context_, window_, adjusted_event);
}

void RmlUiRuntime::update() {
    if (!context_) {
        return;
    }

    context_->Update();
}

void RmlUiRuntime::syncViewport(const RmlUiViewport& viewport) {
    viewport_ = sanitizeViewport(viewport);
    applyContextDimensions();
}

void RmlUiRuntime::setLogicalSize(int width, int height) {
    logical_width_ = std::max(width, 0);
    logical_height_ = std::max(height, 0);
    applyContextDimensions();
}

Rml::ElementDocument* RmlUiRuntime::loadDocument(std::string_view document_path, uint64_t owner_scene_id) {
    if (!context_) {
        return nullptr;
    }
    if (document_path.empty()) {
        spdlog::warn("RmlUiRuntime::loadDocument ignored empty path.");
        return nullptr;
    }

    const Rml::String path_string{document_path.data(), document_path.size()};
    auto* doc = context_->LoadDocument(path_string);
    if (!doc) {
        spdlog::error("RmlUiRuntime::loadDocument failed: {}", path_string);
        return nullptr;
    }

    doc->Show();
    documents_.push_back({doc, owner_scene_id, std::string(document_path)});

    if (active_scene_id_ != 0) {
        applyInteractionPolicy();
    }
    return doc;
}

void RmlUiRuntime::unloadDocument(Rml::ElementDocument* doc) {
    if (!doc) {
        return;
    }

    auto it = std::find_if(documents_.begin(), documents_.end(), [doc](const DocumentEntry& entry) {
        return entry.doc == doc;
    });
    if (it != documents_.end()) {
        documents_.erase(it);
    }

    doc->Close();
}

void RmlUiRuntime::unloadDocumentsByOwner(uint64_t owner_scene_id) {
    std::vector<Rml::ElementDocument*> documents_to_close;
    for (const auto& entry : documents_) {
        if (entry.owner == owner_scene_id) {
            documents_to_close.push_back(entry.doc);
        }
    }

    std::erase_if(documents_, [owner_scene_id](const DocumentEntry& entry) {
        return entry.owner == owner_scene_id;
    });

    for (auto* doc : documents_to_close) {
        if (doc) {
            doc->Close();
        }
    }
}

void RmlUiRuntime::showDocument(Rml::ElementDocument* doc) {
    if (doc) {
        doc->Show();
    }
}

void RmlUiRuntime::hideDocument(Rml::ElementDocument* doc) {
    if (doc) {
        doc->Hide();
    }
}

void RmlUiRuntime::setActiveScene(uint64_t scene_id) {
    if (active_scene_id_ == scene_id) {
        return;
    }
    active_scene_id_ = scene_id;
    applyInteractionPolicy();
}

bool RmlUiRuntime::reloadLastDocument() {
    if (documents_.empty()) {
        spdlog::warn("RmlUiRuntime::reloadLastDocument: no documents loaded.");
        return false;
    }

    auto& last = documents_.back();
    const std::string path = last.path;
    const uint64_t owner = last.owner;

    unloadDocument(last.doc);
    return loadDocument(path, owner) != nullptr;
}

void RmlUiRuntime::applyContextDimensions() {
    if (!context_) {
        return;
    }

    if (logical_width_ > 0 && logical_height_ > 0) {
        context_->SetDimensions(Rml::Vector2i{logical_width_, logical_height_});
        context_->SetDensityIndependentPixelRatio(1.0f);
        return;
    }

    context_->SetDimensions(Rml::Vector2i{viewport_.width, viewport_.height});
    if (!window_) {
        return;
    }

    const float display_scale = SDL_GetWindowDisplayScale(window_);
    if (display_scale > 0.0f) {
        context_->SetDensityIndependentPixelRatio(display_scale);
    }
}

void RmlUiRuntime::adjustEventForViewport(SDL_Event& event) const {
    if (!window_) {
        return;
    }

    const float pixel_density = std::max(SDL_GetWindowPixelDensity(window_), 1.0f);
    const float offset_x = static_cast<float>(viewport_.offset_x) / pixel_density;
    const float offset_y = static_cast<float>(viewport_.offset_y) / pixel_density;
    const float viewport_width_window = static_cast<float>(viewport_.width) / pixel_density;
    const float viewport_height_window = static_cast<float>(viewport_.height) / pixel_density;

    const bool use_logical =
        logical_width_ > 0 && logical_height_ > 0 && viewport_width_window > 0.0f && viewport_height_window > 0.0f;
    const float scale_x = use_logical ? (static_cast<float>(logical_width_) / viewport_width_window) : 1.0f;
    const float scale_y = use_logical ? (static_cast<float>(logical_height_) / viewport_height_window) : 1.0f;

    switch (event.type) {
    case SDL_EVENT_MOUSE_MOTION:
        event.motion.x = (event.motion.x - offset_x) * scale_x;
        event.motion.y = (event.motion.y - offset_y) * scale_y;
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        event.button.x = (event.button.x - offset_x) * scale_x;
        event.button.y = (event.button.y - offset_y) * scale_y;
        break;
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        if (use_logical) {
            event.window.data1 = logical_width_;
            event.window.data2 = logical_height_;
        } else {
            event.window.data1 = viewport_.width;
            event.window.data2 = viewport_.height;
        }
        break;
    default:
        break;
    }
}

// 根据当前活跃 Scene 更新所有文档的交互权限。
// owner == 0 的文档为全局 UI（如 HUD），始终保持可交互；
// owner 与 active_scene_id_ 匹配的文档属于当前 Scene，恢复正常交互；
// 其余文档（后台 Scene 的 UI）设置 pointer-events:none，
// 防止非活跃 Scene 的界面响应鼠标点击。
void RmlUiRuntime::applyInteractionPolicy() {
    for (auto& entry : documents_) {
        if (!entry.doc) {
            continue;
        }

        const bool interactive = (entry.owner == 0) || (entry.owner == active_scene_id_);
        if (interactive) {
            // 移除之前写入的内联样式覆盖，而非设为 "auto"。
            // pointer-events 是 RmlUi 内置属性，默认值即为 auto（可交互）。
            // 用 RemoveProperty 可恢复 RCSS 层叠规则，避免覆盖文档自身的样式设定。
            entry.doc->RemoveProperty("pointer-events");
        } else {
            entry.doc->SetProperty("pointer-events", "none");
        }
    }
}

} // namespace engine::ui::rmlui
