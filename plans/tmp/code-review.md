# UI 布局重构 Code Review

> 审阅范围：`plans/2026-02-17-ui-layout-refactor-backlog.md` (UIL-000 ~ UIL-032) 全部已完成项
> 编码标准：`for_agent/code-guide.md`（现代 C++17/C++20、精简、仅最优方案、不考虑向后兼容）
> 审阅日期：2026-02-18

---

## 1. Backlog 完成度验证

| ID | 标题 | 状态 | 验证结果 |
|---|---|---|---|
| UIL-000 | 建立布局行为清单 | 已完成 | OK — `docs/testing/ui-layout-regression-checklist.md` 存在 |
| UIL-001 | 补首批布局自动化测试 | 已完成 | OK — `ui_stack_layout_test.cpp` (9 case) + `ui_grid_layout_test.cpp` (7 case)，无 SDL 依赖 |
| UIL-002 | 冻结布局语义文档 | 已完成 | OK — `docs/ui/layout-contract.md` 内容完整，覆盖全部 API 语义 |
| UIL-010 | 消除 Grid 尺寸副作用 | 已完成 | OK — `setLayoutOverrideSize(cell_size_)` 替代 `setSize(cell_size_)`，grep 确认零 `child->setSize` 残留 |
| UIL-011 | Stack 使用一致尺寸语义 | 已完成 | OK — 主轴统一使用 `getLayoutSize()`，合并可见子项计数到第一遍循环 |
| UIL-012 | 增加跨场景布局断言 | 已完成 | OK — 标记为集成测试分组 |
| UIL-020 | 完善 layout_override_size 管线 | 已完成 | OK — override 清理、debug trace、parent-change 自动 reset |
| UIL-021 | Stack 尺寸读取策略收敛 | 已完成 | OK — stretch/override 组合场景测试覆盖 |
| UIL-022 | Grid cell 语义与边界收敛 | 已完成 | OK — intrinsic 流式排布、边界溢出、reparent override 清除测试 |
| UIL-023 | 迁移依赖控件并清理过渡 | 已完成 | OK — ProgressBar/ItemSlot/DragPreview 已适配 |
| UIL-030 | 优化脏标记与重复布局开销 | 已完成 | OK — 同值跳过 + recompute counter + 传播短路 |
| UIL-031 | 交叉轴对齐能力补齐 | 已完成 | OK — `setCrossAxisAlignment(Alignment)` 支持 Start/Center/End |
| UIL-032 | 完整 measure/arrange 预研 | 已完成 | OK — 决策记录：保持当前方案，有明确触发重评条件 |

**结论：全部 13 项 backlog 均已按验收标准完成。**

---

## 2. 核心改动验证

### 2.1 `layout_override_size_` 机制（UIL-010 核心）

**UIElement 新增字段**：`std::optional<glm::vec2> layout_override_size_`

**`ensureLayout()` 优先级链** (`ui_element.cpp:342-349`):
```
layout_override_size_ 有值 → 使用 override
else stretch              → available_size - margin
else                      → requested_size
```

**验证**：
- `setLayoutOverrideSize()` 做 epsilon 比较避免冗余脏化
- `setLayoutOverrideSize()` 强制 `max(0, ...)` 防止负值
- `setParentInternal()` 自动 `reset()` override，防止跨容器残留
- trace 日志记录 set/clear/old/new

**评价**：设计干净，生命周期管理正确。采用我在 plan review 中建议的 `layout_override_size_` 方案（而非完整两阶段），是正确的工程决策。

### 2.2 Grid 副作用消除

**旧代码**：`child->setSize(cell_size_)` — 修改 `size_`，触发 `invalidateLayout`
**新代码**：`child->setLayoutOverrideSize(cell_size_)` — 仅影响 `layout_size_`，不污染 `size_`

Grid 测试 `FixedCellSizeAndSpacingDriveChildPositions` 验证：
- `getRequestedSize()` 保持原始值 (5x4, 7x8, 11x12)
- `getLayoutSize()` 返回 cell_size (20x10)

`ClearingFixedCellFallsBackToIntrinsicRequestedSize` 验证切换 cell→intrinsic 时 override 正确清除。
`ReparentAfterRemovalClearsFixedCellLayoutOverride` 验证 reparent 时 override 自动清除。

**评价**：副作用问题彻底解决，测试覆盖边界场景充分。

### 2.3 Stack 尺寸语义统一

**旧代码**：`child->getRequestedSize()` — 与 layout 结果可能偏离
**新代码**：`child->getLayoutSize()` — 使用最终布局尺寸

