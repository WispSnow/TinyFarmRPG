# UI 布局重构 Backlog 审阅

> 审阅对象：`plans/2026-02-17-ui-layout-refactor-backlog.md`
> 审阅日期：2026-02-18

---

## 1. 问题诊断验证

### 1.1 UIGridLayout `setSize` 副作用 — 已确认

`ui_grid_layout.cpp:44-48`:
```cpp
if (cell_size_.x > 0.0f && cell_size_.y > 0.0f) {
    size = cell_size_;
    if (glm::distance(child->getRequestedSize(), cell_size_) > 0.001f) {
        child->setSize(cell_size_);  // ← 副作用：修改子节点 size_，触发 invalidateLayout
    }
}
```

**问题确认**：`setSize()` → `setSizeInternal()` → `invalidateLayout()`，在 `onLayout()` 内部触发子节点布局脏化。当前代码用 `distance > 0.001f` 避免了"已是目标尺寸"时的重复写入，所以实际场景中不会无限循环。但语义上存在问题——`onLayout()` 应该只做定位，不该永久修改子节点的 requested size。

**风险程度**：中。当前被 0.001f 阈值兜住，但如果子节点有自己的 resize 逻辑（如 auto_resize 的 StackLayout 嵌套在 Grid 中），会产生竞争。

### 1.2 UIStackLayout 使用 `getRequestedSize()` 计算主轴 — 已确认

`ui_stack_layout.cpp:50,89`:
```cpp
glm::vec2 child_size = child->getRequestedSize();  // 第一遍：计算总长
glm::vec2 child_req_size = child->getRequestedSize();  // 第二遍：定位
```

**分析**：对于非 stretch 子节点，`getRequestedSize() == getLayoutSize()`，没有问题。但如果子节点设置了 anchor stretch（`anchor_min != anchor_max`），`getLayoutSize()` 会根据父元素可用空间计算出不同值，而 `getRequestedSize()` 仍返回原始 `size_`。

**实际影响**：当前游戏代码中 StackLayout 的子节点（UIButton 等）均未使用 stretch，所以暂无可见 bug。但文档注释已提到"如果子元素设置了对应的轴向拉伸(Anchor)，布局会尝试调整子元素大小（暂未完全实现）"——这正是待修复的语义不一致。

### 1.3 布局测试缺失 — 已确认

现有 `tests/engine/ui/` 目录中无任何 `layout` 相关测试文件。布局行为仅靠手工回归验证。

---

## 2. 方案评审

### 2.1 总体策略

"先锁语义再改实现"的策略正确。布局系统不像交互状态那样有明确的 bug 驱动（交互重构由 UIR-010 点击丢失触发），布局问题更多是语义模糊，先建立文档和测试基线再动实现是稳妥的。

### 2.2 逐项评审

#### UIL-000 建立布局行为清单 — OK

与交互重构 UIR-000 对齐。无异议。

#### UIL-001 补首批布局自动化测试 — OK，有一个建议

**建议**：布局测试不需要 SDL/OpenGL 环境。`UIElement`/`UILayout` 的布局计算是纯数学，可以直接构造对象测试。与交互重构的运行时测试（需要 SDL）不同，布局测试应该设计为**无依赖纯单元测试**，不需要 `GTEST_SKIP` 逻辑。这是布局测试的天然优势，建议在计划中明确提出。

#### UIL-002 冻结布局语义文档 — OK

明确 `requestedSize` vs `layoutSize` 的契约是后续工作的前提。

#### UIL-010 消除 Grid 尺寸副作用 — OK，方案需要细化

**当前问题**：`child->setSize(cell_size_)` 永久修改了子节点的 `size_`。

**建议方案**：`onLayout()` 不应调用 `setSize()`。Grid 应该通过 anchor stretch 让子节点自适应 cell 区域，或者引入一个 layout-only 的"override size"概念（类似于 `layout_size_` 但可由父布局写入，不影响 `size_`）。

但要注意：当前游戏代码中 `inventory_ui.cpp` 使用 Grid + `setCellSize()` 来统一槽位大小，这是核心用例。修改时必须确保这个路径继续正常工作。

