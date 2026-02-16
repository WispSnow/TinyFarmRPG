#include "engine/debug/panels/scene_debug_panel.h"
#include "engine/scene/scene_manager.h"
#include "engine/scene/scene.h"
#include <imgui.h>
#include <entt/entity/registry.hpp>

namespace engine::debug {

SceneDebugPanel::SceneDebugPanel(engine::scene::SceneManager& scene_manager)
    : scene_manager_(scene_manager) {
}

std::string_view SceneDebugPanel::name() const {
    return "Scene";
}

void SceneDebugPanel::draw(bool& is_open) {
    if (!is_open) {
        return;
    }

    if (!ImGui::Begin("Scene Debug", &is_open, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    // 显示场景栈大小
    const size_t stack_size = scene_manager_.getSceneStackSize();
    ImGui::Text("场景栈大小: %zu", stack_size);

    // 获取当前场景
    engine::scene::Scene* current_scene = scene_manager_.getCurrentScene();
    if (current_scene) {
        ImGui::Separator();
        ImGui::Text("当前场景: %s", current_scene->getName().data());
        
        // 显示当前场景中的实体数量
        entt::registry& registry = current_scene->getRegistry();
        auto& view = registry.storage<entt::entity>();
        const auto entity_count = view.size();
        ImGui::Text("实体数量: %zu", entity_count);
    } 
    ImGui::End();
}

} // namespace engine::debug
