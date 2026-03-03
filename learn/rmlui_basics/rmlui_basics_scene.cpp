#include "rmlui_basics_scene.h"

#include "engine/core/context.h"
#include "engine/render/opengl/gl_renderer.h"

#include <spdlog/spdlog.h>

namespace learn::rmlui {

bool RmlUiBasicsScene::init() {
    if (!Scene::init()) {
        return false;
    }

    auto& gl_renderer = context_.getGLRenderer();
    gl_renderer.setDebugUIEnabled(true);

    if (!gl_renderer.loadRmlUiDocument("ui/rmlui/learn/learn_hello.rml")) {
        spdlog::error("Failed to load learn_hello.rml");
        return false;
    }

    spdlog::info("RmlUi basics scene initialized.");
    return true;
}

void RmlUiBasicsScene::clean() {
    Scene::clean();
}

} // namespace learn::rmlui
