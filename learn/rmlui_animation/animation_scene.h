#pragma once

#include "engine/scene/scene.h"

#include <RmlUi/Core/DataModelHandle.h>

#include <string>
#include <vector>

namespace Rml {
class Element;
class ElementDocument;
} // namespace Rml

namespace learn::rmlui {

/// L10 动画、变换与过渡 演示场景
class AnimationScene final : public engine::scene::Scene {
public:
    using Scene::Scene;

    [[nodiscard]] bool init() override;
    void clean() override;
    void update(float dt) override;

private:
    void setupDataModel();

    Rml::ElementDocument* doc_ = nullptr;
    Rml::DataModelHandle  model_handle_{};

    // --- 数据绑定 ---
    int   damage_value_ = 42;
    bool  window_visible_ = false;
    std::string status_text_ = "Ready";

    // 伤害数字：每个弹出元素独立计时，动画结束后自动销毁
    struct DmgPopup {
        Rml::Element* el;
        float age;
    };
    std::vector<DmgPopup> dmg_popups_;

    // 窗口关闭动画计时
    float close_timer_ = 0.0f;
};

} // namespace learn::rmlui
