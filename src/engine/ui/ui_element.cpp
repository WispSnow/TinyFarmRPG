#include "ui_element.h"
#include "ui_interactive.h"
#include "engine/core/context.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <spdlog/spdlog.h>

namespace engine::ui {
namespace {

constexpr float kLayoutEpsilon = 0.001F;
std::uint64_t g_layout_recompute_counter = 0;

[[nodiscard]] bool sameLayoutOverride(const std::optional<glm::vec2>& lhs,
                                      const std::optional<glm::vec2>& rhs) {
    if (lhs.has_value() != rhs.has_value()) {
        return false;
    }
    if (!lhs.has_value()) {
        return true;
    }
    return std::fabs(lhs->x - rhs->x) <= kLayoutEpsilon &&
	   std::fabs(lhs->y - rhs->y) <= kLayoutEpsilon;
}

[[nodiscard]] bool sameVec2(const glm::vec2& lhs, const glm::vec2& rhs) {
    return std::fabs(lhs.x - rhs.x) <= kLayoutEpsilon &&
           std::fabs(lhs.y - rhs.y) <= kLayoutEpsilon;
}

[[nodiscard]] bool sameThickness(const Thickness& lhs, const Thickness& rhs) {
    return std::fabs(lhs.left - rhs.left) <= kLayoutEpsilon &&
           std::fabs(lhs.top - rhs.top) <= kLayoutEpsilon &&
           std::fabs(lhs.right - rhs.right) <= kLayoutEpsilon &&
           std::fabs(lhs.bottom - rhs.bottom) <= kLayoutEpsilon;
}

} // namespace

UIElement::UIElement(glm::vec2 position, glm::vec2 size)
    : position_(std::move(position)), size_(std::move(size)) {
    layout_position_ = position_;
    layout_size_ = size_;
}   

void UIElement::update(float delta_time, engine::core::Context& context) {
    ensureLayout();

    // 如果元素不可见，直接返回
    if (!visible_) return; 

    // 遍历所有子节点，并删除标记了移除的元素
    for (auto it = children_.begin(); it != children_.end();) {
        if (*it && !(*it)->isNeedRemove()) {
            (*it)->update(delta_time, context);
            ++it;
        } else {
            it = children_.erase(it);
        }
    }
}

void UIElement::render(engine::core::Context& context) {
    ensureLayout();

    if (!visible_) return;

    renderSelf(context);

    // 渲染子元素
    for (const auto& child : children_) {
        if (child) child->render(context);
    }
}

void UIElement::addChild(std::unique_ptr<UIElement> child, int order_index) {
    if (child) {
        child->setParent(this); // 设置父指针
        if (order_index >= 0) {
            child->setOrderIndex(order_index);
        }
        children_.push_back(std::move(child));
        sortChildrenByOrderIndex();
        invalidateLayout();
    }
}

std::unique_ptr<UIElement> UIElement::removeChild(UIElement* child_ptr) {
    // 使用 std::remove_if 和 lambda 表达式自定义比较的方式移除
    auto it = std::find_if(children_.begin(), children_.end(),
                           [child_ptr](const std::unique_ptr<UIElement>& p) { 
                                return p.get() == child_ptr; 
                           });

    if (it != children_.end()) {
        std::unique_ptr<UIElement> removed_child = std::move(*it);
        children_.erase(it);
        removed_child->setParent(nullptr);      // 清除父指针
        invalidateLayout();
        return removed_child;                   // 返回被移除的子元素（可以挂载到别处）
    }
    return nullptr; // 未找到子元素
}

std::unique_ptr<UIElement> UIElement::removeChildById(entt::id_type id) {
    auto it = std::find_if(children_.begin(), children_.end(),
                           [id](const std::unique_ptr<UIElement>& p) { 
                                return p->getId() == id; 
                           });
    if (it != children_.end()) {
        std::unique_ptr<UIElement> removed_child = std::move(*it);
        children_.erase(it);
        removed_child->setParent(nullptr);      // 清除父指针
        invalidateLayout();
        return removed_child;                   // 返回被移除的子元素（可以挂载到别处）
    }
    return nullptr; // 未找到子元素
}

void UIElement::removeAllChildren() {
    for (auto& child : children_) {
        child->setParent(nullptr); // 清除父指针
    }
    children_.clear();
    invalidateLayout();
}

UIElement* UIElement::getChildById(entt::id_type id) const {
    auto it = std::find_if(children_.begin(), children_.end(),
                           [id](const std::unique_ptr<UIElement>& p) { 
                                return p->getId() == id; 
                           });
    if (it != children_.end()) {
        return it->get();
    }
    spdlog::trace("UIElement::getChildById: 未找到子元素: {}", id);
    return nullptr; // 未找到子元素
}

glm::vec2 UIElement::getScreenPosition() const {
    ensureLayout();
    return layout_position_; // layout_position_ 是元素的左上角坐标
}

void UIElement::sortChildrenByOrderIndex() {
    // 使用stable_sort避免破坏原来相等元素的顺序
    std::stable_sort(children_.begin(), children_.end(), [](const std::unique_ptr<UIElement>& a, const std::unique_ptr<UIElement>& b) {
        return a->getOrderIndex() < b->getOrderIndex();
    });
}

void UIElement::setOrderIndex(int order_index) {
    if (order_index_ == order_index) {
        return;
    }
    order_index_ = order_index;
    if (parent_) {
        parent_->sortChildrenByOrderIndex();
        parent_->invalidateLayout();
    }
}

engine::utils::Rect UIElement::getBounds() const {
    ensureLayout();
    return engine::utils::Rect(layout_position_, layout_size_);
}

bool UIElement::isPointInside(const glm::vec2& point) const {
    auto bounds = getBounds();
    return (point.x >= bounds.pos.x && point.x < (bounds.pos.x + bounds.size.x) &&
            point.y >= bounds.pos.y && point.y < (bounds.pos.y + bounds.size.y));
}

glm::vec2 UIElement::getSize() const {
    return getLayoutSize();
}

glm::vec2 UIElement::getLayoutSize() const {
    ensureLayout();
    return layout_size_;
}

engine::utils::Rect UIElement::getContentBounds() const {
    ensureLayout();
    glm::vec2 content_pos = layout_position_ + glm::vec2{padding_.left, padding_.top};
    glm::vec2 content_size = glm::max(layout_size_ - glm::vec2{padding_.width(), padding_.height()}, glm::vec2{0.0f});
    return {content_pos, content_size};
}

void UIElement::setAnchor(glm::vec2 anchor_min, glm::vec2 anchor_max) {
    if (sameVec2(anchor_min_, anchor_min) && sameVec2(anchor_max_, anchor_max)) {
        return;
    }
    anchor_min_ = anchor_min;
    anchor_max_ = anchor_max;
    invalidateLayout();
}

void UIElement::setPivot(glm::vec2 pivot) {
    pivot_ = pivot;
    // pivot 会影响自身的屏幕位置，进而影响子节点的布局基准（parent content bounds）。
    invalidateLayout(true);
}

void UIElement::setPadding(const Thickness& padding) {
    if (sameThickness(padding_, padding)) {
        return;
    }
    padding_ = padding;
    invalidateLayout();
}

void UIElement::setMargin(const Thickness& margin) {
    if (sameThickness(margin_, margin)) {
        return;
    }
    margin_ = margin;
    invalidateLayout();
}

void UIElement::setLayoutOverrideSize(std::optional<glm::vec2> size) {
    if (size.has_value()) {
        size->x = std::max(0.0F, size->x);
        size->y = std::max(0.0F, size->y);
    }

    if (sameLayoutOverride(layout_override_size_, size)) {
        return;
    }

    const bool had_override = layout_override_size_.has_value();
    const glm::vec2 previous_override =
            had_override ? *layout_override_size_ : glm::vec2{0.0F, 0.0F};

    layout_override_size_ = size;

    if (layout_override_size_.has_value()) {
        spdlog::trace(
                "UIElement::setLayoutOverrideSize id={} ptr={} old_override={}({}, {}) new_override=({}, {})",
                id_, static_cast<const void*>(this), had_override, previous_override.x,
                previous_override.y, layout_override_size_->x, layout_override_size_->y);
    } else {
        spdlog::trace(
                "UIElement::setLayoutOverrideSize id={} ptr={} old_override={}({}, {}) new_override=cleared",
                id_, static_cast<const void*>(this), had_override, previous_override.x,
                previous_override.y);
    }

    invalidateLayout();
}

void UIElement::setWorldAnchor(glm::vec2 world_pos, glm::vec2 screen_offset) {
    const bool mode_changed = positioning_mode_ != PositioningMode::WorldAnchor;
    const bool world_pos_changed = !sameVec2(world_anchor_, world_pos);
    const bool offset_changed = !sameVec2(world_anchor_offset_, screen_offset);
    const bool anchor_changed = world_pos_changed || offset_changed;
    if (!mode_changed && !anchor_changed) {
        return;
    }

    if (mode_changed) {
        world_anchor_previous_ = world_pos;
        world_anchor_has_previous_ = true;
    } else if (world_pos_changed) {
        world_anchor_previous_ = world_anchor_;
        world_anchor_has_previous_ = true;
    }

    positioning_mode_ = PositioningMode::WorldAnchor;
    world_anchor_ = world_pos;
    world_anchor_offset_ = screen_offset;
    invalidateLayout();
}

void UIElement::clearWorldAnchor() {
    const glm::vec2 zero{0.0F, 0.0F};
    const bool mode_changed = positioning_mode_ != PositioningMode::Screen;
    const bool anchor_changed = !sameVec2(world_anchor_, zero) ||
                                !sameVec2(world_anchor_offset_, zero);
    if (!mode_changed && !anchor_changed) {
        return;
    }

    positioning_mode_ = PositioningMode::Screen;
    world_anchor_previous_ = zero;
    world_anchor_ = zero;
    world_anchor_offset_ = zero;
    world_anchor_has_previous_ = false;
    invalidateLayout();
}

const UIInteractive* UIElement::findInteractiveAt(const glm::vec2& point) const {
    if (!visible_) {
        return nullptr;
    }

    ensureLayout();

    // 先从子元素中查找，找到就返回，确保最上层的可交互元素被找到
    for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
        if (*it) {
            if (auto* result = (*it)->findInteractiveAt(point); result) {
                return result;
            }
        }
    }

