#pragma once
#include "ui_state.h"

namespace engine::ui::state {

/**
 * @brief 按下状态
 *
 * 当鼠标按下UI元素时，会切换到该状态。
 */
class UIPressedState final: public UIState {
    friend class engine::ui::UIInteractive;
public:
    explicit UIPressedState(engine::ui::UIInteractive* owner) : UIState(owner) {}

    // 重写状态查询方法
    bool isHovered() const override { return true; }  // 按下状态也算悬停
    bool isPressed() const override { return true; }

private:
    void enter() override;
    // 按下状态只需要考虑鼠标释放事件
    void onMouseReleased(bool is_inside) override;
};

} // namespace engine::ui::state