#pragma once
#include "ui_state.h"

namespace engine::ui::state {

/**
 * @brief 正常状态
 *
 * 正常状态是UI元素的默认状态。
 */
class UINormalState final: public UIState {
    friend class engine::ui::UIInteractive;
public:
    explicit UINormalState(engine::ui::UIInteractive* owner) : UIState(owner) {}
    ~UINormalState() override = default;

    /* 默认实现已经返回false，不需要重写 */

private:
    void enter() override;
    void update(float, engine::core::Context&) override {}

    // 正常状态只需要考虑鼠标进入事件
    void onMouseEnter() override;
};

} // namespace engine::ui::state