#pragma once

#include "engine/scene/scene.h"

#include <RmlUi/Core/DataModelHandle.h>

#include <string>

namespace Rml {
class ElementDocument;
} // namespace Rml

namespace learn::rmlui {

/// 游戏设置数据，各字段通过 data-value / data-checked 与表单控件双向绑定
struct SettingsData {
    int         bgm_volume  = 70;
    int         se_volume   = 80;
    std::string text_speed  = "medium";
    bool        fullscreen  = false;
    std::string window_mode = "windowed";
    std::string player_name = "Hero";
    std::string notes;
    std::string submit_log  = "(none)";
};

/// L07 表单控件演示场景
class FormsScene final : public engine::scene::Scene {
public:
    using Scene::Scene;

    [[nodiscard]] bool init() override;
    void clean() override;

private:
    bool setupDataModel();

    Rml::ElementDocument* doc_          = nullptr;
    Rml::DataModelHandle  model_handle_;
    SettingsData          settings_;
};

} // namespace learn::rmlui
