#include "save_load_debug_panel.h"

#include "game/save/save_service.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>

namespace game::debug {

SaveLoadDebugPanel::SaveLoadDebugPanel(game::save::SaveService& save_service)
    : save_service_(save_service) {
    path_ = game::save::SaveService::slotPath(slot_).string();
}

std::string_view SaveLoadDebugPanel::name() const {
    return "Save/Load";
}

void SaveLoadDebugPanel::draw(bool& is_open) {
    if (!is_open) {
        return;
    }

    if (!ImGui::Begin("Save/Load Debug", &is_open, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    ImGui::InputInt("Slot", &slot_);
    slot_ = std::clamp(slot_, 0, 9);
    ImGui::SameLine();
    if (ImGui::Button("Use Slot Path")) {
        path_ = game::save::SaveService::slotPath(slot_).string();
    }

    char path_buf[512];
    std::snprintf(path_buf, sizeof(path_buf), "%s", path_.c_str());
    if (ImGui::InputText("Path", path_buf, sizeof(path_buf))) {
        path_ = path_buf;
    }

    if (ImGui::Button("Save")) {
        std::string err;
        if (save_service_.saveToFile(std::filesystem::path(path_), err)) {
            status_ = "Save OK";
        } else {
            status_ = "Save FAILED: " + err;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Load")) {
        std::string err;
        if (save_service_.loadFromFile(std::filesystem::path(path_), err)) {
            status_ = "Load OK";
        } else {
            status_ = "Load FAILED: " + err;
        }
    }

    if (!status_.empty()) {
        ImGui::Separator();
        ImGui::TextWrapped("%s", status_.c_str());
    }

    ImGui::End();
}

} // namespace game::debug
