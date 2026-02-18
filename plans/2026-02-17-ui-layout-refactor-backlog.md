# TinyFarmRPG UI 布局重构 Backlog（2026-02-17）

## 背景与边界
- 来源：从 `plans/ui-refactor-backlog.md` 中移出的原 `UIR-052`（布局 measure/arrange）。
- 当前问题集中在：
  - `UIStackLayout` / `UIGridLayout` 以 `getRequestedSize()` 排布，和真实布局尺寸可能偏离。
  - `UIGridLayout` 在 `onLayout()` 内直接改子节点 `setSize(cell_size_)`，存在布局副作用链条。
  - 布局相关自动化测试缺失（`tests/engine/ui/` 无 `UIStackLayout`/`UIGridLayout` 覆盖）。
- 本专项目标：提高布局语义一致性与可测性，不与交互状态重构混做。

## 范围
- 包含：
  - `src/engine/ui/ui_element.*` 中布局核心流程（`ensureLayout/invalidateLayout`）
  - `src/engine/ui/layout/*`（`UILayout/UIStackLayout/UIGridLayout`）
  - 布局相关控件的适配（如 `UIProgressBar`、`UIItemSlot` 的布局依赖点）
  - `tests/engine/ui/` 中新增布局测试
- 不包含：
  - `UIInteractive/UIState/InteractionPhase` 交互状态逻辑
  - 输入系统、命中测试、行为回调机制

## 完成定义（DoD）
- 每个迭代项可独立提交和回滚。
- 每个迭代项都要满足：
  - 编译通过 + 布局专项测试通过。
  - 纯布局单测不依赖 SDL/OpenGL（不使用 `GTEST_SKIP`）。
  - 全量回归保持通过（`ctest --test-dir build --output-on-failure -j4`）。
  - 关键语义（请求尺寸、最终布局尺寸、锚点/拉伸）在文档中有明确约定。

## 设计约束
- 先“锁语义”再“改实现”。
- 优先消除行为歧义和副作用，再做性能优化。
- 避免一次性重写；保持每步 0.5~1.5d。
- `onLayout()` 默认只做布局定位，不直接修改会触发连锁脏化的持久属性（如子节点 requested size、anchor）。

---

## Iteration 0：基线与语义冻结

### UIL-000（P0）建立布局行为清单（已完成）
- 目标：明确当前可见行为，避免重构后“看起来差不多”。
- 主要改动：
  - 新增 `docs/testing/ui-layout-regression-checklist.md`
  - 覆盖场景：背包网格、快捷栏水平布局、分页按钮区、进度条、拖拽预览文字对齐
- 验收标准：
  - checklist 可独立执行并复现布局行为

### UIL-001（P0）补首批布局自动化测试
- 目标：建立最小防护网。
- 主要改动：
  - 新增 `tests/engine/ui/ui_stack_layout_test.cpp`
  - 新增 `tests/engine/ui/ui_grid_layout_test.cpp`
  - 覆盖：spacing、visible 子项跳过、auto-resize、固定 cell、大于/小于容器时对齐
  - 测试设计为纯布局单元测试，不依赖 SDL/OpenGL，不引入 `GTEST_SKIP`
- 验收标准：
  - 至少覆盖 6 个核心断言场景
  - 在无图形环境下可稳定执行

### UIL-002（P0）冻结布局语义文档
- 目标：明确 `requested size` 与 `layout size` 的契约。
- 主要改动：
  - 新增 `docs/ui/layout-contract.md`
  - 定义：`getRequestedSize/getLayoutSize/getSize` 的语义与使用场景
  - 定义：anchor/stretch/margin/padding 的优先级
  - 定义：`onLayout()` 的副作用边界（禁止写入导致链式重排的持久属性）
- 验收标准：
  - 文档可映射到现有 API（`getRequestedSize/getLayoutSize`）

---

## Iteration 1：先修当前布局实现中的高风险不一致

### UIL-010（P0）消除 `UIGridLayout::onLayout()` 的尺寸副作用
- 目标：避免布局阶段写入子节点请求尺寸导致链式脏化。
- 主要改动：
  - 在 `UIElement` 引入布局期覆盖尺寸（建议字段：`layout_override_size_`）
  - `UIGridLayout` 使用覆盖尺寸参与排布，不再直接 `child->setSize(cell_size_)`
  - 明确覆盖尺寸生命周期（布局帧内生效，不污染 requested size）
- 验收标准：
  - 同一帧重复 `ensureLayout()` 不出现额外尺寸抖动
  - `cell_size` 固定网格用例视觉不回退（背包/快捷栏）
  - 既有网格 UI 行为不回退

### UIL-011（P0）`UIStackLayout` 主轴计算改用最终布局尺寸语义
- 目标：减少 `requested size` 与实际显示尺寸不一致导致的偏移。
- 主要改动：
  - 主轴长度计算与定位逻辑收敛到一致的数据来源（遵循 `layout-contract.md`）
  - 合并可见子项计数与长度统计遍历，减少一次无效循环
  - 对当前无法准确处理的主轴 stretch 子项，增加调试告警与文档约束（避免静默错误）
- 验收标准：
  - Center/End 对齐在 stretch 子项下位置稳定