    // 只有UIInteractive才能被交互，其它类型跳过检查
    const UIInteractive* interactive = asInteractive();
    if (interactive && interactive->isInteractive() && isPointInside(point)) {
        return interactive;
    }
    return nullptr;
}

UIInteractive* UIElement::findInteractiveAt(const glm::vec2& point) {
    // 非 const 版本调用 const 版本，返回时去掉 const
    return const_cast<UIInteractive*>(std::as_const(*this).findInteractiveAt(point));
}

void UIElement::renderSelf(engine::core::Context& /*context*/) {
    // 默认不渲染自身，由子类决定
}

void UIElement::invalidateLayout(bool propagate) {
    const bool was_dirty = layout_dirty_;
    layout_dirty_ = true;

    if (!propagate || was_dirty) {
        return;
    }

    for (auto& child : children_) {
        if (child) {
            child->invalidateLayout(true);
        }
    }
}

void UIElement::ensureLayout() const {
    if (!layout_dirty_) {
        return;
    }
    ++g_layout_recompute_counter;

    if (!parent_) {
        layout_size_ = layout_override_size_.value_or(size_);
        layout_position_ = position_;
        const bool has_override = layout_override_size_.has_value();
        const glm::vec2 override_size =
                has_override ? *layout_override_size_ : glm::vec2{0.0F, 0.0F};
        spdlog::trace(
                "UIElement::ensureLayout(root) id={} ptr={} requested=({}, {}) layout=({}, {}) override={}({}, {}) position=({}, {})",
                id_, static_cast<const void*>(this), size_.x, size_.y, layout_size_.x,
                layout_size_.y, has_override, override_size.x, override_size.y,
                layout_position_.x, layout_position_.y);
        layout_dirty_ = false;
        return;
    }

    if (positioning_mode_ == PositioningMode::WorldAnchor) {
        if (parent_->getParent() != nullptr) {
            spdlog::warn(
                    "UIElement::ensureLayout: WorldAnchor mode on non-root-child element (id={}), falling back to Screen mode.",
                    id_);
            // Fall through to Screen layout logic below (constraint C1).
        } else {
            layout_size_ = layout_override_size_.value_or(size_);
            layout_dirty_ = false;
            const_cast<UIElement*>(this)->onLayout();
            return;
        }
    }

    // 获取父元素的内容(可用)区域
    auto parent_content = parent_->getContentBounds();
    glm::vec2 parent_origin = parent_content.pos;
    glm::vec2 parent_size = parent_content.size;

    // 判断是否能被拉伸(拉伸意味着可以被父元素的内容区域拉伸)
    bool stretched = std::fabs(anchor_min_.x - anchor_max_.x) > std::numeric_limits<float>::epsilon() ||
                     std::fabs(anchor_min_.y - anchor_max_.y) > std::numeric_limits<float>::epsilon();

    // 计算锚点位置（相对于父元素内容区域的归一化坐标转换为绝对坐标）
    glm::vec2 anchor_min_pos = parent_origin + parent_size * anchor_min_;
    glm::vec2 anchor_max_pos = parent_origin + parent_size * anchor_max_;
    // 计算可用区域大小
    glm::vec2 available_size = anchor_max_pos - anchor_min_pos;

    // 计算最终大小
    glm::vec2 final_size = size_;
    if (layout_override_size_.has_value()) {
        final_size = *layout_override_size_;
    } else if (stretched) {
        // 可拉伸：使用可用区域大小，减去margin
        final_size = available_size;
        final_size.x = std::max(0.0f, final_size.x - margin_.width());
        final_size.y = std::max(0.0f, final_size.y - margin_.height());
    }
    // 不可拉伸：使用请求的大小（size_），不考虑margin（margin只影响位置）
    layout_size_ = final_size;

    // 计算锚点参考位置
    // position_ 是相对于 anchor_min_pos 的偏移量
    glm::vec2 anchor_reference = anchor_min_pos + position_;

    // 计算布局位置（屏幕坐标）
    // 1. 从锚点参考位置开始
    // 2. 加上margin的左/上边距（margin影响元素位置）
    // 3. 减去根据pivot计算的偏移（pivot是相对于元素自身的归一化坐标）
    glm::vec2 top_left = anchor_reference + glm::vec2{margin_.left, margin_.top} - layout_size_ * pivot_;
    layout_position_ = top_left;

    // 设置布局脏标志为false，以防onLayout中再次查询可能导致递归（虽然onLayout通常是修改子元素）
    layout_dirty_ = false;

    // 调用子类布局回调
    const_cast<UIElement*>(this)->onLayout();

    const bool has_override = layout_override_size_.has_value();
    const glm::vec2 override_size =
            has_override ? *layout_override_size_ : glm::vec2{0.0F, 0.0F};
    spdlog::trace(
            "UIElement::ensureLayout id={} ptr={} requested=({}, {}) layout=({}, {}) override={}({}, {}) position=({}, {}) stretched={}",
            id_, static_cast<const void*>(this), size_.x, size_.y, layout_size_.x,
            layout_size_.y, has_override, override_size.x, override_size.y,
            layout_position_.x, layout_position_.y, stretched);
}