合并改进：可见子项计数不再单独循环，而是在第一遍遍历中用 `visible_children` vector 同步收集。

新增测试覆盖：
- `MainAxisStretchUsesLayoutSizeForAlignment` — stretch 子项的主轴长度正确
- `MainAxisLengthUsesLayoutOverrideInsteadOfRequestedSize` — override 子项读取最终尺寸
- `EndAlignmentIsStableWithStretchAndOverrideChildren` — stretch+override 混合对齐

**评价**：语义一致性问题解决，stretch/override 组合场景有测试保障。

### 2.4 Grid intrinsic 流式排布改进

旧 Grid `onLayout()` 使用固定公式 `col * (size.x + spacing.x)` 定位，假设所有子项同尺寸。

新 Grid 使用流式游标 `cursor_x/cursor_y`，按实际子项宽度推进，行间使用 `row_max_height` 跟踪最大行高。

测试 `IntrinsicVariableSizeUsesFlowLayoutPerRowWithoutOverlap` 验证变尺寸子项的正确排布。

**评价**：intrinsic 模式下的排布逻辑更准确，不再假设等宽。

### 2.5 脏标记优化（UIL-030）

**同值跳过**：`setSize/setPosition/setAnchor/setPadding/setMargin/setOrderIndex/setLayoutOverrideSize` 均增加了 epsilon 比较，值未变化时不触发脏化。

**传播短路** (`ui_element.cpp:291`):
```cpp
if (!propagate || was_dirty) {
    return;
}
```
已经 dirty 的节点不再向下递归传播——避免冗余遍历。

**recompute counter** (`ui_element.cpp:308`): `++g_layout_recompute_counter`，`UIManager::update()` 输出 trace 日志。

测试 `UILayoutInvalidationTest` 覆盖：同值 position/size/anchor 不触发 relayout + counter 正确统计。

**评价**：优化有效且测试完备。

### 2.6 交叉轴对齐（UIL-031）

新增 `cross_alignment_` 字段 + `setCrossAxisAlignment()` API + `resolveCrossAxisOffset()` lambda。

测试覆盖 Vertical+Center 和 Horizontal+End 两种组合。

**评价**：最小改动实现功能，无过度设计。

### 2.7 依赖控件适配（UIL-023）

#### UIProgressBar
- `onLayout()` 不再调用 `updateFillVisual()` — 消除了 onLayout 内修改子节点 anchor 的副作用
- `updateFillVisual()` 增加 anchor diff guard — 值未变化时不调用 `setAnchor`
- `showLabel()` 增加可见性判断 — 值未变化时不 `invalidateLayout`
- `setFillType()` 新增方法，增加 diff guard
- 构造函数末尾调用 `updateFillVisual()` — 初始化时一次性设置 anchor

#### UIItemSlot
- `onLayout()` fallback 初始化改为 `glm::vec2 pos{0.0f, 0.0f}` — 使用局部坐标原点，不再引用 `getPosition()` (父坐标系)

#### UIDragPreview
- `onLayout()` 改用 `getLayoutSize()` — 与 layout 语义一致
- `renderSelf()` 同样使用 `getLayoutSize()`

**评价**：三个控件的适配都是最小必要改动，符合精简原则。

---

## 3. 编码标准合规性

### 3.1 现代 C++ (C++17/C++20)

| 实践 | 评估 |
|---|---|
| `std::optional<glm::vec2>` | C++17 optional 用于可选 override |
| `std::nullopt` | 清除 override |
| 结构化绑定 `const auto& [child, length]` | StackLayout 第二遍遍历 |
| `[[nodiscard]]` 标注 | 辅助函数 `sameLayoutOverride/sameVec2/sameThickness` |
| lambda | `resolveCrossAxisOffset` 在 `onLayout()` 内定义 |
| `constexpr` | `kLayoutEpsilon`、`kAnchorEpsilon`、`COUNT_PADDING` |
| `static_cast<const void*>(this)` | trace 日志中的指针输出 |
| `std::uint64_t` | layout recompute counter |
| `0.0F` 后缀 | 统一浮点字面量风格 |

**评估：符合现代 C++ 标准。**

### 3.2 保持代码精简，不做过度防御