### UIL-012（P1）增加布局回归断言：跨场景实际 UI
- 目标：把单元测试与真实 UI 组合场景串起来。
- 主要改动：
  - 为 `InventoryUI` / `HotbarUI` 增加最小布局断言测试（例如槽位坐标间距、分页区域位置）
  - 作为集成测试分组；在无 SDL 环境可 `GTEST_SKIP`
- 验收标准：
  - 能检测布局重构后的位置回退

---

## Iteration 2：语义收敛与管线加固（不强推完整两阶段）

### UIL-020（P0）完善 `layout_override_size_` 管线与调试能力
- 目标：把“父布局可覆盖子布局尺寸”做成稳定机制。
- 主要改动：
  - 补充 override 清理/继承规则，避免跨帧残留
  - 增加 debug trace（requested/layout/override 三者可观测）
- 验收标准：
  - 默认行为不变，且可定位 override 来源

### UIL-021（P1）收敛 `UIStackLayout` 对子项尺寸读取策略
- 目标：在 Stack 中统一“读哪一种尺寸”语义。
- 主要改动：
  - 统一主轴总长计算与定位的尺寸来源（优先 layout 语义）
  - 增加 stretch/override 组合场景测试
- 验收标准：
  - 线性布局对齐结果可预测，测试可稳定复现

### UIL-022（P1）收敛 `UIGridLayout` cell 语义与边界行为
- 目标：明确 fixed cell 与 intrinsic size 的优先级，不留隐式行为。
- 主要改动：
  - 明确 fixed cell 与 intrinsic cell 的优先级
  - 增加超出容器/隐藏子项/不满行等边界断言
- 验收标准：
  - 网格类场景无重叠、无抖动、无尺寸回写副作用

### UIL-023（P1）迁移依赖控件并删除过渡分支
- 目标：减少双路径维护成本。
- 主要改动：
  - 适配 `UIProgressBar`、`UIItemSlot`、`UIDragPreview` 的布局依赖点
  - 清理临时兼容逻辑
- 验收标准：
  - 控件布局行为与基线一致

---

## Iteration 3：稳定性与性能收口

### UIL-030（P1）优化脏标记传播与重复布局开销
- 目标：减少无效 `invalidateLayout` 传播成本。
- 主要改动：
  - 收敛“值未变化仍触发脏化”的路径
  - 增加轻量布局统计（每帧 layout 次数）
- 验收标准：
  - 典型场景 layout 次数下降或不高于基线

### UIL-031（P2）补充对齐能力（可选）
- 目标：补齐 `UIStackLayout` 交叉轴对齐能力（当前主要是 Start）。
- 主要改动：
  - 增加 cross-axis `Start/Center/End`（保持向后兼容）
- 验收标准：
  - 新增对齐模式测试通过，旧布局不回退

### UIL-032（P2）评估完整 measure/arrange 原型（仅探索）
- 目标：为未来复杂控件预研，不纳入当前主线交付。
- 主要改动：
  - 输出设计草案：是否需要 bottom-up measure pass
  - 基于 1~2 个复杂示例评估收益/改造成本
- 验收标准：
  - 形成决策记录（继续演进 or 保持当前方案）

---

## Backlog 总表

| ID | 标题 | 优先级 | 预计工作量 | 依赖 |
|---|---|---|---|---|
| UIL-000 | 建立布局行为清单（已完成） | P0 | 0.5d | - |
| UIL-001 | 补首批布局自动化测试 | P0 | 1d | UIL-000 |
| UIL-002 | 冻结布局语义文档 | P0 | 0.5d | UIL-000 |
| UIL-010 | 消除 Grid 尺寸副作用 | P0 | 1d | UIL-001, UIL-002 |
| UIL-011 | Stack 使用一致尺寸语义 | P0 | 1d | UIL-001, UIL-002 |
| UIL-012 | 增加跨场景布局断言 | P1 | 1d | UIL-001 |
| UIL-020 | 完善 layout_override_size 管线 | P0 | 1d | UIL-010 |
| UIL-021 | Stack 尺寸读取策略收敛 | P1 | 1d | UIL-011, UIL-020 |
| UIL-022 | Grid cell 语义与边界收敛 | P1 | 1d | UIL-020 |
| UIL-023 | 迁移依赖控件并清理过渡 | P1 | 1d | UIL-021, UIL-022 |
| UIL-030 | 优化脏标记与重复布局开销 | P1 | 1d | UIL-023 |
| UIL-031 | 交叉轴对齐能力补齐（可选） | P2 | 1d | UIL-023 |
| UIL-032 | 完整 measure/arrange 预研（可选） | P2 | 1d | UIL-023 |

---

## 与交互重构的协作方式
- 当前状态：`UIR` 主线已完成，可独立推进 `UIL`。
- 若后续仍有交互改动并行进入，建议避免同批次混改输入命中与布局主路径。

## 风险与缓解
- 风险 L1：后续若直接引入完整两阶段，可能引入隐蔽布局回归。
  - 缓解：当前主线不强推完整两阶段，先完成 override 方案并建立测试覆盖。
- 风险 L2：布局专项与交互专项同时改 UI，冲突概率高。
  - 缓解：按目录与里程碑分支（`src/engine/ui/layout` 优先单独合并）。
- 风险 L3：测试成本上升。
  - 缓解：先最小核心断言，再补跨场景断言，不追求一次覆盖全部像素级差异。
