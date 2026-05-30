#pragma once

#include "engine/ui/rmlui/rml_generated_image_registry.h"
#include "engine/ui/rmlui/rml_ui_viewport.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Rml {
class Context;
class ElementDocument;
}

namespace engine::input {
class MouseCursorService;
}

namespace engine::ui::rmlui {

class RenderInterface_GL3_STB;
class RmlSystemInterfaceSdl;

/// @brief RmlUi retained-mode runtime。
///
/// 负责全局 RmlUi 初始化/关闭、主 `Rml::Context`、文档管理、SDL 事件处理，
/// 以及显式的 `Update()` 阶段。渲染细节不在这里处理。
class RmlUiRuntime final {
public:
    using DocumentLoadedCallback =
        std::function<void(Rml::ElementDocument&, uint64_t owner_scene_id, const std::string& path)>;

    enum class InputMode : std::uint8_t {
        Mouse,
        Navigation,
    };

    [[nodiscard]] static std::unique_ptr<RmlUiRuntime> create(SDL_Window* window,
                                                              RenderInterface_GL3_STB& render_interface,
                                                              const RmlUiViewport& viewport,
                                                              engine::input::MouseCursorService* cursor_service = nullptr);
    ~RmlUiRuntime();

    RmlUiRuntime(const RmlUiRuntime&) = delete;
    RmlUiRuntime& operator=(const RmlUiRuntime&) = delete;
    RmlUiRuntime(RmlUiRuntime&&) = delete;
    RmlUiRuntime& operator=(RmlUiRuntime&&) = delete;

    void clean();

    [[nodiscard]] bool loadFontFace(std::string_view path, bool fallback_face = false) const;
    [[nodiscard]] bool processEvent(SDL_Event& event);
    void update();
    void syncViewport(const RmlUiViewport& viewport);
    void setLogicalSize(int width, int height);
    void setInputMode(InputMode mode);
    [[nodiscard]] InputMode getInputMode() const { return input_mode_; }

    [[nodiscard]] Rml::ElementDocument* loadDocument(std::string_view document_path,
                                                     uint64_t owner_scene_id = 0);
    /// @brief 重新加载已由 runtime 托管的文档；加载失败时保留旧文档。
    [[nodiscard]] Rml::ElementDocument* reloadDocument(Rml::ElementDocument* doc);
    void unloadDocument(Rml::ElementDocument* doc);

    /// @brief 把指定 class 互斥地写到所有已加载文档的 <body>（移除其它两个 tf-font-* class）。
    ///        同时记录为"默认字号 class"，新加载的文档会自动应用该 class，无需对单一服务句柄
    ///        保留任何引用（避免外部 service 销毁后产生悬空 lambda）。
    void applyBodyFontScaleClassToAllDocuments(std::string_view next_class);
    /// @brief 设置文档加载回调。owner_scene_id 是 Scene 实例 id，path 是 RML 文档路径。
    void setDocumentLoadedCallback(DocumentLoadedCallback callback);
    void unloadDocumentsByOwner(uint64_t owner_scene_id);
    void showDocument(Rml::ElementDocument* doc);
    void hideDocument(Rml::ElementDocument* doc);

    void setActiveScene(uint64_t scene_id);
    void setVisibleSceneOwners(std::vector<uint64_t> scene_owner_ids);
    [[nodiscard]] uint64_t getActiveSceneId() const { return active_scene_id_; }

    [[nodiscard]] bool reloadLastDocument();

    // Debugger
    void setDebuggerEnabled(bool enabled);
    [[nodiscard]] bool isDebuggerEnabled() const { return debugger_enabled_; }
    void toggleDebuggerVisible();
    void setDebuggerVisible(bool visible);
    [[nodiscard]] bool isDebuggerVisible() const;

    [[nodiscard]] Rml::Context* getContext() const { return context_; }
    [[nodiscard]] const RmlUiViewport& getViewport() const { return viewport_; }
    [[nodiscard]] size_t getDocumentCount() const { return documents_.size(); }
    [[nodiscard]] RmlGeneratedImageRegistry& generatedImages() { return generated_images_; }
    [[nodiscard]] const RmlGeneratedImageRegistry& generatedImages() const { return generated_images_; }

    template<typename Fn>
    void forEachDocument(Fn&& fn) const {
        for (const auto& entry : documents_) {
            if (entry.doc) {
                fn(*entry.doc, entry.owner, entry.path);
            }
        }
    }

private:
    RmlUiRuntime() = default;
    [[nodiscard]] bool init(SDL_Window* window,
                            RenderInterface_GL3_STB& render_interface,
                            const RmlUiViewport& viewport,
                            engine::input::MouseCursorService* cursor_service);

    struct DocumentEntry {
        Rml::ElementDocument* doc{nullptr};
        uint64_t owner{0};
        std::string path;
        bool requested_visible{true};
        bool currently_visible{false};
    };

    void applyContextDimensions();
    void adjustEventForViewport(SDL_Event& event) const;
    void applyInteractionPolicy();
    void applyVisibilityPolicy();
    void applyDocumentVisibility(DocumentEntry& entry);
    [[nodiscard]] bool isOwnerVisible(uint64_t owner_scene_id) const;
    void applyInputModeClass(Rml::ElementDocument* doc);
    void applyInputModeClasses();
    void applyFontScaleClassToBody(Rml::ElementDocument* doc, std::string_view next_class);

    [[nodiscard]] bool ensureDebuggerInitialized();

    SDL_Window* window_{nullptr};
    std::unique_ptr<RmlSystemInterfaceSdl> system_interface_;
    RenderInterface_GL3_STB* render_interface_{nullptr};
    Rml::Context* context_{nullptr};
    RmlUiViewport viewport_{};
    RmlGeneratedImageRegistry generated_images_{};
    int logical_width_{0};
    int logical_height_{0};
    bool initialized_{false};
    bool debugger_enabled_{false};
    bool debugger_initialized_{false};

    std::vector<DocumentEntry> documents_;
    DocumentLoadedCallback document_loaded_callback_{};
    std::vector<uint64_t> visible_scene_owners_;
    uint64_t active_scene_id_{0};
    InputMode input_mode_{InputMode::Mouse};
    std::string body_font_scale_class_{"tf-font-normal"};
};

} // namespace engine::ui::rmlui