void UIElement::setParentInternal(UIElement* parent) {
    if (layout_override_size_.has_value()) {
        spdlog::trace(
                "UIElement::setParentInternal id={} ptr={} parent_change clears override=({}, {})",
                id_, static_cast<const void*>(this), layout_override_size_->x,
                layout_override_size_->y);
    }
    parent_ = parent;
    layout_override_size_.reset();
    invalidateLayout();
}

void UIElement::setSizeInternal(glm::vec2 size) {
    if (sameVec2(size_, size)) {
        return;
    }
    size_ = std::move(size);
    invalidateLayout();
}

void UIElement::setSize(glm::vec2 size) {
    setSizeInternal(std::move(size));
}

void UIElement::setPosition(glm::vec2 position) {
    if (sameVec2(position_, position)) {
        return;
    }
    position_ = std::move(position);
    invalidateLayout();
}

glm::vec2 UIElement::sampleWorldAnchor(float interpolation_alpha) const {
    if (!world_anchor_has_previous_) {
        return world_anchor_;
    }
    const float alpha = std::clamp(interpolation_alpha, 0.0F, 1.0F);
    return world_anchor_previous_ + (world_anchor_ - world_anchor_previous_) * alpha;
}

void UIElement::applyWorldAnchorPosition(glm::vec2 screen_pos) {
    const glm::vec2 final_size = layout_override_size_.value_or(size_);
    const glm::vec2 new_position = screen_pos - final_size * pivot_;
    if (!layout_dirty_ && sameVec2(layout_position_, new_position) &&
        sameVec2(layout_size_, final_size)) {
        return;
    }

    layout_position_ = new_position;
    layout_size_ = final_size;
    layout_dirty_ = false;

    for (auto& child : children_) {
        if (child) {
            child->invalidateLayout(true);
        }
    }
}

void UIElement::resetLayoutRecomputeCounter() {
    g_layout_recompute_counter = 0;
}

std::uint64_t UIElement::consumeLayoutRecomputeCounter() {
    const std::uint64_t current = g_layout_recompute_counter;
    g_layout_recompute_counter = 0;
    return current;
}

} // namespace engine::ui 
