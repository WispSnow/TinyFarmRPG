#include "engine/debug/panels/rmlui_debug_panel.h"

#include "engine/core/context.h"
#include "engine/ui/rmlui/rml_ui_render_backend_gl.h"
#include "engine/ui/rmlui/rml_ui_runtime.h"

#include <RmlUi/Core/DataModelHandle.h>

#include <imgui.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>

namespace engine::debug {

namespace {

constexpr std::string_view kDocumentRoot{"ui/rmlui"};
constexpr std::string_view kBindingTestPath{"ui/rmlui/tests/05_data_binding.rml"};

[[nodiscard]] bool hasRmlExtension(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext == ".rml";
}

void copyToBuffer(std::array<char, 512>& buf, std::string_view str) {
    buf.fill('\0');
    const auto len = std::min(str.size(), buf.size() - 1);
    if (len > 0) {
        std::memcpy(buf.data(), str.data(), len);
    }
}

} // namespace

RmlUiDebugPanel::RmlUiDebugPanel(engine::core::Context& context,
                                 engine::ui::rmlui::RmlUiRenderBackendGl& render_backend)
    : context_(context),
      render_backend_(render_backend) {
    refreshAvailableDocuments();
    // 初始化 message buffer
    constexpr std::string_view default_msg{"Hello RmlUi!"};
    std::memcpy(bound_message_buffer_.data(), default_msg.data(), default_msg.size());
    bound_message_buffer_[default_msg.size()] = '\0';
}

std::string_view RmlUiDebugPanel::name() const {
    return "RmlUi";
}

void RmlUiDebugPanel::draw(bool& is_open) {
    if (!is_open) {
        return;
    }

    if (!ImGui::Begin("RmlUi Debug", &is_open, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    drawFileBrowser();
    ImGui::Separator();
    drawDebugDocuments();
    ImGui::Separator();
    drawLoadedDocuments();
    ImGui::Separator();
    drawTextureFilter();
    ImGui::Separator();
    drawDataBindingTest();

    ImGui::End();
}

// ---- 文件浏览 ----

void RmlUiDebugPanel::drawFileBrowser() {
    ImGui::TextUnformatted("Document Loader");

    if (ImGui::Button("Refresh")) {
        refreshAvailableDocuments();
    }

    // 文件下拉列表
    const char* preview = selected_index_ >= 0
        ? available_documents_[static_cast<size_t>(selected_index_)].c_str()
        : "(none)";
    if (ImGui::BeginCombo("##rml_files", preview)) {
        for (size_t i = 0; i < available_documents_.size(); ++i) {
            const bool selected = static_cast<int>(i) == selected_index_;
            if (ImGui::Selectable(available_documents_[i].c_str(), selected)) {
                copyToBuffer(path_buffer_, available_documents_[i]);
                selected_index_ = static_cast<int>(i);
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    if (ImGui::InputText("Path", path_buffer_.data(), path_buffer_.size())) {
        updateSelectedIndex();
    }

    if (ImGui::Button("Load")) {
        (void)loadDocument(path_buffer_.data());
    }

    // 状态
    const ImVec4 color = status_is_error_
        ? ImVec4{0.95f, 0.35f, 0.35f, 1.0f}
        : ImVec4{0.45f, 0.9f, 0.45f, 1.0f};
    ImGui::TextColored(color, "%s", status_message_.c_str());
}

// ---- 调试文档管理（多文档） ----

void RmlUiDebugPanel::drawDebugDocuments() {
    ImGui::Text("Debug Documents: %zu", debug_documents_.size());

    if (!debug_documents_.empty() && ImGui::Button("Unload All Debug")) {
        unloadAllDebugDocuments();
    }

    // 逆序遍历，安全删除
    for (size_t i = debug_documents_.size(); i > 0; --i) {
        const size_t idx = i - 1;
        auto& entry = debug_documents_[idx];

        ImGui::PushID(static_cast<int>(idx));

        // 可见性切换
        bool visible = entry.visible;
        if (ImGui::Checkbox("##vis", &visible)) {
            if (auto* layer = context_.getRmlUi()) {
                if (visible) {
                    layer->showDocument(entry.doc);
                } else {
                    layer->hideDocument(entry.doc);
                }
            }
            entry.visible = visible;
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(entry.path.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) {
            unloadDebugDocument(idx);
        }

        ImGui::PopID();
    }
}

// ---- 全局文档列表（只读） ----

void RmlUiDebugPanel::drawLoadedDocuments() {
    auto* layer = context_.getRmlUi();
    if (!layer) {
        ImGui::TextUnformatted("RmlUiRuntime not available.");
        return;
    }

    const size_t count = layer->getDocumentCount();
    ImGui::Text("All documents: %zu  |  Active scene: %llu",
                count, static_cast<unsigned long long>(layer->getActiveSceneId()));

    if (count > 0 && ImGui::BeginTable("##rml_docs", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Path");
        ImGui::TableSetupColumn("Owner");
        ImGui::TableHeadersRow();

        layer->forEachDocument([](const std::string& path, uint64_t owner) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(path.c_str());
            ImGui::TableNextColumn();
            if (owner == 0) {
                ImGui::TextUnformatted("global");
            } else {
                ImGui::Text("%llu", static_cast<unsigned long long>(owner));
            }
        });

        ImGui::EndTable();
    }
}

void RmlUiDebugPanel::drawTextureFilter() {
    ImGui::TextUnformatted("Texture Filter");

    int filter_index = 0;
    switch (render_backend_.getTextureFilterMode()) {
    case engine::ui::rmlui::RmlUiTextureFilterMode::Nearest:
        filter_index = 0;
        break;
    case engine::ui::rmlui::RmlUiTextureFilterMode::Linear:
        filter_index = 1;
        break;
    }

    if (ImGui::RadioButton("Nearest (Pixel Art)", filter_index == 0)) {
        render_backend_.setTextureFilterMode(engine::ui::rmlui::RmlUiTextureFilterMode::Nearest);
    }
    if (ImGui::RadioButton("Linear (Smooth)", filter_index == 1)) {
        render_backend_.setTextureFilterMode(engine::ui::rmlui::RmlUiTextureFilterMode::Linear);
    }

    ImGui::TextDisabled("Affects RmlUi images and font atlases. Filters and off-screen effects remain linear.");
}

// ---- Data Binding 测试 ----

void RmlUiDebugPanel::drawDataBindingTest() {
    ImGui::TextUnformatted("Data Binding Test");

    if (!binding_test_active_) {
        if (ImGui::Button("Start Binding Test")) {
            initDataBindingTest();
        }
        return;
    }

    if (ImGui::Button("Stop Binding Test")) {
        destroyDataBindingTest();
        return;
    }

    bool dirty = false;

    if (ImGui::InputInt("Counter", &bound_counter_)) {
        dirty = true;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("+1")) {
        ++bound_counter_;
        dirty = true;
    }

    if (ImGui::InputText("Message", bound_message_buffer_.data(), bound_message_buffer_.size())) {
        dirty = true;
    }

    if (dirty && data_bridge_.isValid()) {
        data_bridge_.markAllDirty();
    }
}

void RmlUiDebugPanel::initDataBindingTest() {
    auto* layer = context_.getRmlUi();
    if (!layer || !layer->getContext()) {
        status_message_ = "Cannot start binding test: no RmlUi context.";
        status_is_error_ = true;
        return;
    }

    // 创建 data model
    auto constructor = data_bridge_.create(layer->getContext(), "debug_binding");
    if (!constructor) {
        status_message_ = "Failed to create data model 'debug_binding'.";
        status_is_error_ = true;
        return;
    }

    constructor.Bind("counter", &bound_counter_);
    constructor.BindFunc("message", [this](Rml::Variant& variant) {
        variant = Rml::String{bound_message_buffer_.data()};
    });

    // 加载测试文档
    binding_test_doc_ = layer->loadDocument(kBindingTestPath, 0);
    if (!binding_test_doc_) {
        status_message_ = "Failed to load " + std::string(kBindingTestPath);
        status_is_error_ = true;
        return;
    }

    binding_test_active_ = true;
    status_message_ = "Binding test started.";
    status_is_error_ = false;
}

void RmlUiDebugPanel::destroyDataBindingTest() {
    auto* layer = context_.getRmlUi();
    if (layer && binding_test_doc_) {
        layer->unloadDocument(binding_test_doc_);
    }
    binding_test_doc_ = nullptr;
    data_bridge_.destroy();
    binding_test_active_ = false;

    status_message_ = "Binding test stopped.";
    status_is_error_ = false;
}

// ---- 文件扫描 ----

void RmlUiDebugPanel::refreshAvailableDocuments() {
    available_documents_.clear();

    std::error_code ec;
    const std::filesystem::path root{kDocumentRoot};
    if (!std::filesystem::exists(root, ec)) {
        status_message_ = "ui/rmlui not found.";
        status_is_error_ = true;
        selected_index_ = -1;
        return;
    }

    for (std::filesystem::recursive_directory_iterator it(root, ec), end; it != end; it.increment(ec)) {
        if (ec) {
            break;
        }
        if (!it->is_regular_file(ec) || !hasRmlExtension(it->path())) {
            continue;
        }
        available_documents_.push_back(it->path().generic_string());
    }

    std::sort(available_documents_.begin(), available_documents_.end());
    updateSelectedIndex();

    status_message_ = "Found " + std::to_string(available_documents_.size()) + " .rml files.";
    status_is_error_ = false;
}

void RmlUiDebugPanel::updateSelectedIndex() {
    const std::string current{path_buffer_.data()};
    selected_index_ = -1;
    for (size_t i = 0; i < available_documents_.size(); ++i) {
        if (available_documents_[i] == current) {
            selected_index_ = static_cast<int>(i);
            return;
        }
    }
}

// ---- 文档加载/卸载 ----

bool RmlUiDebugPanel::loadDocument(std::string_view path) {
    auto* layer = context_.getRmlUi();
    if (!layer) {
        status_message_ = "RmlUiRuntime not available.";
        status_is_error_ = true;
        return false;
    }

    if (path.empty()) {
        status_message_ = "Load failed: empty path.";
        status_is_error_ = true;
        return false;
    }

    auto* doc = layer->loadDocument(path, 0);
    if (!doc) {
        status_message_ = "Load failed: " + std::string(path);
        status_is_error_ = true;
        return false;
    }

    debug_documents_.push_back({doc, std::string(path), true});

    status_message_ = "Loaded: " + std::string(path);
    status_is_error_ = false;
    return true;
}

void RmlUiDebugPanel::unloadDebugDocument(size_t index) {
    if (index >= debug_documents_.size()) {
        return;
    }

    auto* layer = context_.getRmlUi();
    if (layer) {
        layer->unloadDocument(debug_documents_[index].doc);
    }
    debug_documents_.erase(debug_documents_.begin() + static_cast<ptrdiff_t>(index));
}

void RmlUiDebugPanel::unloadAllDebugDocuments() {
    auto* layer = context_.getRmlUi();
    for (auto& entry : debug_documents_) {
        if (layer) {
            layer->unloadDocument(entry.doc);
        }
    }
    debug_documents_.clear();
}

} // namespace engine::debug
