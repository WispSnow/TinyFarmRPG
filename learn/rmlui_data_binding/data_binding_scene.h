#pragma once

#include "engine/scene/scene.h"

#include <RmlUi/Core/DataModelHandle.h>

#include <string>
#include <vector>

namespace Rml {
class ElementDocument;
} // namespace Rml

namespace learn::rmlui {

/// 技能数据结构体，需通过 RegisterStruct 向数据模型注册
struct Skill {
    std::string name;
    std::string type;
    int         mp_cost = 0;
};

/// 角色数据，各字段通过 constructor.Bind() 逐一绑定到 data model
struct CharacterData {
    std::string name     = "Aelindra";
    std::string job      = "White Mage";
    int         level    = 12;
    int         hp       = 85;
    int         max_hp   = 100;
    int         mp       = 42;
    int         max_mp   = 60;
    int         gold     = 9876;
    bool        is_poisoned = false;
    bool        is_stunned  = false;
    std::vector<Skill> skills;
    std::string last_skill_used = "(none)";
};

/// L06 数据绑定演示场景
class DataBindingScene final : public engine::scene::Scene {
public:
    using Scene::Scene;

    [[nodiscard]] bool init() override;
    void update(float delta_time) override;
    void clean() override;

private:
    /// 创建并配置 data model "character"
    bool setupDataModel();

    Rml::ElementDocument* doc_          = nullptr;
    Rml::DataModelHandle  model_handle_;
    CharacterData         char_data_;
    float                 regen_timer_  = 0.0f;
};

} // namespace learn::rmlui