| 检查点 | 评估 |
|---|---|
| `setLayoutOverrideSize()` | 25 行，含 trace 日志。日志占比较高但 trace 级不影响运行时 |
| `ensureLayout()` 中 override 分支 | 2 行：`if (has_value()) final_size = override` — 最小侵入 |
| `setParentInternal()` override 清除 | 3 行：判断+trace+reset — 简洁 |
| `invalidateLayout()` 传播短路 | 2 行条件判断 — 精简有效 |
| 同值比较辅助函数 | `sameVec2/sameThickness/sameLayoutOverride` 匿名命名空间，无过度泛化 |
| Grid `onLayout()` | 42 行，流式游标逻辑清晰 |
| Stack `onLayout()` | 100 行（含交叉轴对齐），结构合理 |

**评估：符合精简要求。**

### 3.3 仅考虑最优方案，不考虑向后兼容

| 检查点 | 评估 |
|---|---|
| override 机制 | 直接嵌入 `UIElement`，不做 mixin 或 adapter |
| Grid 无 fallback 路径 | 没有保留旧 `setSize` 路径的开关 |
| Stack 直接改为 `getLayoutSize()` | 无 `getRequestedSize()` fallback |
| ProgressBar `onLayout()` | 直接移除 `updateFillVisual()` 调用，无条件编译 |
| measure/arrange 预研 | 明确决策"保持当前方案"，不留两阶段桩代码 |

**评估：完全符合，无向后兼容负担。**

---

## 4. 测试覆盖评估

### 布局行为测试（纯单元测试，无 SDL 依赖）

**UIStackLayoutTest** — 9 case:
- 水平布局跳过不可见子项
- Center 对齐偏移计算
- End 对齐允许负偏移（内容溢出）
- AutoResize 只统计可见子项
- 主轴 stretch 使用 layout size
- 主轴 layout override 使用最终尺寸
- End 对齐 + stretch + override 混合
- 交叉轴 Center (Vertical)
- 交叉轴 End (Horizontal)

**UIGridLayoutTest** — 7 case:
- Fixed cell + spacing 定位 + requested size 不变
- 不可见子项不占格子
- 非正列数设置被拒绝
- Fixed cell → intrinsic 回退
- Reparent 后 override 自动清除
- Intrinsic 变尺寸流式排布
- 溢出行不裁剪

**UILayoutInvalidationTest** — 3 case:
- 同值 position 不触发 relayout
- 同值 size/anchor 不触发 relayout
- Recompute counter 正确统计

**UILayoutSourceTest** — 5 case:
- ProgressBar onLayout 不调用 updateFillVisual
- ProgressBar fill visual 有 anchor diff guard
- ProgressBar showLabel 有可见性 diff guard
- ItemSlot count label fallback 使用局部坐标原点
- DragPreview 使用 layout size

**总计：24 个测试 case**

**评估**：覆盖充分。核心场景（Grid override 不污染 requested、Stack 读 layout size、脏标记优化、控件适配）均有直接断言。纯单元测试不依赖 SDL 是正确的设计决策。

---

## 5. 发现的问题

### F-1 [Minor] `invalidateLayout()` 逻辑有冗余判断

**位置**：`ui_element.cpp:291-301`

```cpp
if (!propagate || was_dirty) {
    return;
}

if (propagate) {    // ← 此处 propagate 必为 true（已被上方排除）
    for (auto& child : children_) { ... }
}
```

到达 `if (propagate)` 时 `propagate` 必定为 `true`（因为 `!propagate` 的情况已在第一个 `if` 中 return）。这个 `if (propagate)` 是冗余判断。

**影响**：无功能影响，纯代码冗余。
**建议**：移除外层 `if (propagate)` 判断，直接写循环体。

### F-2 [Minor] `ensureLayout()` 根节点路径不考虑 `layout_override_size_`

**位置**：`ui_element.cpp:310-323`

```cpp
if (!parent_) {
    layout_size_ = size_;          // ← 忽略了 layout_override_size_
    layout_position_ = position_;
    layout_dirty_ = false;
    return;                        // ← 也不调用 onLayout()
}
```

根节点路径直接使用 `size_`，不检查 `layout_override_size_`，也不调用 `onLayout()`。虽然当前使用中根节点不太可能被设置 override（override 通常由父布局容器设置），且 trace 日志中已记录了 override 值，但语义上这是一个遗漏。

**影响**：当前无功能影响——根节点不会被父布局容器覆盖。但如果手动对根节点调用 `setLayoutOverrideSize()`，override 会被忽略。
**建议**：在 `layout-contract.md` 中明确记录"根节点不响应 layout override"的约束，或者在根节点路径也加入 override 优先判断。

### F-3 [Info] Grid `onLayout()` 对不可见子项也设置/清除 override

**位置**：`ui_grid_layout.cpp:44-50`

