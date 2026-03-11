#pragma once

#include "engine/scene/scene.h"

#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Input.h>

#include <string>
#include <vector>

namespace Rml {
class Element;
class ElementDocument;
} // namespace Rml

namespace learn::jrpg {

class JrpgWindowScene final : public engine::scene::Scene {
public:
    using Scene::Scene;

    [[nodiscard]] bool init() override;
    void update(float dt) override;
    void clean() override;

private:
    struct MenuItemInfo {
        std::string title;
        std::string description;
    };

    void onMenuFocus(int index);
    void onMenuSelect(int index);
    void openPopup();
    void closePopup(bool confirmed);

    // Helper to register listeners with automatic cleanup tracking
    void addListener(Rml::Element* el, const Rml::String& event, bool capture = false);

    // --- Event listener ---
    class UIEventListener final : public Rml::EventListener {
    public:
        explicit UIEventListener(JrpgWindowScene& scene) : scene_(scene) {}
        void ProcessEvent(Rml::Event& event) override;

    private:
        JrpgWindowScene& scene_;
    };

    Rml::ElementDocument* doc_ = nullptr;

    // Cached element references
    Rml::Element* info_title_el_  = nullptr;
    Rml::Element* info_desc_el_   = nullptr;
    Rml::Element* status_text_el_ = nullptr;
    Rml::Element* modal_overlay_  = nullptr;
    Rml::Element* popup_window_   = nullptr;

    std::unique_ptr<UIEventListener> listener_;

    // Listener registration tracking for cleanup
    struct ListenerReg {
        Rml::Element* el;
        Rml::String   event;
        bool          capture;
    };
    std::vector<ListenerReg> registrations_;

    // State
    std::vector<MenuItemInfo> menu_info_;
    int   focused_index_     = -1;
    bool  popup_open_        = false;
    float popup_close_timer_ = -1.0f;

    static constexpr int   kMenuItemCount      = 6;
    static constexpr float kPopupCloseDuration  = 0.18f;
};

} // namespace learn::jrpg