#### UIL-011 Stack 使用一致尺寸语义 — OK

方向正确。但需要明确一个决策：**StackLayout 在计算主轴长度时，应该用 `getRequestedSize()` 还是 `getLayoutSize()`？**

- 如果用 `getLayoutSize()`：需要保证子节点的布局已经先于父节点计算完成（当前 `ensureLayout` 是自顶向下的，子节点在父节点 `onLayout()` 调用后才会重新计算——存在先有鸡还是先有蛋的问题）。
- 如果用 `getRequestedSize()`：对非 stretch 子节点正确，对 stretch 子节点不正确。

这正是 UIL-020 "measure/arrange 两阶段"要解决的核心问题。建议在 UIL-011 中仅做文档化和最小修正（如：对 stretch 子节点给出 warning log），把根本解决留给 UIL-020。

#### UIL-012 跨场景布局断言 — OK

为 InventoryUI/HotbarUI 增加布局断言是好的，但要注意这些测试可能需要 SDL 环境（因为涉及 `Context`）。建议区分：
- **纯布局测试**（UIL-001）：无 SDL 依赖
- **集成布局测试**（UIL-012）：需要 SDL，可 `GTEST_SKIP`

#### UIL-020 引入 measure/arrange 两阶段 — 方向正确，风险最高

这是整个 backlog 中最大的改动。几点建议：

**A. 明确两阶段的定义**

当前 `ensureLayout()` 本质上是一个单阶段 top-down 布局：
```
parent.ensureLayout()
  → 计算 parent.layout_size_ / layout_position_
  → parent.onLayout()
    → 设置 children position/size
    → children.ensureLayout() (下次被查询时惰性触发)
```

标准两阶段（参考 WPF/Flutter）：
```
Measure pass (bottom-up): child.measure(available) → child.desired_size
Arrange pass (top-down):  parent.arrange(child, final_rect) → child.layout_rect
```

关键区别在于 Measure 是 **bottom-up**（先子后父），而当前的 `ensureLayout` 是 **top-down**（先父后子）。引入 Measure 会显著改变布局流程。

**B. 建议不引入完整的两阶段**

对于当前项目的复杂度（两种布局容器 + 几个组合控件），完整的 measure/arrange 引入成本高、收益有限。建议改为：

- **UIL-020 alternative**：在 `UIElement` 中引入 `layout_override_size_`（可选），布局容器可通过此字段覆盖子节点的布局尺寸，而不修改 `size_`。`getLayoutSize()` 优先返回 override，否则走现有计算。这解决了 Grid 的副作用问题，且改动量小得多。
- 如果未来真的需要完整两阶段，再做 UIL-020 原版。

**C. 如果坚持两阶段，建议拆分更细**

当前 UIL-020 "兼容模式引入"+ UIL-021/022 "迁移"只分了 3 步。建议至少拆为：
1. 定义 `MeasureResult` 结构 + `measure()` 虚方法（默认返回 `size_`）
2. 让 `ensureLayout()` 内部可选走 measure-first 路径
3. 迁移 StackLayout
4. 迁移 GridLayout
5. 清理兼容路径

#### UIL-023 迁移依赖控件并清理 — OK

UIProgressBar、UIItemSlot、UIDragPreview 的 `onLayout()` 都是简单的子节点定位，迁移风险低。

#### UIL-030 优化脏标记传播 — OK

`invalidateLayout(true)` 目前是递归传播到所有后代，这确实可以优化。但在控件数量不多时（当前 UI 层级不深），这不是瓶颈。优先级 P1 合理。

#### UIL-031 交叉轴对齐 — OK

P2 可选项，合理。

---

## 3. 依赖关系评审

```
UIL-000 ─┬─ UIL-001 ─┬─ UIL-010 ─┬─ UIL-012
          │           │           │
          └─ UIL-002 ─┤           ├─ UIL-020 ─┬─ UIL-021
                      │           │            │
                      └───────────┘            ├─ UIL-022
                                               │
                                               └─ UIL-023 ─── UIL-030
                                                              UIL-031
```

