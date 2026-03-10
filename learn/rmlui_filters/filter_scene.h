#pragma once

#include "engine/scene/scene.h"

#include <RmlUi/Core/DataModelHandle.h>

#include <string>

namespace Rml {
class ElementDocument;
} // namespace Rml

namespace learn::rmlui {

/// L11 滤镜、阴影与视觉特效 演示场景
class FilterScene final : public engine::scene::Scene {
public:
    using Scene::Scene;

    [[nodiscard]] bool init() override;
    void clean() override;

private:
    void setupDataModel();

    Rml::ElementDocument* doc_ = nullptr;
    Rml::DataModelHandle  model_handle_{};

    // --- 数据绑定 ---
    bool  poisoned_ = false;
    std::string status_text_ = "Ready";
};

} // namespace learn::rmlui
