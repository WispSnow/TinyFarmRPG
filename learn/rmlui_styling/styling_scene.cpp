#include "styling_scene.h"

#include "engine/core/context.h"
#include "engine/render/opengl/gl_renderer.h"

#include <spdlog/spdlog.h>

namespace learn::rmlui {

bool StylingScene::init() {
    if (!Scene::init()) {
        return false;
    }

    context_.getGLRenderer().setDebugUIEnabled(true);

    if (!loadRmlDocument("ui/rmlui/learn/learn_styling.rml")) {
        spdlog::error("Failed to load learn_styling.rml");
        return false;
    }

    spdlog::info("Styling scene initialized.");
    return true;
}

void StylingScene::clean() {
    unloadAllRmlDocuments();
    Scene::clean();
}

} // namespace learn::rmlui
