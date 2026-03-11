#include "jrpg_window_scene.h"

#include "engine/core/context.h"
#include "engine/render/opengl/gl_renderer.h"

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <spdlog/spdlog.h>

namespace learn::jrpg {

// ── UIEventListener ────────────────────────────────────────

void JrpgWindowScene::UIEventListener::ProcessEvent(Rml::Event& event) {
    const auto& type   = event.GetType();
    auto*       target = event.GetTargetElement();
    if (!target) return;

    const auto& id = target->GetId();

    if (type == "focus") {
        // Menu item focus — extract index from "mi-N"
        if (id.size() >= 4 && id[0] == 'm' && id[1] == 'i' && id[2] == '-') {
            int idx = id[3] - '0';
            scene_.onMenuFocus(idx);
        }
    }
    else if (type == "click") {
        if (id.size() >= 4 && id[0] == 'm' && id[1] == 'i' && id[2] == '-') {
            int idx = id[3] - '0';
            scene_.onMenuSelect(idx);
        }
        else if (id == "popup-yes") {
            scene_.closePopup(true);
        }
        else if (id == "popup-no") {
            scene_.closePopup(false);
        }
    }
    else if (type == "keydown") {
        auto key = event.GetParameter<int>("key_identifier", 0);
        if (key == static_cast<int>(Rml::Input::KI_ESCAPE) && scene_.popup_open_) {
            scene_.closePopup(false);
            event.StopPropagation();
        }
    }
}

// ── Scene lifecycle ────────────────────────────────────────

bool JrpgWindowScene::init() {
    if (!Scene::init()) return false;

    context_.getGLRenderer().setDebugUIEnabled(true);

    doc_ = loadRmlDocument("ui/rmlui/learn/learn_jrpg_window.rml");
    if (!doc_) {
        spdlog::error("JrpgWindowScene: failed to load RML");
        return false;
    }

    // Cache element refs
    info_title_el_  = doc_->GetElementById("info-title");
    info_desc_el_   = doc_->GetElementById("info-desc");
    status_text_el_ = doc_->GetElementById("status-text");
    modal_overlay_  = doc_->GetElementById("modal-overlay");
    popup_window_   = doc_->GetElementById("popup-window");

    // Menu item descriptions
    menu_info_ = {
        {"Items",     "View and manage your inventory. Use consumable items to restore HP, MP, or cure status effects."},
        {"Equipment", "Equip weapons, armor, and accessories to your party members. Compare stats before changing gear."},
        {"Skills",    "Browse learned abilities for each character. Skills consume MP to cast in battle."},
        {"Status",    "View detailed stats, experience, and active status effects for all party members."},
        {"Save",      "Record your current progress to a save slot. You can keep up to 3 save files."},
        {"Exit",      "Return to the title screen. Any unsaved progress will be lost."},
    };

    // Create event listener
    listener_ = std::make_unique<UIEventListener>(*this);

    // Register focus + click on each menu item
    for (int i = 0; i < kMenuItemCount; ++i) {
        auto* item = doc_->GetElementById("mi-" + std::to_string(i));
        if (item) {
            addListener(item, "focus");
            addListener(item, "click");
        }
    }

    // Register click on popup buttons
    addListener(doc_->GetElementById("popup-yes"), "click");
    addListener(doc_->GetElementById("popup-no"),  "click");

    // Register keydown on document for Escape handling
    addListener(doc_, "keydown");

    // Give initial focus to the first menu item (focus-visible for keyboard highlight)
    if (auto* first = doc_->GetElementById("mi-0")) {
        first->Focus(true);
    }

    spdlog::info("JrpgWindowScene initialized");
    return true;
}

void JrpgWindowScene::update(float dt) {
    Scene::update(dt);

    // Track popup close animation completion
    if (popup_close_timer_ > 0.0f) {
        popup_close_timer_ -= dt;
        if (popup_close_timer_ <= 0.0f) {
            popup_open_ = false;
            modal_overlay_->SetClass("visible", false);
            popup_window_->SetClass("anim-close", false);

            // Return focus to the Exit menu item
            if (auto* exit_item = doc_->GetElementById("mi-5")) {
                exit_item->Focus(true);
            }
        }
    }
}

void JrpgWindowScene::clean() {
    // Remove all listeners BEFORE unloading documents
    for (auto& [el, event, capture] : registrations_) {
        el->RemoveEventListener(event, listener_.get(), capture);
    }
    registrations_.clear();
    listener_.reset();

    unloadAllRmlDocuments();
    doc_ = nullptr;

    Scene::clean();
}

// ── Menu interaction ───────────────────────────────────────

void JrpgWindowScene::onMenuFocus(int index) {
    if (index < 0 || index >= static_cast<int>(menu_info_.size())) return;
    focused_index_ = index;

    if (info_title_el_) info_title_el_->SetInnerRML(menu_info_[index].title);
    if (info_desc_el_)  info_desc_el_->SetInnerRML(menu_info_[index].description);
}

void JrpgWindowScene::onMenuSelect(int index) {
    if (index < 0 || index >= static_cast<int>(menu_info_.size())) return;

    spdlog::info("Selected: {}", menu_info_[index].title);

    // "Exit" opens the confirm popup
    if (index == 5) {
        openPopup();
        return;
    }

    // Other items: update status bar
    if (status_text_el_) {
        status_text_el_->SetInnerRML(
            Rml::String("Selected: ") + menu_info_[index].title +
            " (not implemented in this demo)");
    }
}

// ── Popup management ───────────────────────────────────────

void JrpgWindowScene::openPopup() {
    if (popup_open_) return;
    popup_open_ = true;

    modal_overlay_->SetClass("visible", true);
    popup_window_->SetClass("anim-open", true);
    popup_window_->SetClass("anim-close", false);

    // Focus the "No" button by default (safe choice)
    if (auto* no_btn = doc_->GetElementById("popup-no")) {
        no_btn->Focus(true);
    }
}

void JrpgWindowScene::closePopup(bool confirmed) {
    if (!popup_open_ || popup_close_timer_ > 0.0f) return;

    if (confirmed) {
        spdlog::info("Exit confirmed — would return to title screen");
        if (status_text_el_) {
            status_text_el_->SetInnerRML("Exit confirmed! (returning to title screen...)");
        }
    } else {
        if (status_text_el_) {
            status_text_el_->SetInnerRML("Arrows: move cursor | Enter: confirm | Esc: cancel");
        }
    }

    // Start close animation
    popup_window_->SetClass("anim-open", false);
    popup_window_->SetClass("anim-close", true);
    popup_close_timer_ = kPopupCloseDuration;
}

// ── Helpers ────────────────────────────────────────────────

void JrpgWindowScene::addListener(Rml::Element* el, const Rml::String& event, bool capture) {
    if (!el) return;
    el->AddEventListener(event, listener_.get(), capture);
    registrations_.push_back({el, event, capture});
}

} // namespace learn::jrpg