```cpp
for (auto& child : children_) {
    if (use_fixed_cell) {
        child->setLayoutOverrideSize(cell_size_);   // ← 包括不可见子项
    } else {
        child->clearLayoutOverrideSize();            // ← 包括不可见子项
    }

    if (!child->isVisible()) continue;               // ← 可见性检查在 override 之后
    ...
```

不可见子项也会被设置/清除 override。这在语义上是正确的（保证了切换可见性后 override 状态一致），但有些多余——不可见子项的 layout size 通常不被查询。

**影响**：无功能影响。`setLayoutOverrideSize` 有 epsilon 比较，同值时不触发脏化，所以不会导致额外布局计算。
**建议**：保持当前行为（语义一致性优先于微优化）。

### F-4 [Info] `sameVec2` 辅助函数与 StackLayout 中的 `glm::distance` 阈值检查风格不统一

**UIElement** 使用 `sameVec2()` (逐分量 epsilon 比较)：
```cpp
bool sameVec2(const glm::vec2& lhs, const glm::vec2& rhs) {
    return std::fabs(lhs.x - rhs.x) <= kLayoutEpsilon && std::fabs(lhs.y - rhs.y) <= kLayoutEpsilon;
}
```

**StackLayout/GridLayout** 使用 `glm::distance() > 0.001f` (欧氏距离)：
```cpp
if (glm::distance(child_pos, new_pos) > 0.001f) { ... }
```

两种方式在数学上不完全等价——`sameVec2({0, 0.001}, {0.001, 0})` 会判断为相同（两个分量差都 <= 0.001），但 `distance < 0.001` 也会判断为相同（distance ≈ 0.00141）。实际差异极小，但风格不统一。

**影响**：无功能影响——0.001 像素级别的差异不可见。
**建议**：长期统一为逐分量比较（更可预测），但不急需修改。

### F-5 [Info] trace 日志量较大

`setLayoutOverrideSize()` 和 `ensureLayout()` 每次执行都会输出 trace 日志（约 200 字符/条）。在大量子节点的布局场景中，trace 级别日志量可能较大。

**影响**：无——trace 级别在 Release 模式下通常不输出。`spdlog` 的 trace 在低于编译期日志级别时是零开销。
**建议**：确认项目的 `SPDLOG_ACTIVE_LEVEL` 设置，确保 Release 构建不编译 trace 调用。

---

## 6. 文档质量评估

### `docs/ui/layout-contract.md`

- 覆盖完整：术语定义、基础语义、尺寸计算规则、位置计算规则、padding/margin/anchor/pivot 优先级、onLayout 约束、布局容器行为、可观测性、测试映射、变更纪律
- 特别好的设计：
  - 明确 `onLayout()` "推荐约束"而非"强制约束"——务实的文档策略
  - "变更纪律"要求同步更新文档、测试和 backlog
  - 测试映射一节让读者快速定位验证入口
- 唯一遗漏：根节点路径不调用 `onLayout()` 的行为已提及，但根节点是否响应 `layout_override_size_` 未明确（见 F-2）

### `plans/2026-02-18-ui-layout-measure-arrange-decision.md`

- 决策清晰：方案 A vs B、当前选择、触发重评条件
- 有上下文基线说明，便于后续评审时理解决策背景
- 篇幅适中，无过度分析

**评估：文档质量优秀。**

---

## 7. 总结

| 维度 | 评分 | 说明 |
|---|---|---|
| Backlog 完成度 | **A** | 13/13 全部按验收标准完成 |
| 核心问题修复 | **A** | Grid 副作用彻底消除、Stack 语义统一、脏标记优化有效 |
| 编码标准合规 | **A** | 现代 C++、精简、无向后兼容负担 |
| 测试覆盖 | **A** | 24 case，纯单元测试 + 源码契约测试双层覆盖 |
| 文档质量 | **A** | 语义契约完整、决策记录清晰 |
| 架构决策 | **A** | 选择 `layout_override_size_` 而非完整两阶段，工程判断正确 |
| 发现问题 | 2 Minor + 3 Info | 均不影响功能正确性 |

**整体评价：布局重构质量优秀。** `layout_override_size_` 机制以最小改动解决了 Grid 副作用和 Stack 语义不一致两个核心问题。脏标记同值跳过 + 传播短路有效减少冗余计算。交叉轴对齐作为增值功能自然集成。语义契约文档和决策记录为后续维护提供了清晰基线。建议处理 F-1 的小冗余，F-2 在文档中补充说明即可。
