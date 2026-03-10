#pragma once

#include "engine/scene/scene.h"

#include <RmlUi/Core/DataModelHandle.h>

#include <string>

namespace Rml {
class ElementDocument;
} // namespace Rml

namespace learn::rmlui {

/// 场景演示数据
struct DemoData {
    int         hp       = 80;
    int         max_hp   = 100;
    int         mp       = 45;
    int         max_mp   = 60;
    int         gold     = 1234;
};

/// L08 自定义元素 + 文档管理演示场景
class CustomElementsScene final : public engine::scene::Scene {
public:
    using Scene::Scene;

    [[nodiscard]] bool init() override;
    void clean() override;

private:
    bool setupDataModel();
    void addLogEntry(const std::string& text);

    Rml::ElementDocument* hud_doc_   = nullptr;
    Rml::ElementDocument* menu_doc_  = nullptr;
    Rml::DataModelHandle  model_handle_;
    DemoData              data_;
    bool                  menu_visible_ = false;
};

} // namespace learn::rmlui
