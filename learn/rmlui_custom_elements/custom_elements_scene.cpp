#include "custom_elements_scene.h"
#include "element_hp_bar.h"

#include "engine/core/context.h"
#include "engine/render/opengl/gl_renderer.h"
#include "engine/ui/rmlui/rml_ui_layer.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Factory.h>
#include <spdlog/spdlog.h>

#include <algorithm>

namespace learn::rmlui {

bool CustomElementsScene::init() {
    if (!Scene::init()) return false;

    context_.getGLRenderer().setDebugUIEnabled(true);

    // ----------------------------------------------------------------
    // 1. 注册自定义元素 <hp-bar>（全局注册，只需一次）
    //    ElementInstancerGeneric<T> 是 RmlUi 提供的通用工厂模板
    // ----------------------------------------------------------------
    static Rml::ElementInstancerGeneric<ElementHpBar> hp_bar_instancer;
    static bool registered = false;
    if (!registered) {
        Rml::Factory::RegisterElementInstancer("hp-bar", &hp_bar_instancer);
        registered = true;
        spdlog::info("Registered custom element: <hp-bar>");
    }

    // ----------------------------------------------------------------
    // 2. 创建数据模型（必须在加载文档前）
    // ----------------------------------------------------------------
    if (!setupDataModel()) {
        spdlog::error("CustomElementsScene: failed to setup data model");
        return false;
    }

    // ----------------------------------------------------------------
    // 3. 加载两个文档——演示多文档管理
    //    HUD 始终可见，Menu 初始隐藏
    // ----------------------------------------------------------------
    hud_doc_ = loadRmlDocument("ui/rmlui/learn/learn_custom_hud.rml");
    if (!hud_doc_) {
        spdlog::error("CustomElementsScene: failed to load learn_custom_hud.rml");
        return false;
    }

    menu_doc_ = loadRmlDocument("ui/rmlui/learn/learn_custom_menu.rml");
    if (menu_doc_) {
        menu_doc_->Hide();
    }

    spdlog::info("CustomElementsScene initialized. HP={}/{} MP={}/{}",
        data_.hp, data_.max_hp, data_.mp, data_.max_mp);
    return true;
}

bool CustomElementsScene::setupDataModel() {
    auto* rml_ctx = context_.getGLRenderer().getRmlUILayer()->getContext();
    if (!rml_ctx) return false;

    Rml::DataModelConstructor constructor = rml_ctx->CreateDataModel("demo");
    if (!constructor) {
        spdlog::error("CustomElementsScene: CreateDataModel('demo') failed");
        return false;
    }

    // 标量绑定
    constructor.Bind("hp",     &data_.hp);
    constructor.Bind("max_hp", &data_.max_hp);
    constructor.Bind("mp",     &data_.mp);
    constructor.Bind("max_mp", &data_.max_mp);
    constructor.Bind("gold",   &data_.gold);

    // ----------------------------------------------------------------
    // 事件回调：Damage — 修改 HP + 通过 DOM 操作添加日志
    // ----------------------------------------------------------------
    constructor.BindEventCallback("on_damage",
        [this](Rml::DataModelHandle model, Rml::Event& /*ev*/, const Rml::VariantList& /*args*/) {
            constexpr int dmg = 15;
            data_.hp = std::max(0, data_.hp - dmg);
            model.DirtyVariable("hp");
            addLogEntry("Took " + std::to_string(dmg) + " damage! HP: " +
                std::to_string(data_.hp) + "/" + std::to_string(data_.max_hp));
        });

    // ----------------------------------------------------------------
    // 事件回调：Heal — 修改 HP
    // ----------------------------------------------------------------
    constructor.BindEventCallback("on_heal",
        [this](Rml::DataModelHandle model, Rml::Event& /*ev*/, const Rml::VariantList& /*args*/) {
            constexpr int heal = 20;
            data_.hp = std::min(data_.hp + heal, data_.max_hp);
            model.DirtyVariable("hp");
            addLogEntry("Healed " + std::to_string(heal) + "! HP: " +
                std::to_string(data_.hp) + "/" + std::to_string(data_.max_hp));
        });

    // ----------------------------------------------------------------
    // 事件回调：Use MP
    // ----------------------------------------------------------------
    constructor.BindEventCallback("on_mp_use",
        [this](Rml::DataModelHandle model, Rml::Event& /*ev*/, const Rml::VariantList& /*args*/) {
            constexpr int cost = 10;
            if (data_.mp < cost) {
                addLogEntry("Not enough MP!");
                return;
            }
            data_.mp -= cost;
            model.DirtyVariable("mp");
            addLogEntry("Used " + std::to_string(cost) + " MP! MP: " +
                std::to_string(data_.mp) + "/" + std::to_string(data_.max_mp));
        });

    // ----------------------------------------------------------------
    // 事件回调：Toggle Menu — 演示多文档 Show / Hide / PullToFront
    // ----------------------------------------------------------------
    constructor.BindEventCallback("toggle_menu",
        [this](Rml::DataModelHandle /*model*/, Rml::Event& /*ev*/, const Rml::VariantList& /*args*/) {
            if (!menu_doc_) return;
            menu_visible_ = !menu_visible_;
            if (menu_visible_) {
                menu_doc_->Show();
                menu_doc_->PullToFront();
                spdlog::info("[menu] opened (PullToFront)");
            } else {
                menu_doc_->Hide();
                spdlog::info("[menu] closed (Hide)");
            }
        });

    // ----------------------------------------------------------------
    // 事件回调：Close Menu
    // ----------------------------------------------------------------
    constructor.BindEventCallback("close_menu",
        [this](Rml::DataModelHandle /*model*/, Rml::Event& /*ev*/, const Rml::VariantList& /*args*/) {
            if (menu_doc_) {
                menu_doc_->Hide();
                menu_visible_ = false;
                spdlog::info("[menu] closed");
            }
        });

    // ----------------------------------------------------------------
    // 事件回调：Clear Log — 演示 RemoveChild 清空
    // ----------------------------------------------------------------
    constructor.BindEventCallback("clear_log",
        [this](Rml::DataModelHandle /*model*/, Rml::Event& /*ev*/, const Rml::VariantList& /*args*/) {
            if (!hud_doc_) return;
            auto* log = hud_doc_->GetElementById("action-log");
            if (!log) return;
            log->SetInnerRML("");
            spdlog::info("[log] cleared via SetInnerRML(\"\")");
        });

    model_handle_ = constructor.GetModelHandle();
    return true;
}

/// 通过 DOM 操作动态添加日志条目
/// 演示：GetElementById + CreateElement + SetInnerRML + AppendChild + RemoveChild
void CustomElementsScene::addLogEntry(const std::string& text) {
    if (!hud_doc_) return;

    auto* log = hud_doc_->GetElementById("action-log");
    if (!log) return;

    // CreateElement 创建新元素
    auto entry = hud_doc_->CreateElement("div");
    entry->SetClassNames("log-entry");

    // SetInnerRML 设置元素内容
    entry->SetInnerRML("&gt; " + text);

    // AppendChild 添加到日志容器
    log->AppendChild(std::move(entry));

    // RemoveChild 移除最早的条目（保持最多 6 条）
    while (log->GetNumChildren() > 6) {
        log->RemoveChild(log->GetFirstChild());
    }
}

void CustomElementsScene::clean() {
    unloadAllRmlDocuments();
    hud_doc_  = nullptr;
    menu_doc_ = nullptr;

    if (auto* rml_ctx = context_.getGLRenderer().getRmlUILayer()->getContext()) {
        rml_ctx->RemoveDataModel("demo");
    }

    Scene::clean();
}

} // namespace learn::rmlui
