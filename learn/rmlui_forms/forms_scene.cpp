#include "forms_scene.h"

#include "engine/core/context.h"
#include "engine/render/opengl/gl_renderer.h"
#include "engine/ui/rmlui/rml_ui_layer.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/ElementDocument.h>
#include <spdlog/spdlog.h>

namespace learn::rmlui {

bool FormsScene::init() {
    if (!Scene::init()) return false;

    context_.getGLRenderer().setDebugUIEnabled(true);

    if (!setupDataModel()) {
        spdlog::error("FormsScene: failed to setup data model");
        return false;
    }

    doc_ = loadRmlDocument("ui/rmlui/learn/learn_forms.rml");
    if (!doc_) {
        spdlog::error("FormsScene: failed to load learn_forms.rml");
        return false;
    }

    spdlog::info("FormsScene initialized. BGM={} SE={} Speed={} Fullscreen={}",
        settings_.bgm_volume, settings_.se_volume,
        settings_.text_speed, settings_.fullscreen);
    return true;
}

bool FormsScene::setupDataModel() {
    auto* rml_ctx = context_.getGLRenderer().getRmlUILayer()->getContext();
    if (!rml_ctx) return false;

    Rml::DataModelConstructor constructor = rml_ctx->CreateDataModel("settings");
    if (!constructor) {
        spdlog::error("FormsScene: CreateDataModel('settings') failed — model already exists?");
        return false;
    }

    // ----------------------------------------------------------------
    // 1. 标量绑定（data-value / data-checked 双向绑定的目标变量）
    // ----------------------------------------------------------------
    constructor.Bind("bgm_volume",  &settings_.bgm_volume);
    constructor.Bind("se_volume",   &settings_.se_volume);
    constructor.Bind("text_speed",  &settings_.text_speed);
    constructor.Bind("fullscreen",  &settings_.fullscreen);
    constructor.Bind("window_mode", &settings_.window_mode);
    constructor.Bind("player_name", &settings_.player_name);
    constructor.Bind("notes",       &settings_.notes);
    constructor.Bind("submit_log",  &settings_.submit_log);

    // ----------------------------------------------------------------
    // 2. 表单提交回调：data-event-submit="on_submit"
    //    submit 事件的参数包含所有表单控件的 name:value 键值对
    // ----------------------------------------------------------------
    constructor.BindEventCallback("on_submit",
        [this](Rml::DataModelHandle model, Rml::Event& ev, const Rml::VariantList& /*args*/) {
            // 从 submit 事件参数中读取表单控件的值
            auto name = ev.GetParameter<Rml::String>("player_name", "");
            auto bgm  = ev.GetParameter<Rml::String>("bgm_volume", "");
            auto se   = ev.GetParameter<Rml::String>("se_volume", "");
            auto speed = ev.GetParameter<Rml::String>("text_speed", "");

            settings_.submit_log = "Saved! Name=" + name +
                " BGM=" + bgm + " SE=" + se + " Speed=" + speed;
            model.DirtyVariable("submit_log");

            spdlog::info("[on_submit] player={} bgm={} se={} speed={} fullscreen={} window={}",
                name, bgm, se, speed, settings_.fullscreen, settings_.window_mode);
        });

    // ----------------------------------------------------------------
    // 3. 重置回调：data-event-click="on_reset"
    //    恢复所有设置到默认值，调用 DirtyAllVariables 一次性刷新
    // ----------------------------------------------------------------
    constructor.BindEventCallback("on_reset",
        [this](Rml::DataModelHandle model, Rml::Event& /*ev*/, const Rml::VariantList& /*args*/) {
            settings_.bgm_volume  = 70;
            settings_.se_volume   = 80;
            settings_.text_speed  = "medium";
            settings_.fullscreen  = false;
            settings_.window_mode = "windowed";
            settings_.player_name = "Hero";
            settings_.notes.clear();
            settings_.submit_log  = "(reset)";
            model.DirtyAllVariables();

            spdlog::info("[on_reset] all settings restored to defaults");
        });

    model_handle_ = constructor.GetModelHandle();
    return true;
}

void FormsScene::clean() {
    unloadAllRmlDocuments();
    doc_ = nullptr;

    if (auto* rml_ctx = context_.getGLRenderer().getRmlUILayer()->getContext()) {
        rml_ctx->RemoveDataModel("settings");
    }

    Scene::clean();
}

} // namespace learn::rmlui
