# UI 布局 measure/arrange 预研结论（2026-02-18）

## 背景
- 对应 `plans/2026-02-17-ui-layout-refactor-backlog.md` 中 `UIL-032`。
- 目标：评估是否在当前阶段引入完整两阶段布局（bottom-up measure + top-down arrange）。

## 现状基线（已具备）
- 已有 `requested size / layout size / layout override size` 语义契约与测试覆盖。
- `UIGridLayout` fixed cell 不再回写子节点 requested size。
- `UIStackLayout` 主轴/交叉轴对齐语义已稳定，并覆盖 stretch/override 组合场景。
- 脏标记冗余路径已收敛，具备 `layout_recompute_count` 调试计数。

## 方案评估
- 方案 A：继续演进当前单阶段 + override 方案。
  - 优点：改动面小，兼容现有控件，风险可控。
  - 成本：复杂控件的“内容驱动测量”能力有限，需要在控件层补约定。
- 方案 B：引入完整 measure/arrange。
  - 优点：理论上更统一，可支持复杂自适应布局。
  - 成本：需重写 `UIElement` 核心流程、布局容器与控件适配层，回归面显著扩大。
  - 风险：短期易引入行为回退（锚点、拖拽预览、文本尺寸依赖链路）。

## 决策
- 当前阶段选择 **方案 A（保持当前方案）**，不引入完整 measure/arrange 主线改造。
- 触发重评条件：
  - 出现 2 个以上复杂控件必须依赖多轮测量协商；
  - 或当前 override 方案在性能/维护性上出现持续性瓶颈（经 `layout_recompute_count` 与回归数据确认）。

## 后续建议
- 继续沿 backlog 方式做小步收敛，优先保证行为稳定与可回滚性。
- 若满足触发条件，再单开探索分支做最小可行原型（不直接并入主线）。
