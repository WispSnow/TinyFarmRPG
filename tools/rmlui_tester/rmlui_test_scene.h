#pragma once

#include "engine/scene/scene.h"

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace engine::debug {
class DebugPanel;
}

namespace tools::rmlui {

class RmlUiTesterControlPanel;

class RmlUiTestScene final : public engine::scene::Scene {
    friend class RmlUiTesterControlPanel;

public:
    RmlUiTestScene(std::string_view name, engine::core::Context& context);
    ~RmlUiTestScene() override;

    [[nodiscard]] bool init() override;
    void update(float delta_time) override;
    void clean() override;

private:
    void drawControlPanel();
    void refreshAvailableDocuments();
    void updateSelectedDocumentIndex();
    void handleHotReloadShortcuts();

    [[nodiscard]] bool loadDocument(std::string_view path);
    [[nodiscard]] bool reloadDocument();
    void setStatus(std::string_view message, bool is_error);

    engine::debug::DebugPanel* control_panel_handle_{nullptr};
    std::vector<std::string> available_documents_{};
    std::array<char, 512> document_path_buffer_{};
    std::string status_message_{"Ready"};
    bool status_is_error_{false};
    int selected_document_index_{-1};

    bool previous_f5_pressed_{false};
    bool previous_primary_reload_pressed_{false};
};

} // namespace tools::rmlui
