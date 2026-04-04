#pragma once

#include "engine/debug/debug_panel.h"
#include "engine/ui/rmlui/rml_data_bridge.h"

#include <array>
#include <string>
#include <vector>

namespace Rml {
class ElementDocument;
}

namespace engine::render::opengl {
class GLRenderer;
}

namespace engine::core {
class Context;
}

namespace engine::ui::rmlui {
class RmlUiRenderBackendGl;
}

namespace engine::debug {

class RmlUiDebugPanel final : public DebugPanel {
public:
    RmlUiDebugPanel(engine::core::Context& context, engine::ui::rmlui::RmlUiRenderBackendGl& render_backend);

    [[nodiscard]] std::string_view name() const override;
    void draw(bool& is_open) override;

private:
    void drawFileBrowser();
    void drawDebugDocuments();
    void drawLoadedDocuments();
    void drawTextureFilter();
    void drawDataBindingTest();

    void refreshAvailableDocuments();
    void updateSelectedIndex();

    [[nodiscard]] bool loadDocument(std::string_view path);
    void unloadDebugDocument(size_t index);
    void unloadAllDebugDocuments();

    void initDataBindingTest();
    void destroyDataBindingTest();

    engine::core::Context& context_;
    engine::ui::rmlui::RmlUiRenderBackendGl& render_backend_;

    // 文件浏览
    std::vector<std::string> available_documents_;
    std::array<char, 512> path_buffer_{};
    std::string status_message_{"Ready"};
    bool status_is_error_{false};
    int selected_index_{-1};

    // 调试文档列表（支持多文档并行显示）
    struct DebugDocEntry {
        Rml::ElementDocument* doc{nullptr};
        std::string path;
        bool visible{true};
    };
    std::vector<DebugDocEntry> debug_documents_;

    // Data binding 测试
    engine::ui::rmlui::RmlDataBridge data_bridge_;
    int bound_counter_{0};
    std::array<char, 128> bound_message_buffer_{};
    Rml::ElementDocument* binding_test_doc_{nullptr};
    bool binding_test_active_{false};
};

} // namespace engine::debug
