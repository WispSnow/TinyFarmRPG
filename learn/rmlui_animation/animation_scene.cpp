#include "animation_scene.h"

#include "engine/core/context.h"
#include "engine/render/opengl/gl_renderer.h"
#include "engine/ui/rmlui/rml_ui_runtime.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <spdlog/spdlog.h>

#include <cstdlib>

namespace learn::rmlui {

namespace {

[[nodiscard]] Rml::Context* getRmlContext(const engine::core::Context& context) {
    auto* runtime = context.getRmlUi();
    return runtime ? runtime->getContext() : nullptr;
}

} // namespace

bool AnimationScene::init() {
    if (!Scene::init()) return false;

    context_.getGLRenderer().setDebugUIEnabled(true);

    setupDataModel();

    doc_ = loadRmlDocument("ui/rmlui/learn/learn_animation.rml");
    if (!doc_) {
        spdlog::error("AnimationScene: failed to load learn_animation.rml");
        return false;
    }

    spdlog::info("AnimationScene initialized.");
    return true;
}

void AnimationScene::setupDataModel() {
    auto* rml_ctx = getRmlContext(context_);
    if (!rml_ctx) return;

    auto ctor = rml_ctx->CreateDataModel("anim");
    if (!ctor) return;

    ctor.Bind("damage_value", &damage_value_);
    ctor.Bind("window_visible", &window_visible_);
    ctor.Bind("status_text", &status_text_);

    // ----------------------------------------------------------------
    // 伤害数字弹出：每次创建独立元素，可同时显示多个
    // ----------------------------------------------------------------
    ctor.BindEventCallback("on_hit",
        [this](Rml::DataModelHandle model, Rml::Event& /*ev*/, const Rml::VariantList& /*args*/) {
            if (!doc_) return;

            damage_value_ = 25 + (damage_value_ % 50);
            model.DirtyVariable("damage_value");

            auto* area = doc_->GetElementById("dmg-area");
            if (!area) return;

            // 随机水平偏移，避免多个数字完全重叠
            int offset_x = 20 + std::rand() % 80;

            auto new_el = doc_->CreateElement("div");
            new_el->SetClassNames("dmg-text dmg-animate");
            new_el->SetProperty("left", std::to_string(offset_x) + "dp");
            new_el->SetInnerRML("-" + std::to_string(damage_value_));

            auto* raw = new_el.get();
            area->AppendChild(std::move(new_el));
            dmg_popups_.push_back({raw, 0.0f});

            status_text_ = "Hit! -" + std::to_string(damage_value_);
            model.DirtyVariable("status_text");
        });

    // ----------------------------------------------------------------
    // 切换窗口可见性
    // ----------------------------------------------------------------
    ctor.BindEventCallback("toggle_window",
        [this](Rml::DataModelHandle model, Rml::Event& /*ev*/, const Rml::VariantList& /*args*/) {
            if (!doc_) return;
            auto* win_el = doc_->GetElementById("popup-window");
            if (!win_el) return;

            window_visible_ = !window_visible_;
            model.DirtyVariable("window_visible");

            if (window_visible_) {
                win_el->SetProperty("display", "block");
                win_el->SetProperty("opacity", "1");
                win_el->SetClass("popup-open", true);
                win_el->SetClass("popup-close", false);
                status_text_ = "Window opened";
            } else {
                win_el->SetClass("popup-open", false);
                win_el->SetClass("popup-close", true);
                close_timer_ = 0.0f;
                status_text_ = "Window closing...";
            }
            model.DirtyVariable("status_text");
        });

    model_handle_ = ctor.GetModelHandle();
}

void AnimationScene::update(float dt) {
    Scene::update(dt);

    // 清理已完成动画的伤害数字（动画时长 0.8s）
    if (!dmg_popups_.empty() && doc_) {
        auto* area = doc_->GetElementById("dmg-area");
        if (area) {
            for (auto& p : dmg_popups_)
                p.age += dt;

            // 从后往前移除，避免迭代器失效
            for (int i = static_cast<int>(dmg_popups_.size()) - 1; i >= 0; --i) {
                if (dmg_popups_[i].age >= 0.85f) {
                    area->RemoveChild(dmg_popups_[i].el);
                    dmg_popups_.erase(dmg_popups_.begin() + i);
                }
            }
        }
    }

    // 关闭动画计时：动画 0.3s 结束后隐藏元素
    if (!window_visible_ && doc_) {
        auto* win_el = doc_->GetElementById("popup-window");
        if (win_el && win_el->IsClassSet("popup-close")) {
            close_timer_ += dt;
            if (close_timer_ >= 0.32f) {
                win_el->SetProperty("display", "none");
                win_el->SetProperty("opacity", "0");
                win_el->SetClass("popup-close", false);
                close_timer_ = 0.0f;
            }
        }
    }
}

void AnimationScene::clean() {
    dmg_popups_.clear();
    unloadAllRmlDocuments();
    doc_ = nullptr;

    if (auto* rml_ctx = getRmlContext(context_)) {
        rml_ctx->RemoveDataModel("anim");
    }

    Scene::clean();
}

} // namespace learn::rmlui
