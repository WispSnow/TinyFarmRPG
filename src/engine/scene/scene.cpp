#include "engine/scene/scene.h"
#include "engine/core/context.h"
#include "engine/render/opengl/gl_renderer.h"
#include "engine/ui/rmlui/rml_ui_layer.h"
#include "engine/spatial/spatial_index_manager.h"
#include "engine/utils/events.h"
#include <atomic>
#include <spdlog/spdlog.h>
#include <entt/signal/dispatcher.hpp>

namespace engine::scene {

uint64_t Scene::nextInstanceId() {
    static std::atomic<uint64_t> counter{1};
    return counter.fetch_add(1, std::memory_order_relaxed);
}

Scene::Scene(std::string_view name, engine::core::Context& context)
    : scene_name_(name),
      instance_id_(nextInstanceId()),
      context_(context) {
    spdlog::trace("场景 '{}' (id={}) 构造完成。", scene_name_, instance_id_);
}

Scene::~Scene() = default;

bool Scene::init() {
    is_initialized_ = true;
    spdlog::trace("场景 '{}' 初始化完成。", scene_name_);
    return true;
}

void Scene::fixedUpdate(float /* delta_time */) {
    // 默认场景不参与固定步长逻辑更新。
}

void Scene::update(float /*delta_time*/) {
    if (!is_initialized_) return;
}

void Scene::render(float /*interpolation_alpha*/) {
    if (!is_initialized_) return;
}

void Scene::clean() {
    unloadAllRmlDocuments();
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

Rml::ElementDocument* Scene::loadRmlDocument(std::string_view path) {
    if (auto* layer = context_.getGLRenderer().getRmlUILayer()) {
        return layer->loadDocument(path, instance_id_);
    }
    spdlog::warn("Scene '{}': RmlUILayer 不可用，无法加载文档 '{}'。", scene_name_, path);
    return nullptr;
}

void Scene::unloadAllRmlDocuments() {
    if (auto* layer = context_.getGLRenderer().getRmlUILayer()) {
        layer->unloadDocumentsByOwner(instance_id_);
    }
}

} // namespace engine::scene
