#pragma once

namespace engine::core {
    class Context;
}

namespace engine::ui {
    class UIInteractive;
}

namespace engine::ui::state {

/**
 * @brief 可交互UI元素在特定状态下的行为接口。
 *
 * 该接口定义了所有具体UI状态必须实现的通用操作，
 * 例如处理输入事件、更新状态逻辑以及确定视觉表现。
 */
class UIState {
    friend class engine::ui::UIInteractive;
protected:
    engine::ui::UIInteractive* owner_ = nullptr;    ///< @brief 指向父节点

public:
    /**
     * @brief 构造函数传入父节点指针
     */
    UIState(engine::ui::UIInteractive* owner) : owner_(owner) {}
    virtual ~UIState() = default;

    // 删除拷贝和移动构造函数/赋值运算符
    UIState(const UIState&) = delete;
    UIState& operator=(const UIState&) = delete;
    UIState(UIState&&) = delete;
    UIState& operator=(UIState&&) = delete;

    /**
     * @brief 查询当前是否处于悬停状态
     * @note 默认返回false，由具体状态重写
     */
    virtual bool isHovered() const { return false; }
    
    /**
     * @brief 查询当前是否处于按下状态
     * @note 默认返回false，由具体状态重写
     */
    virtual bool isPressed() const { return false; }

protected:
    // --- 核心方法 --- 
    virtual void enter() = 0;
    virtual void update(float, engine::core::Context&) {}
    
    virtual void onMouseEnter() {}
    virtual void onMouseExit() {}
    virtual void onMousePressed() {}
    virtual void onMouseReleased(bool /*is_inside*/) {}
};

} // namespace engine::ui::state