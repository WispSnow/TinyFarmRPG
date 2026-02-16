#pragma once
#include "engine/utils/math.h"
#include <memory>
#include <vector>
#include <entt/entity/entity.hpp>

namespace engine::core {
    class Context;
}

namespace engine::ui {

class UIInteractive;

struct Thickness {
    float left{0.0f};
    float top{0.0f};
    float right{0.0f};
    float bottom{0.0f};

    [[nodiscard]] glm::vec2 horizontal() const { return {left, right}; }
    [[nodiscard]] glm::vec2 vertical() const { return {top, bottom}; }
    [[nodiscard]] float width() const { return left + right; }
    [[nodiscard]] float height() const { return top + bottom; }
};

/**
 * @brief 所有UI元素的基类
 *
 * 定义了位置、大小、可见性、状态等通用属性。
 * 管理子元素的层次结构。
 * 提供事件处理、更新和渲染的虚方法。
 */
class UIElement {
protected:
    glm::vec2 position_;                                    ///< @brief 相对于父元素的局部位置
    glm::vec2 size_;                                        ///< @brief 元素大小
    bool visible_ = true;                                   ///< @brief 元素当前是否可见
    bool need_remove_ = false;                              ///< @brief 是否需要移除(延迟删除)
    int order_index_ = 0;                                   ///< @brief 一个用于排序的索引
    entt::id_type id_ = entt::null;                         ///< @brief 可用于标记或查找的ID    

    UIElement* parent_ = nullptr;                           ///< @brief 指向父节点的非拥有指针
    std::vector<std::unique_ptr<UIElement>> children_;      ///< @brief 子元素列表(容器)

    glm::vec2 anchor_min_{0.0f, 0.0f};                      ///< @brief 锚点最小值（相对父内容区域）
    glm::vec2 anchor_max_{0.0f, 0.0f};                      ///< @brief 锚点最大值（用于拉伸）
    glm::vec2 pivot_{0.0f, 0.0f};                           ///< @brief 枢轴（相对自身 [0,1]）
    Thickness padding_{};                                   ///< @brief 内边距，影响子内容布局
    Thickness margin_{};                                    ///< @brief 外边距，影响自身定位

    mutable bool layout_dirty_{true};                       ///< @brief 布局是否需要重新计算
    mutable glm::vec2 layout_position_{0.0f, 0.0f};         ///< @brief 计算后的屏幕坐标
    mutable glm::vec2 layout_size_{0.0f, 0.0f};             ///< @brief 计算后的尺寸

public:
    /**
     * @brief 构造UIElement
     * @param position 初始局部位置
     * @param size 初始大小
     */
    explicit UIElement(glm::vec2 position = {0.0f, 0.0f}, glm::vec2 size = {0.0f, 0.0f});

    /**
     * @brief 虚析构函数，确保派生类正确清理
     */
    virtual ~UIElement() = default;

    // --- 核心虚循环方法 --- (没有使用init和clean，注意构造函数和析构函数的使用)
    virtual void update(float delta_time, engine::core::Context& context);
    virtual void render(engine::core::Context& context);

    // --- 层次结构管理 ---
    /// @brief 添加子元素, 可指定子元素的排序键(默认为-1，不设置排序键)
    void addChild(std::unique_ptr<UIElement> child, int order_index = -1);                
    std::unique_ptr<UIElement> removeChild(UIElement* child_ptr);   ///< @brief 将指定子元素从列表中移除，并返回其智能指针
    std::unique_ptr<UIElement> removeChildById(entt::id_type id);   ///< @brief 根据ID移除子元素，并返回其智能指针
    void removeAllChildren();                                       ///< @brief 移除所有子元素

