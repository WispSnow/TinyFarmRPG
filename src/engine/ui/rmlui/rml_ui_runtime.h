#pragma once

#include "engine/ui/rmlui/rml_ui_viewport.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Rml {
class Context;
class Element;
class ElementDocument;
}

class SystemInterface_SDL;

namespace engine::ui::rmlui {

class RenderInterface_GL3_STB;

/// @brief RmlUi retained-mode runtime。
///
/// 负责全局 RmlUi 初始化/关闭、主 `Rml::Context`、文档与焦点管理、SDL 事件处理，
/// 以及显式的 `Update()` 阶段。渲染细节不在这里处理。
class RmlUiRuntime final {
public:
    [[nodiscard]] static std::unique_ptr<RmlUiRuntime> create(SDL_Window* window,
                                                              RenderInterface_GL3_STB& render_interface,
                                                              const RmlUiViewport& viewport);
    ~RmlUiRuntime();

    RmlUiRuntime(const RmlUiRuntime&) = delete;
    RmlUiRuntime& operator=(const RmlUiRuntime&) = delete;
    RmlUiRuntime(RmlUiRuntime&&) = delete;
    RmlUiRuntime& operator=(RmlUiRuntime&&) = delete;

    void clean();

    [[nodiscard]] bool loadFontFace(std::string_view path) const;
    [[nodiscard]] bool processEvent(SDL_Event& event);
    void update();
    void syncViewport(const RmlUiViewport& viewport);
    void setLogicalSize(int width, int height);

    void navigateUp();
    void navigateDown();
    void navigateLeft();
    void navigateRight();
    void confirmFocusedElement();
    [[nodiscard]] Rml::Element* getFocusedElement() const;
    [[nodiscard]] bool focusElement(Rml::Element* element);
    [[nodiscard]] bool focusElementById(Rml::ElementDocument* document, std::string_view element_id);
    [[nodiscard]] bool focusFirstEnabledElementByClass(Rml::ElementDocument* document, std::string_view class_name);

    /// @brief 延迟到下一次 @ref update 末尾再设置焦点。
    ///
    /// 当目标元素尚未完成布局时（如文档刚加载、尚未经过首次 `Context::Update()`），
    /// 直接调用 `focusElement` 会静默失败。此系列函数将请求加入队列，
    /// 在 `update()` 调用 `Context::Update()` 完成布局后再统一执行，确保成功。
    /// @see update
    void queueFocusElement(Rml::Element* element);
    /// @brief 按元素 ID 延迟设置焦点。@see queueFocusElement
    void queueFocusElementById(Rml::ElementDocument* document, std::string_view element_id);
    /// @brief 按 CSS 类名查找第一个未禁用元素并延迟设置焦点。@see queueFocusElement
    void queueFocusFirstEnabledElementByClass(Rml::ElementDocument* document, std::string_view class_name);

    [[nodiscard]] Rml::ElementDocument* loadDocument(std::string_view document_path,
                                                     uint64_t owner_scene_id = 0);
    void unloadDocument(Rml::ElementDocument* doc);
    void unloadDocumentsByOwner(uint64_t owner_scene_id);
    void showDocument(Rml::ElementDocument* doc);
    void hideDocument(Rml::ElementDocument* doc);

    void setActiveScene(uint64_t scene_id);
    [[nodiscard]] uint64_t getActiveSceneId() const { return active_scene_id_; }

    [[nodiscard]] bool reloadLastDocument();

    [[nodiscard]] Rml::Context* getContext() const { return context_; }
    [[nodiscard]] const RmlUiViewport& getViewport() const { return viewport_; }
    [[nodiscard]] size_t getDocumentCount() const { return documents_.size(); }

    template<typename Fn>
    void forEachDocument(Fn&& fn) const {
        for (const auto& entry : documents_) {
            fn(entry.path, entry.owner);
        }
    }

private:
    RmlUiRuntime() = default;
    [[nodiscard]] bool init(SDL_Window* window,
                            RenderInterface_GL3_STB& render_interface,
                            const RmlUiViewport& viewport);

    void applyContextDimensions();
    void adjustEventForViewport(SDL_Event& event) const;
    void applyInteractionPolicy();

    struct DocumentEntry {
        Rml::ElementDocument* doc{nullptr};
        uint64_t owner{0};
        std::string path;
    };

    struct PendingFocusRequest {
        enum class Kind : uint8_t {
            Element,
            ElementId,
            FirstEnabledElementByClass
        };

        Kind kind{Kind::Element};
        Rml::ElementDocument* document{nullptr};
        Rml::Element* element{nullptr};
        std::string token;
    };

    void clearPendingFocusRequestsForDocument(Rml::ElementDocument* document);

    SDL_Window* window_{nullptr};
    std::unique_ptr<SystemInterface_SDL> system_interface_;
    RenderInterface_GL3_STB* render_interface_{nullptr};
    Rml::Context* context_{nullptr};
    RmlUiViewport viewport_{};
    int logical_width_{0};
    int logical_height_{0};
    bool initialized_{false};

    std::vector<DocumentEntry> documents_;
    uint64_t active_scene_id_{0};
    std::vector<PendingFocusRequest> pending_focus_requests_;
};

} // namespace engine::ui::rmlui
