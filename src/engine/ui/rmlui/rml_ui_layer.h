#pragma once

#include <SDL3/SDL.h>

#include <memory>
#include <string>
#include <string_view>

namespace Rml {
class Context;
class ElementDocument;
}

class SystemInterface_SDL;

namespace engine::ui::rmlui {

class RenderInterface_GL3_STB;

class RmlUILayer final {
public:
    [[nodiscard]] static std::unique_ptr<RmlUILayer> create(SDL_Window* window,
                                                             int viewport_width,
                                                             int viewport_height,
                                                             int viewport_offset_x = 0,
                                                             int viewport_offset_y = 0);
    ~RmlUILayer();

    RmlUILayer(const RmlUILayer&) = delete;
    RmlUILayer& operator=(const RmlUILayer&) = delete;
    RmlUILayer(RmlUILayer&&) = delete;
    RmlUILayer& operator=(RmlUILayer&&) = delete;

    void clean();

    [[nodiscard]] bool processEvent(SDL_Event& event);
    void update();
    void render();
    void setViewport(int width, int height, int offset_x = 0, int offset_y = 0);
    [[nodiscard]] bool loadDocument(std::string_view document_path);
    [[nodiscard]] bool reloadDocument();
    [[nodiscard]] std::string_view getCurrentDocumentPath() const { return current_document_path_; }

    [[nodiscard]] Rml::Context* getContext() const { return context_; }

private:
    RmlUILayer() = default;

    [[nodiscard]] bool init(SDL_Window* window,
                            int viewport_width,
                            int viewport_height,
                            int viewport_offset_x,
                            int viewport_offset_y);

    void adjustEventForViewport(SDL_Event& event) const;

    SDL_Window* window_{nullptr};
    std::unique_ptr<RenderInterface_GL3_STB> render_interface_;
    std::unique_ptr<SystemInterface_SDL> system_interface_;
    Rml::Context* context_{nullptr};
    Rml::ElementDocument* current_document_{nullptr};
    std::string current_document_path_{};

    int viewport_width_{1};
    int viewport_height_{1};
    int viewport_offset_x_{0};
    int viewport_offset_y_{0};

    bool initialized_{false};
};

} // namespace engine::ui::rmlui
