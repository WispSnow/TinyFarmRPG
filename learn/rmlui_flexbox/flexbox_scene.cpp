#include "flexbox_scene.h"

#include "engine/core/context.h"
#include "engine/render/opengl/gl_renderer.h"

#include <spdlog/spdlog.h>

namespace learn::rmlui {

bool FlexboxScene::init() {
    if (!Scene::init()) {
        return false;
    }

    context_.getGLRenderer().setDebugUIEnabled(true);

    if (!loadRmlDocument("ui/rmlui/learn/learn_flexbox.rml")) {
        spdlog::error("Failed to load learn_flexbox.rml");
        return false;
    }

    spdlog::info("Flexbox scene initialized.");
    return true;
}

void FlexboxScene::clean() {
    unloadAllRmlDocuments();
    Scene::clean();
}

} // namespace learn::rmlui
