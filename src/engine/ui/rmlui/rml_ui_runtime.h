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
    void queueFocusElement(Rml::Element* element);
    void queueFocusElementById(Rml::ElementDocument* document, std::string_view element_id);
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