**问题 D-1**：UIL-010 和 UIL-011 互相独立，但都依赖 UIL-001 和 UIL-002。它们可以并行。但 UIL-012 同时依赖 UIL-010 和 UIL-011。这意味着 UIL-012 要等两者都完成——这是合理的。

**问题 D-2**：UIL-020 依赖 UIL-010 和 UIL-011。但如果采用上面建议的 alternative 方案（`layout_override_size_`），UIL-020 的范围会大幅缩小，甚至可以合并到 UIL-010 中。

---

## 4. 补充建议

### S-1 [重要] 考虑用 `layout_override_size_` 替代完整两阶段

如上文 UIL-020 部分所述。在 `UIElement` 中增加：
```cpp
std::optional<glm::vec2> layout_override_size_;  // 布局容器可设置
```

`ensureLayout()` 中：
```cpp
if (layout_override_size_) {
    layout_size_ = *layout_override_size_;
} else if (stretched) {
    layout_size_ = available_size - margin;
} else {
    layout_size_ = size_;
}
```

Grid 的 `onLayout()` 改为：
```cpp
child->setLayoutOverrideSize(cell_size_);  // 不触发 setSize，不修改 requested
child->setPosition(new_pos);
```

这种方案：
- 解决了 Grid 副作用问题（UIL-010 的核心目标）
- 不需要引入 bottom-up measure pass
- 改动量约 20 行（UIElement + Grid）
- 未来如果需要完整两阶段，`layout_override_size_` 可以自然演化为 arrange 阶段的输出

### S-2 StackLayout 可见子节点计数可合并到第一遍循环

`ui_stack_layout.cpp:57-63` 第二个独立循环仅为了计数可见子节点，可以合并到第一遍循环中，减少一次遍历。这是一个小优化，可以在 UIL-011 中顺手做。

### S-3 `UIProgressBar::onLayout()` 中 `updateFillVisual()` 调用时机

`ui_progress_bar.cpp:101` 在 `onLayout()` 中调用 `updateFillVisual()`（设置子节点 anchor）。这会触发子节点 `invalidateLayout()`。虽然不会无限循环（`ensureLayout` 先设置 `layout_dirty_ = false` 再调用 `onLayout`），但语义上 `onLayout()` 不应该修改导致重新布局的属性。建议在布局语义文档（UIL-002）中明确 `onLayout()` 的约束：**只可修改子节点的 position，不可修改导致自身或兄弟节点重新布局的属性（如 anchor、size）**。

### S-4 明确 `getSize()` 应返回什么

当前 `UIElement::getSize()` 返回 `getLayoutSize()`，而 `getRequestedSize()` 返回 `size_`。这个命名容易混淆——用户调用 `getSize()` 时可能期望得到"我设置的那个 size"，但实际得到的是布局计算后的 size。建议在 UIL-002 语义文档中明确说明这个设计决策，并考虑是否需要重命名。

### S-5 与交互重构的协作方式 — 无异议

文档中已说明两个专项可以并行推进，但避免同时修改 UI 管线核心路径。这是正确的。交互重构已完成，所以现在布局重构可以独立推进。

---

## 5. 总结

| 维度 | 评价 |
|---|---|
| 问题诊断 | **准确** — Grid 副作用、Stack 尺寸语义、测试缺失均已确认 |
| 迭代策略 | **合理** — 先基线后实现，小步迭代 |
| 工作量估算 | **偏乐观** — UIL-020~022（两阶段引入）的 3d 估算偏低，建议 4-5d 或考虑 alternative 方案 |
| 依赖关系 | **合理** — 无循环依赖，关键路径清晰 |
| 风险识别 | **充分** — L1/L2/L3 均有缓解措施 |

**核心建议**：评估是否真的需要完整的 measure/arrange 两阶段（UIL-020~022）。对于当前项目规模，`layout_override_size_` 方案（见 S-1）可能是更好的选择——以 1/3 的改动量解决 80% 的问题。如果未来控件复杂度增长到需要 intrinsic size negotiation，再引入完整两阶段不迟。
