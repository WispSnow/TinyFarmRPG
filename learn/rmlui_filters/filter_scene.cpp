#include "filter_scene.h"

#include "engine/core/context.h"
#include "engine/render/opengl/gl_renderer.h"
#include "engine/ui/rmlui/rml_ui_runtime.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <spdlog/spdlog.h>

namespace learn::rmlui {

namespace {

[[nodiscard]] Rml::Context* getRmlContext(const engine::core::Context& context) {
    auto* runtime = context.getRmlUi();
    return runtime ? runtime->getContext() : nullptr;
}

} // namespace

bool FilterScene::init() {
    if (!Scene::init()) return false;

    context_.getGLRenderer().setDebugUIEnabled(true);

    setupDataModel();

    doc_ = loadRmlDocument("ui/rmlui/learn/learn_filters.rml");
    if (!doc_) {
        spdlog::error("FilterScene: failed to load learn_filters.rml");
        return false;
    }

    spdlog::info("FilterScene initialized.");
    return true;
}

void FilterScene::setupDataModel() {
    auto* rml_ctx = getRmlContext(context_);
    if (!rml_ctx) return;

    auto ctor = rml_ctx->CreateDataModel("filters");
    if (!ctor) return;

    ctor.Bind("poisoned", &poisoned_);
    ctor.Bind("status_text", &status_text_);

    // 切换中毒状态
    ctor.BindEventCallback("toggle_poison",
        [this](Rml::DataModelHandle model, Rml::Event& /*ev*/, const Rml::VariantList& /*args*/) {
            if (!doc_) return;
            auto* avatar = doc_->GetElementById("avatar");
            if (!avatar) return;

            poisoned_ = !poisoned_;
            model.DirtyVariable("poisoned");

            avatar->SetClass("poisoned", poisoned_);

            status_text_ = poisoned_
                ? "Poisoned! hue-rotate + saturate"
                : "Poison cured";
            model.DirtyVariable("status_text");
        });

    model_handle_ = ctor.GetModelHandle();
}

void FilterScene::clean() {
    unloadAllRmlDocuments();
    doc_ = nullptr;

    if (auto* rml_ctx = getRmlContext(context_)) {
        rml_ctx->RemoveDataModel("filters");
    }

    Scene::clean();
}

} // namespace learn::rmlui
