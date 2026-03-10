#pragma once

#include "engine/scene/scene.h"

namespace Rml {
class ElementDocument;
} // namespace Rml

namespace learn::rmlui {

/// L09 精灵表与装饰器演示场景（纯展示，不需要数据绑定）
class SpritesheetScene final : public engine::scene::Scene {
public:
    using Scene::Scene;

    [[nodiscard]] bool init() override;
    void clean() override;

private:
    Rml::ElementDocument* doc_ = nullptr;
};

} // namespace learn::rmlui
