#include "spritesheet_scene.h"

#include "engine/core/context.h"
#include "engine/render/opengl/gl_renderer.h"

#include <RmlUi/Core/ElementDocument.h>
#include <spdlog/spdlog.h>

namespace learn::rmlui {

bool SpritesheetScene::init() {
    if (!Scene::init()) return false;

    context_.getGLRenderer().setDebugUIEnabled(true);

    doc_ = loadRmlDocument("ui/rmlui/learn/learn_spritesheet.rml");
    if (!doc_) {
        spdlog::error("SpritesheetScene: failed to load learn_spritesheet.rml");
        return false;
    }

    spdlog::info("SpritesheetScene initialized.");
    return true;
}

void SpritesheetScene::clean() {
    unloadAllRmlDocuments();
    doc_ = nullptr;
    Scene::clean();
}

} // namespace learn::rmlui
