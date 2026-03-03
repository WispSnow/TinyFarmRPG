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

    // dp_ratio 初始值；后续 setViewport 会在 logical_size 设置后重新计算
    if (logical_width_ > 0 && logical_height_ > 0) {
        context_->SetDimensions(Rml::Vector2i{logical_width_, logical_height_});
        context_->SetDensityIndependentPixelRatio(1.0f);
        render_interface_->SetViewport(logical_width_, logical_height_, 0, 0);
    } else {
        const float display_scale = SDL_GetWindowDisplayScale(window_);
        if (display_scale > 0.0f) {
            context_->SetDensityIndependentPixelRatio(display_scale);
        }
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

    if (logical_width_ > 0 && logical_height_ > 0) {
        // GL3 renderer 的 EndFrame 先将 FBO resolve 到 postprocess buffer，
        // 再 blit 到 backbuffer。此时需将 glViewport 设为物理 viewport，
        // 让逻辑尺寸的 FBO 拉伸填满物理显示区域。
        // EndFrame 内部的 glViewport 使用 SetViewport 设置的值（逻辑尺寸），
        // 所以需要在 EndFrame 之前临时切回物理 viewport。
        render_interface_->SetViewport(viewport_width_, viewport_height_,
                                       viewport_offset_x_, viewport_offset_y_);
        render_interface_->EndFrame();
        // 还原为逻辑尺寸，以便下一帧 BeginFrame 使用正确的投影
        render_interface_->SetViewport(logical_width_, logical_height_, 0, 0);
    } else {
        render_interface_->EndFrame();
    }
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
        if (logical_width_ > 0 && logical_height_ > 0) {
            // 锁定逻辑分辨率：Context 和渲染投影都使用逻辑尺寸，
            // dp_ratio 保持 1.0，让 1dp = 1逻辑像素。
            // GL viewport（在 render() 中设置）将逻辑空间拉伸到物理 viewport。
            context_->SetDimensions(Rml::Vector2i{logical_width_, logical_height_});
            context_->SetDensityIndependentPixelRatio(1.0f);
            render_interface_->SetViewport(logical_width_, logical_height_, 0, 0);
        } else {
            // 未设置逻辑分辨率时，回退到物理像素 + 显示缩放
            context_->SetDimensions(Rml::Vector2i{viewport_width_, viewport_height_});
            if (window_) {
                const float display_scale = SDL_GetWindowDisplayScale(window_);
                if (display_scale > 0.0f) {
                    context_->SetDensityIndependentPixelRatio(display_scale);
                }
            }
        }
    }
}

void RmlUILayer::setLogicalSize(int width, int height) {
    logical_width_ = std::max(width, 0);
    logical_height_ = std::max(height, 0);
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
            // 移除内联覆盖，让 RCSS 声明的 pointer-events 生效
            entry.doc->RemoveProperty("pointer-events");
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

    // SDL 鼠标坐标是 window coordinates。
    // 需要：1) 减去 letterbox 偏移  2) 如果锁定了逻辑分辨率，缩放到逻辑空间。
    //
    // viewport_offset_ 是物理像素，SDL 鼠标坐标是窗口坐标，
    // 两者的比值就是 pixel density。
    const float pixel_density = std::max(SDL_GetWindowPixelDensity(window_), 1.0f);
    const float offset_x = static_cast<float>(viewport_offset_x_) / pixel_density;
    const float offset_y = static_cast<float>(viewport_offset_y_) / pixel_density;

    // 窗口坐标中的视口尺寸
    const float vp_w_win = static_cast<float>(viewport_width_) / pixel_density;
    const float vp_h_win = static_cast<float>(viewport_height_) / pixel_density;

    // 从窗口坐标映射到逻辑坐标的缩放因子
    const bool use_logical = (logical_width_ > 0 && logical_height_ > 0 && vp_w_win > 0.0f && vp_h_win > 0.0f);
    const float scale_x = use_logical ? (static_cast<float>(logical_width_) / vp_w_win) : 1.0f;
    const float scale_y = use_logical ? (static_cast<float>(logical_height_) / vp_h_win) : 1.0f;

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
                event.window.data1 = viewport_width_;
                event.window.data2 = viewport_height_;
            }
            break;
        default:
            break;
    }
}

} // namespace engine::ui::rmlui
