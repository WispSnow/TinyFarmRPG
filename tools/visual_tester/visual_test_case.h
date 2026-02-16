#pragma once
#include <string>
#include <string_view>

namespace engine::core {
class Context;
}

namespace tools::visual {

class VisualTestCase {
protected:
    std::string name_;
    std::string category_;
    std::string purpose_;
    std::string steps_;
    std::string expected_;
    std::string common_issues_;

public:
    VisualTestCase(std::string name,
                   std::string category = {},
                   std::string purpose = {},
                   std::string steps = {},
                   std::string expected = {},
                   std::string common_issues = {})
        : name_(std::move(name)),
          category_(std::move(category)),
          purpose_(std::move(purpose)),
          steps_(std::move(steps)),
          expected_(std::move(expected)),
          common_issues_(std::move(common_issues)) {}
    virtual ~VisualTestCase() = default;

    [[nodiscard]] std::string_view getName() const noexcept { return name_; }
    [[nodiscard]] std::string_view getCategory() const noexcept { return category_; }
    [[nodiscard]] std::string_view getPurpose() const noexcept { return purpose_; }
    [[nodiscard]] std::string_view getSteps() const noexcept { return steps_; }
    [[nodiscard]] std::string_view getExpected() const noexcept { return expected_; }
    [[nodiscard]] std::string_view getCommonIssues() const noexcept { return common_issues_; }

    virtual void onEnter(engine::core::Context& /*context*/) {}
    virtual void onExit(engine::core::Context& /*context*/) {}
    virtual void onUpdate(float /*delta_time*/, engine::core::Context& /*context*/) {}
    virtual void onRender(engine::core::Context& context) = 0;
    virtual void onImGui(engine::core::Context& /*context*/) {}
};

} // namespace tools::visual
