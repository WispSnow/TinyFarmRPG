#include "engine/scene/scene.h"
#include "engine/core/context.h"
#include "engine/spatial/spatial_index_manager.h"
#include "engine/ui/ui_manager.h"
#include "engine/utils/events.h"
#include <spdlog/spdlog.h>
#include <entt/signal/dispatcher.hpp>

namespace engine::scene {

Scene::Scene(std::string_view name, engine::core::Context& context)
    : scene_name_(name),
      context_(context) {
    spdlog::trace("场景 '{}' 构造完成。", scene_name_);
}

Scene::~Scene() = default;

bool Scene::init() {
    is_initialized_ = true;
    spdlog::trace("场景 '{}' 初始化完成。", scene_name_);
    return true;
}

void Scene::update(float delta_time) {
    if (!is_initialized_) return;

    // 更新UI管理器（部分场景可能不需要 UI）
    if (ui_manager_) {
        ui_manager_->update(delta_time, context_);
    }

}

void Scene::render() {
    if (!is_initialized_) return;

    // 渲染UI管理器（部分场景可能不需要 UI）
    if (ui_manager_) {
        ui_manager_->render(context_);
    }
}

void Scene::clean() {
    if (!is_initialized_) return;

    context_.getSpatialIndexManager().resetIfUsingRegistry(&registry_);
    registry_ = entt::registry{};  // 重建 registry，确保实体/组件与 ctx 一并重置
    is_initialized_ = false;        // 清理完成后，设置场景为未初始化
    spdlog::trace("场景 '{}' 清理完成。", scene_name_);
}

void Scene::requestPopScene()
{
    context_.getDispatcher().trigger<engine::utils::PopSceneEvent>();
}

void Scene::requestPushScene(std::unique_ptr<engine::scene::Scene>&& scene)
{
    context_.getDispatcher().trigger<engine::utils::PushSceneEvent>(engine::utils::PushSceneEvent{std::move(scene)});
}

void Scene::requestReplaceScene(std::unique_ptr<engine::scene::Scene>&& scene)
{
    context_.getDispatcher().trigger<engine::utils::ReplaceSceneEvent>(engine::utils::ReplaceSceneEvent{std::move(scene)});
}

void Scene::quit()
{
    context_.getDispatcher().trigger<engine::utils::QuitEvent>();
}

} // namespace engine::scene 
