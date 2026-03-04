#include "data_binding_scene.h"

#include "engine/core/context.h"
#include "engine/render/opengl/gl_renderer.h"
#include "engine/ui/rmlui/rml_ui_layer.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/ElementDocument.h>
#include <spdlog/spdlog.h>

#include <algorithm>

namespace learn::rmlui {

bool DataBindingScene::init() {
    if (!Scene::init()) return false;

    context_.getGLRenderer().setDebugUIEnabled(true);

    // 初始化技能列表数据
    char_data_.skills = {
        {"Cure",    "Magic",    8},
        {"Fire",    "Magic",   12},
        {"Slash",   "Physical", 0},
        {"Thunder", "Magic",   15},
        {"Potion",  "Item",     0},
    };

    // 必须先建立数据模型，再加载 RML 文档（否则文档内绑定无法解析）
    if (!setupDataModel()) {
        spdlog::error("DataBindingScene: failed to setup data model");
        return false;
    }

    doc_ = loadRmlDocument("ui/rmlui/learn/learn_data_binding.rml");
    if (!doc_) {
        spdlog::error("DataBindingScene: failed to load learn_data_binding.rml");
        return false;
    }

    spdlog::info("DataBindingScene initialized. HP={}/{} MP={}/{}",
        char_data_.hp, char_data_.max_hp, char_data_.mp, char_data_.max_mp);
    return true;
}

bool DataBindingScene::setupDataModel() {
    auto* rml_ctx = context_.getGLRenderer().getRmlUILayer()->getContext();
    if (!rml_ctx) return false;

    Rml::DataModelConstructor constructor = rml_ctx->CreateDataModel("character");
    if (!constructor) {
        spdlog::error("DataBindingScene: CreateDataModel('character') failed — model already exists?");
        return false;
    }

    // ----------------------------------------------------------------
    // 1. 注册结构体类型（必须在注册对应数组之前完成）
    // ----------------------------------------------------------------
    if (auto skill_handle = constructor.RegisterStruct<Skill>()) {
        skill_handle.RegisterMember("name",    &Skill::name);
        skill_handle.RegisterMember("type",    &Skill::type);
        skill_handle.RegisterMember("mp_cost", &Skill::mp_cost);
    }
    constructor.RegisterArray<std::vector<Skill>>();

    // ----------------------------------------------------------------
    // 2. 绑定标量变量（每个字段独立绑定，C++ 改值后调用 DirtyVariable 刷新）
    // ----------------------------------------------------------------
    constructor.Bind("name",            &char_data_.name);
    constructor.Bind("job",             &char_data_.job);
    constructor.Bind("level",           &char_data_.level);
    constructor.Bind("hp",              &char_data_.hp);
    constructor.Bind("max_hp",          &char_data_.max_hp);
    constructor.Bind("mp",              &char_data_.mp);
    constructor.Bind("max_mp",          &char_data_.max_mp);
    constructor.Bind("gold",            &char_data_.gold);
    constructor.Bind("is_poisoned",     &char_data_.is_poisoned);
    constructor.Bind("is_stunned",      &char_data_.is_stunned);
    constructor.Bind("last_skill_used", &char_data_.last_skill_used);

    // ----------------------------------------------------------------
    // 3. 绑定结构体数组
    // ----------------------------------------------------------------
    constructor.Bind("skills", &char_data_.skills);

    // ----------------------------------------------------------------
    // 4. 注册 Transform 函数：{{gold | format_gold}}
    //    Transform 函数接收 VariantList 参数，返回格式化后的 Variant
    // ----------------------------------------------------------------
    constructor.RegisterTransformFunc("format_gold",
        [](const Rml::VariantList& args) -> Rml::Variant {
            if (args.empty()) return {};
            return Rml::Variant(Rml::String(std::to_string(args[0].Get<int>()) + " G"));
        });

    // ----------------------------------------------------------------
    // 5. 注册事件回调：data-event-click="use_skill(it_index)"
    //    it_index 由 data-for 提供，作为第一个参数传入
    // ----------------------------------------------------------------
    constructor.BindEventCallback("use_skill",
        [this](Rml::DataModelHandle model, Rml::Event& /*ev*/, const Rml::VariantList& args) {
            if (args.empty()) return;
            const auto idx = args[0].Get<size_t>();
            if (idx >= char_data_.skills.size()) return;
            const auto& skill = char_data_.skills[idx];

            if (char_data_.mp < skill.mp_cost) {
                spdlog::info("[use_skill] Not enough MP for '{}' (cost={}, have={})",
                    skill.name, skill.mp_cost, char_data_.mp);
                return;
            }
            char_data_.mp -= skill.mp_cost;
            char_data_.last_skill_used = skill.name;

            // 只对变化的变量调用 DirtyVariable，避免全量刷新
            model.DirtyVariable("mp");
            model.DirtyVariable("last_skill_used");

            spdlog::info("[use_skill] '{}' (cost={}) → remaining MP={}",
                skill.name, skill.mp_cost, char_data_.mp);
        });

    // ----------------------------------------------------------------
    // 6. 注册事件回调：data-event-click="toggle_poison"
    //    演示 data-if 变量改变后 UI 自动刷新
    // ----------------------------------------------------------------
    constructor.BindEventCallback("toggle_poison",
        [this](Rml::DataModelHandle model, Rml::Event& /*ev*/, const Rml::VariantList& /*args*/) {
            char_data_.is_poisoned = !char_data_.is_poisoned;
            model.DirtyVariable("is_poisoned");
            spdlog::info("[toggle_poison] is_poisoned={}", char_data_.is_poisoned);
        });

    model_handle_ = constructor.GetModelHandle();
    return true;
}

void DataBindingScene::update(float delta_time) {
    Scene::update(delta_time);

    // 每 3 秒自动回复 HP+5 / MP+3，演示 C++ 修改数据 + DirtyVariable → UI 刷新
    regen_timer_ += delta_time;
    if (regen_timer_ < 3.0f) return;
    regen_timer_ = 0.0f;

    bool dirty_hp = false;
    bool dirty_mp = false;

    if (char_data_.hp < char_data_.max_hp) {
        char_data_.hp = std::min(char_data_.hp + 5, char_data_.max_hp);
        dirty_hp = true;
    }
    if (char_data_.mp < char_data_.max_mp) {
        char_data_.mp = std::min(char_data_.mp + 3, char_data_.max_mp);
        dirty_mp = true;
    }

    if (dirty_hp) model_handle_.DirtyVariable("hp");
    if (dirty_mp) model_handle_.DirtyVariable("mp");

    if (dirty_hp || dirty_mp) {
        spdlog::info("[regen] HP={}/{} MP={}/{}",
            char_data_.hp, char_data_.max_hp, char_data_.mp, char_data_.max_mp);
    }
}

void DataBindingScene::clean() {
    // 卸载 RML 文档
    unloadAllRmlDocuments();
    doc_ = nullptr;

    // 移除数据模型（场景重载时 CreateDataModel 同名会失败，必须先删除）
    if (auto* rml_ctx = context_.getGLRenderer().getRmlUILayer()->getContext()) {
        rml_ctx->RemoveDataModel("character");
    }

    Scene::clean();
}

} // namespace learn::rmlui
