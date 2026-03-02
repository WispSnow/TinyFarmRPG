#pragma once

#include "engine/scene/scene.h"

namespace learn::rmlui {

class RmlUiBasicsScene final : public engine::scene::Scene {
public:
    using Scene::Scene;

    [[nodiscard]] bool init() override;
    void clean() override;
};

} // namespace learn::rmlui
