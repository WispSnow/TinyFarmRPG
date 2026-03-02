#include "rmlui_test_scene.h"

#include "engine/core/context.h"
#include "engine/debug/debug_panel.h"
#include "engine/debug/debug_ui_manager.h"
#include "engine/render/opengl/gl_renderer.h"
#include "engine/ui/rmlui/rml_ui_layer.h"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>

namespace tools::rmlui {

class RmlUiTesterControlPanel final : public engine::debug::DebugPanel {
public:
    explicit RmlUiTesterControlPanel(RmlUiTestScene& scene) : scene_(scene) {}

    [[nodiscard]] std::string_view name() const override { return "RmlUi Tester"; }

    void draw(bool& is_open) override {
        if (!ImGui::Begin("RmlUi Tester", &is_open, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::End();
            return;
        }
        scene_.drawControlPanel();
        ImGui::End();
    }

private:
    RmlUiTestScene& scene_;
};

namespace {

constexpr std::string_view kDefaultDocumentPath{"assets/ui/rmlui/demo.rml"};
constexpr std::string_view kDocumentRoot{"assets/ui/rmlui"};

void copyPathToBuffer(std::array<char, 512>& buffer, std::string_view path) {
    buffer.fill('\0');
    const std::size_t copy_len = std::min(path.size(), buffer.size() - 1);
    if (copy_len > 0) {
        std::memcpy(buffer.data(), path.data(), copy_len);
    }
    buffer[copy_len] = '\0';
}

[[nodiscard]] bool hasRmlExtension(std::filesystem::path path) {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return extension == ".rml";
}

} // namespace

RmlUiTestScene::RmlUiTestScene(std::string_view name, engine::core::Context& context)
    : engine::scene::Scene(name, context) {
    copyPathToBuffer(document_path_buffer_, kDefaultDocumentPath);
}

RmlUiTestScene::~RmlUiTestScene() {
    if (control_panel_handle_) {
        context_.getDebugUIManager().unregisterPanel(control_panel_handle_);
        control_panel_handle_ = nullptr;
    }
}

bool RmlUiTestScene::init() {
    if (!Scene::init()) {
        return false;
    }

    auto& gl_renderer = context_.getGLRenderer();
    gl_renderer.setDebugUIEnabled(true);

    auto& debug_ui = context_.getDebugUIManager();
    debug_ui.setVisible(true, engine::debug::PanelCategory::Engine);
    if (!control_panel_handle_) {
        auto panel = std::make_unique<RmlUiTesterControlPanel>(*this);
        control_panel_handle_ = debug_ui.registerPanel(std::move(panel), true, engine::debug::PanelCategory::Engine);
    }

    refreshAvailableDocuments();
    if (!loadDocument(document_path_buffer_.data())) {
        if (!available_documents_.empty()) {
            (void)loadDocument(available_documents_.front());
        }
    }

    spdlog::info("RmlUi tester scene initialized.");
    return true;
}

void RmlUiTestScene::update(float delta_time) {
    Scene::update(delta_time);
    handleHotReloadShortcuts();
}

void RmlUiTestScene::clean() {
    if (control_panel_handle_) {
        context_.getDebugUIManager().unregisterPanel(control_panel_handle_);
        control_panel_handle_ = nullptr;
    }
    current_document_ = nullptr;
    Scene::clean();
}

void RmlUiTestScene::drawControlPanel() {
    ImGui::TextUnformatted("Hotkeys: F5 or Cmd/Ctrl + R");

    if (ImGui::Button("Refresh File List")) {
        refreshAvailableDocuments();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload Current")) {
        (void)reloadDocument();
    }

    const char* combo_preview = selected_document_index_ >= 0
        ? available_documents_[static_cast<std::size_t>(selected_document_index_)].c_str()
        : "(none)";
    if (ImGui::BeginCombo("RML Files", combo_preview)) {
        for (std::size_t i = 0; i < available_documents_.size(); ++i) {
            const bool is_selected = static_cast<int>(i) == selected_document_index_;
            if (ImGui::Selectable(available_documents_[i].c_str(), is_selected)) {
                copyPathToBuffer(document_path_buffer_, available_documents_[i]);
                selected_document_index_ = static_cast<int>(i);
            }
            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    if (ImGui::InputText("Document Path", document_path_buffer_.data(), document_path_buffer_.size())) {
        updateSelectedDocumentIndex();
    }

    if (ImGui::Button("Load")) {
        (void)loadDocument(document_path_buffer_.data());
    }

    const std::string active_path{document_path_buffer_.data()};
    ImGui::Separator();
    ImGui::Text("Current: %s", active_path.empty() ? "(none)" : active_path.c_str());
    ImGui::Text("Discovered .rml files: %d", static_cast<int>(available_documents_.size()));

    const ImVec4 status_color = status_is_error_
        ? ImVec4{0.95f, 0.35f, 0.35f, 1.0f}
        : ImVec4{0.45f, 0.9f, 0.45f, 1.0f};
    ImGui::TextColored(status_color, "%s", status_message_.c_str());
}

void RmlUiTestScene::refreshAvailableDocuments() {
    available_documents_.clear();

    std::error_code error_code;
    const std::filesystem::path root_path{kDocumentRoot};
    if (!std::filesystem::exists(root_path, error_code)) {
        setStatus("assets/ui/rmlui not found.", true);
        selected_document_index_ = -1;
        return;
    }

    for (std::filesystem::recursive_directory_iterator it(root_path, error_code), end; it != end; it.increment(error_code)) {
        if (error_code) {
            break;
        }
        const auto& entry = *it;
        if (!entry.is_regular_file(error_code)) {
            continue;
        }
        if (!hasRmlExtension(entry.path())) {
            continue;
        }
        available_documents_.push_back(entry.path().generic_string());
    }

    std::sort(available_documents_.begin(), available_documents_.end());
    updateSelectedDocumentIndex();
}

void RmlUiTestScene::updateSelectedDocumentIndex() {
    const std::string current_path = document_path_buffer_.data();
    selected_document_index_ = -1;
    for (std::size_t i = 0; i < available_documents_.size(); ++i) {
        if (available_documents_[i] == current_path) {
            selected_document_index_ = static_cast<int>(i);
            return;
        }
    }
}

void RmlUiTestScene::handleHotReloadShortcuts() {
    const bool* keyboard_state = SDL_GetKeyboardState(nullptr);
    if (!keyboard_state) {
        return;
    }

    const bool f5_pressed = keyboard_state[SDL_SCANCODE_F5] != 0;
    if (f5_pressed && !previous_f5_pressed_) {
        (void)reloadDocument();
    }
    previous_f5_pressed_ = f5_pressed;

    const bool primary_modifier_pressed = keyboard_state[SDL_SCANCODE_LCTRL] != 0
        || keyboard_state[SDL_SCANCODE_RCTRL] != 0
        || keyboard_state[SDL_SCANCODE_LGUI] != 0
        || keyboard_state[SDL_SCANCODE_RGUI] != 0;
    const bool primary_reload_pressed = primary_modifier_pressed && keyboard_state[SDL_SCANCODE_R] != 0;
    if (primary_reload_pressed && !previous_primary_reload_pressed_) {
        (void)reloadDocument();
    }
    previous_primary_reload_pressed_ = primary_reload_pressed;
}

bool RmlUiTestScene::loadDocument(std::string_view path) {
    if (path.empty()) {
        setStatus("Load failed: empty path.", true);
        return false;
    }

    auto* next_document = loadRmlDocument(path);
    if (!next_document) {
        std::string message = "Load failed: ";
        message += path;
        setStatus(message, true);
        return false;
    }

    if (current_document_ && current_document_ != next_document) {
        if (auto* layer = context_.getGLRenderer().getRmlUILayer()) {
            layer->unloadDocument(current_document_);
        }
    }
    current_document_ = next_document;

    copyPathToBuffer(document_path_buffer_, std::string(path));
    updateSelectedDocumentIndex();

    std::string message = "Loaded: ";
    message += path;
    setStatus(message, false);
    return true;
}

bool RmlUiTestScene::reloadDocument() {
    const std::string active_path{document_path_buffer_.data()};
    if (active_path.empty()) {
        setStatus("Reload failed: empty path.", true);
        return false;
    }

    if (!loadDocument(active_path)) {
        return false;
    }

    std::string message = "Reloaded: ";
    message += active_path;
    setStatus(message, false);
    return true;
}

void RmlUiTestScene::setStatus(std::string_view message, bool is_error) {
    status_message_ = std::string(message);
    status_is_error_ = is_error;
}

} // namespace tools::rmlui
