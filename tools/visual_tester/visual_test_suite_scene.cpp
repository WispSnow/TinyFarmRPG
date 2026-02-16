#include "visual_test_suite_scene.h"
#include "visual_test_cases.h"
#include "engine/core/context.h"
#include "engine/input/input_manager.h"
#include "engine/render/camera.h"
#include "engine/render/renderer.h"
#include <imgui.h>
#include <entt/core/hashed_string.hpp>
#include <glm/vec2.hpp>
#include <string>

using namespace entt::literals;

namespace tools::visual {

VisualTestSuiteScene::VisualTestSuiteScene(std::string_view name, engine::core::Context& context)
    : engine::scene::Scene(name, context) {}

bool VisualTestSuiteScene::init() {
    buildTestCases();
    if (!test_cases_.empty()) {
        setActiveTest(0);
    }
    return Scene::init();
}

void VisualTestSuiteScene::update(float delta_time) {
    last_delta_time_ = delta_time;
    handleInput(delta_time);

    if (auto* active = getActiveTest(); active != nullptr) {
        active->onUpdate(delta_time, context_);
    }

    Scene::update(delta_time);
}

void VisualTestSuiteScene::render() {
    auto* active = getActiveTest();
    auto& renderer = context_.getRenderer();
    auto& camera = context_.getCamera();
    renderer.beginFrame(camera);

    if (active != nullptr) {
        active->onRender(context_);
    }

    drawOverlay(last_delta_time_, active);
    Scene::render();
}

void VisualTestSuiteScene::clean() {
    setActiveTest(-1);
    Scene::clean();
}

void VisualTestSuiteScene::buildTestCases() {
    test_cases_.clear();
    test_cases_.reserve(9);
    test_cases_.push_back(std::make_unique<TileAutoTileYSortVisualTest>());
    test_cases_.push_back(std::make_unique<RenderPassCoverageVisualTest>());
    test_cases_.push_back(std::make_unique<CameraViewportClippingVisualTest>());
    test_cases_.push_back(std::make_unique<SpriteAndPrimitiveVisualTest>());
    test_cases_.push_back(std::make_unique<LightingVisualTest>());
    test_cases_.push_back(std::make_unique<EmissiveVisualTest>());
    test_cases_.push_back(std::make_unique<UiVisualTest>());
    test_cases_.push_back(std::make_unique<TextRenderingVisualTest>());
    test_cases_.push_back(std::make_unique<AudioVisualTest>());
}

void VisualTestSuiteScene::setActiveTest(int index) {
    if (index == active_test_index_) {
        return;
    }

    if (auto* current = getActiveTest(); current != nullptr) {
        current->onExit(context_);
    }

    if (index < 0 || index >= static_cast<int>(test_cases_.size())) {
        active_test_index_ = -1;
        return;
    }

    active_test_index_ = index;
    test_cases_[active_test_index_]->onEnter(context_);
}

VisualTestCase* VisualTestSuiteScene::getActiveTest() noexcept {
    if (active_test_index_ < 0 || active_test_index_ >= static_cast<int>(test_cases_.size())) {
        return nullptr;
    }
    return test_cases_[active_test_index_].get();
}

void VisualTestSuiteScene::handleInput(float delta_time) {
    auto& camera = context_.getCamera();
    const auto& input_manager = context_.getInputManager();

    if (input_manager.isActionDown("move_up"_hs)) {
        camera.translate(glm::vec2{0.0f, -camera_move_speed_} * delta_time);
    }
    if (input_manager.isActionDown("move_down"_hs)) {
        camera.translate(glm::vec2{0.0f, camera_move_speed_} * delta_time);
    }
    if (input_manager.isActionDown("move_left"_hs)) {
        camera.translate(glm::vec2{-camera_move_speed_, 0.0f} * delta_time);
    }
    if (input_manager.isActionDown("move_right"_hs)) {
        camera.translate(glm::vec2{camera_move_speed_, 0.0f} * delta_time);
    }
    if (input_manager.isActionDown("rotate_left"_hs)) {
        camera.rotate(-camera_rotation_speed_ * delta_time);
    }
    if (input_manager.isActionDown("rotate_right"_hs)) {
        camera.rotate(camera_rotation_speed_ * delta_time);
    }

    if (input_manager.isActionPressed("camera_reset_zoom"_hs)) {
        camera.setZoom(1.0f);
    }

    auto mouse_wheel_delta = input_manager.getMouseWheelDelta();
    camera.zoom(mouse_wheel_delta.y * camera_zoom_speed_ * delta_time);
}

void VisualTestSuiteScene::drawOverlay(float delta_time, VisualTestCase* active_test) {
    ImGui::Begin("Visual Tests");

    const float fps = delta_time > 0.0f ? 1.0f / delta_time : 0.0f;
    ImGui::Text("FPS: %.1f", fps);
    ImGui::Text("Delta: %.3f ms", delta_time * 1000.0f);
    ImGui::Separator();

    ImGui::TextUnformatted("Controls:");
    ImGui::BulletText("Move Camera: WASD / Arrow Keys");
    ImGui::BulletText("Zoom: Mouse Wheel");
    ImGui::BulletText("Rotate: Q / E");
    ImGui::BulletText("Reset Zoom: Mouse Middle");
    {
        auto& camera = context_.getCamera();
        ImGui::Text("Camera: pos(%.1f, %.1f) zoom=%.2f rot=%.2f rad",
                    camera.getPosition().x, camera.getPosition().y,
                    camera.getZoom(), camera.getRotation());
        if (ImGui::Button("Reset Camera")) {
            camera.setPosition({0.0f, 0.0f});
            camera.setRotation(0.0f);
            camera.setZoom(1.0f);
        }
    }
    ImGui::Separator();

    for (std::size_t i = 0; i < test_cases_.size(); ++i) {
        const bool selected = static_cast<int>(i) == active_test_index_;
        std::string label{};
        const std::string_view category = test_cases_[i]->getCategory();
        if (!category.empty()) {
            label.reserve(category.size() + test_cases_[i]->getName().size() + 4);
            label.append("[");
            label.append(category);
            label.append("] ");
        }
        label.append(test_cases_[i]->getName());
        if (ImGui::Selectable(label.c_str(), selected)) {
            setActiveTest(static_cast<int>(i));
        }
    }

    if (!test_cases_.empty()) {
        if (ImGui::Button("上一测试")) {
            auto prev_index = active_test_index_ - 1;
            if (prev_index < 0) {
                prev_index = static_cast<int>(test_cases_.size()) - 1;
            }
            setActiveTest(prev_index);
        }
        ImGui::SameLine();
        if (ImGui::Button("下一测试")) {
            auto next_index = active_test_index_ + 1;
            if (next_index >= static_cast<int>(test_cases_.size())) {
                next_index = 0;
            }
            setActiveTest(next_index);
        }
    }

    if (active_test != nullptr) {
        ImGui::Separator();

        std::string active_label{};
        if (!active_test->getCategory().empty()) {
            active_label.reserve(active_test->getCategory().size() + active_test->getName().size() + 4);
            active_label.append("[");
            active_label.append(active_test->getCategory());
            active_label.append("] ");
        }
        active_label.append(active_test->getName());
        ImGui::Text("当前测试: %s", active_label.c_str());

        auto drawBlock = [](const char* title, std::string_view content) {
            if (content.empty()) {
                return;
            }
            ImGui::TextUnformatted(title);
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextUnformatted(content.data());
            ImGui::PopTextWrapPos();
        };

        drawBlock("目的:", active_test->getPurpose());
        drawBlock("操作:", active_test->getSteps());
        drawBlock("预期:", active_test->getExpected());
        drawBlock("常见问题:", active_test->getCommonIssues());

        active_test->onImGui(context_);
    }

    ImGui::End();
}

} // namespace tools::visual