    // --- Getters and Setters ---
    glm::vec2 getSize() const;                                      ///< @brief 获取元素最终大小
    const glm::vec2& getRequestedSize() const { return size_; }     ///< @brief 获取元素请求大小
    const glm::vec2& getPosition() const { return position_; }      ///< @brief 获取元素位置(相对于父节点)
    bool isVisible() const { return visible_; }                     ///< @brief 检查元素是否可见
    bool isNeedRemove() const { return need_remove_; }              ///< @brief 检查元素是否需要移除
    int getOrderIndex() const { return order_index_; }              ///< @brief 获取元素的排序索引
    UIElement* getParent() const { return parent_; }                ///< @brief 获取父元素
    const std::vector<std::unique_ptr<UIElement>>& getChildren() const { return children_; } ///< @brief 获取子元素列表
    UIElement* getChildById(entt::id_type id) const;                ///< @brief 根据ID获取子元素
    entt::id_type getId() const { return id_; }                     ///< @brief 获取自身的ID
    engine::utils::Rect getBounds() const;                          ///< @brief 获取(计算)元素的边界(屏幕坐标)
    glm::vec2 getScreenPosition() const;                            ///< @brief 获取(计算)元素在屏幕上位置
    glm::vec2 getLayoutSize() const;                                ///< @brief 获取计算后的尺寸
    glm::vec2 getLayoutPosition() const { return getScreenPosition(); } ///< @brief 获取布局位置
    engine::utils::Rect getContentBounds() const;                   ///< @brief 获取内容区域(考虑内边距)
    const Thickness& getPadding() const { return padding_; }        ///< @brief 获取内边距
    const Thickness& getMargin() const { return margin_; }          ///< @brief 获取外边距
    glm::vec2 getAnchorMin() const { return anchor_min_; }          ///< @brief 获取锚点最小值
    glm::vec2 getAnchorMax() const { return anchor_max_; }          ///< @brief 获取锚点最大值
    glm::vec2 getPivot() const { return pivot_; }                   ///< @brief 获取枢轴

    void setSize(glm::vec2 size) { setSizeInternal(std::move(size)); }  ///< @brief 设置元素大小
    void setVisible(bool visible) { visible_ = visible; }               ///< @brief 设置元素的可见性
    void setParent(UIElement* parent) { setParentInternal(parent); }    ///< @brief 设置父节点
    void setPosition(glm::vec2 position) { position_ = std::move(position); invalidateLayout(); }   ///< @brief 设置元素位置(相对于父节点)
    void setNeedRemove(bool need_remove) { need_remove_ = need_remove; }    ///< @brief 设置元素是否需要移除
    void setOrderIndex(int order_index);                                    ///< @brief 设置元素的排序索引
    void setId(entt::id_type id) { id_ = id; }                              ///< @brief 设置元素的ID
    void setAnchor(glm::vec2 anchor_min, glm::vec2 anchor_max);     ///< @brief 设置锚点
    void setPivot(glm::vec2 pivot);                                 ///< @brief 设置枢轴
    void setPadding(const Thickness& padding);                      ///< @brief 设置内边距
    void setMargin(const Thickness& margin);                        ///< @brief 设置外边距

    // --- 辅助方法 ---
    void sortChildrenByOrderIndex();                                ///< @brief 根据order_index_排序子元素
    bool isPointInside(const glm::vec2& point) const;               ///< @brief 检查给定点是否在元素的边界内

    UIInteractive* findInteractiveAt(const glm::vec2& point);
    const UIInteractive* findInteractiveAt(const glm::vec2& point) const;

    // --- 禁用拷贝和移动语义 ---
    UIElement(const UIElement&) = delete;
    UIElement& operator=(const UIElement&) = delete;
    UIElement(UIElement&&) = delete;
    UIElement& operator=(UIElement&&) = delete;

protected:
    virtual void renderSelf(engine::core::Context& context);
    virtual void onLayout() {} // 新增：布局回调，供子类实现自定义布局逻辑

    void invalidateLayout(bool propagate = true);
    void ensureLayout() const;
    void setParentInternal(UIElement* parent);
    void setSizeInternal(glm::vec2 size);
};

} // namespace engine::ui
