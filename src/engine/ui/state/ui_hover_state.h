#pragma once
#include "ui_state.h"

namespace engine::ui::state {

/**
 * @brief 悬停状态
 *
 * 当鼠标悬停在UI元素上时，会切换到该状态。
 */
class UIHoverState final: public UIState {
    friend class engine::ui::UIInteractive;
public:
    explicit UIHoverState(engine::ui::UIInteractive* owner) : UIState(owner) {}

    // 重写状态查询方法
    bool isHovered() const override { return true; }

private:
    void enter() override;
    void update(float, engine::core::Context&) override {}
    
    // 悬停状态只需要考虑鼠标离开、按下事件
    void onMouseExit() override;
    void onMousePressed() override;
};

} // namespace engine::ui::state