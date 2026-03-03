#include "box_model_scene.h"

#include "engine/core/context.h"
#include "engine/render/opengl/gl_renderer.h"

#include <spdlog/spdlog.h>

namespace learn::rmlui {

bool BoxModelScene::init() {
    if (!Scene::init()) {
        return false;
    }

    context_.getGLRenderer().setDebugUIEnabled(true);

    if (!loadRmlDocument("ui/rmlui/learn/learn_box_model.rml")) {
        spdlog::error("Failed to load learn_box_model.rml");
        return false;
    }

    spdlog::info("Box model scene initialized.");
    return true;
}

void BoxModelScene::clean() {
    unloadAllRmlDocuments();
    Scene::clean();
}

} // namespace learn::rmlui
