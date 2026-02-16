#pragma once

#include <string_view>

namespace engine::debug {

/**
 * @brief 抽象调试面板接口，用于统一管理各模块的 ImGui 调试窗口。
 *
 * DebugUIManager 通过该接口渲染面板，实现可见性生命周期回调以及窗口绘制。
 */
class DebugPanel {
public:
    virtual ~DebugPanel() = default;

    /**
     * @brief 返回面板在调试控制面板中的显示名称。
     */
    virtual std::string_view name() const = 0;

    /**
     * @brief 渲染调试窗口。
     * @param is_open 传入当前可见状态，面板可以修改该值以便在窗口关闭按钮被点击时通知管理器。
     */
    virtual void draw(bool& is_open) = 0;

    /**
     * @brief 当面板第一次显示或重新显示时调用。默认不做任何处理。
     */
    virtual void onShow() {}

    /**
     * @brief 当面板被隐藏时调用。默认不做任何处理。
     */
    virtual void onHide() {}
};

} // namespace engine::debug

