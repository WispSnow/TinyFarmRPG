#pragma once
#include "visual_test_case.h"
#include "engine/scene/scene.h"
#include <memory>
#include <vector>

namespace tools::visual {

class VisualTestSuiteScene final : public engine::scene::Scene {
private:
    std::vector<std::unique_ptr<VisualTestCase>> test_cases_;
    int active_test_index_{-1};
    float last_delta_time_{0.0f};
    float camera_move_speed_{400.0f};
    float camera_rotation_speed_{1.0f};
    float camera_zoom_speed_{1.0f};

public:
    VisualTestSuiteScene(std::string_view name, engine::core::Context& context);

    bool init() override;
    void update(float delta_time) override;
    void render() override;
    void clean() override;

private:
    void buildTestCases();
    void setActiveTest(int index);
    void handleInput(float delta_time);
    void drawOverlay(float delta_time, VisualTestCase* active_test);
    [[nodiscard]] VisualTestCase* getActiveTest() noexcept;
};

} // namespace tools::visual

