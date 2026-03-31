# RmlUi Runtime / Render Backend 重构计划索引

本文件是总览索引。

- 上层：本索引，只保留背景、全局约束、阶段顺序、状态看板
- 次层：分阶段文档，开发时只打开当前 `phase-xx.md`

状态标记：

- `[x]` 已完成
- `[~]` 部分完成
- `[ ]` 未开始

## 背景

当前项目的 RmlUi 集成把三层职责揉在了一起：

1. retained-mode UI runtime
2. GL render backend
3. 游戏层 UI composition

主要问题：

- 上层代码需要通过 `Context -> GLRenderer -> RmlUILayer` 才能拿到 UI runtime
- 官方 sample 里的 `ProcessEvents / Update / Render` 在本项目里没有显式分层
- `GameScene` 承担了过多 UI 组合职责

本次重构的最终目标是：

1. 拆出 `RmlUiRuntime`
2. 拆出 `RmlUiRenderBackendGl`
3. 让 `Context` 只暴露 runtime
4. 让 `GLRenderer` 只保留 render hook
5. 恢复显式的 `ProcessEvents / Update / Render` 三段式
6. 提取 `GameSceneUiController`

## 全局约束

- 不引入薄 `RmlUiService`
- `Rml::Initialise()` / `Rml::Shutdown()` 由 `GameApp::initRmlUi()` 和 `RmlUiRuntime` 统一协调
- 默认字体加载放在 `GameApp::initRmlUi()`，不散落在构造函数里
- `RmlUiRuntime` 负责：
  - `Rml::Context`
  - 文档管理
  - active scene
  - focus / queued focus
  - navigation
  - SDL event 处理
  - `Update()`
  - viewport state 对输入坐标和 `Context` 尺寸的同步
- `RmlUiRenderBackendGl` 负责：
  - `RenderInterface_GL3_STB`
  - logical size
  - texture filter
  - `render(context, viewport)`
- viewport 同步链路固定为：
  - `GameApp` 在 init 和 resize 后把 viewport state 推送给 `RmlUiRuntime`
  - `GLRenderer` 在 render 阶段把当前 viewport 传给 render hook
  - `RmlUiRenderBackendGl` 在 `render(context, viewport)` 中消费 viewport，不持有独立的持久 viewport 状态
- `GLRenderer` 不直接依赖 runtime；通过 render hook callback 调用 RmlUi 渲染
- `Context` 只暴露 `RmlUiRuntime*`
- `HoverFocusSyncListener`、`InventoryMenuScene`、全部 focus API 都在迁移范围内
- 移动 `update()` 时必须审计 `queueFocus*()` 调用点

## 阶段顺序

| Phase | 状态 | 文档 | 说明 |
|------|------|------|------|
| Phase 1 | `[x]` | [`phase-01-bootstrap.md`](./rmlui-service-and-game-scene-ui-refactor/phase-01-bootstrap.md) | 拆 `runtime / backend / viewport`，建立 bootstrap 与兼容壳 |
| Phase 2 | `[x]` | [`phase-02-engine-wiring.md`](./rmlui-service-and-game-scene-ui-refactor/phase-02-engine-wiring.md) | `GameApp` ownership、render hook、`Context` 入口、引擎层接线 |
| Phase 3 | `[x]` | [`phase-03-frame-loop.md`](./rmlui-service-and-game-scene-ui-refactor/phase-03-frame-loop.md) | 恢复 `ProcessEvents / Update / Render` 三段式 |
| Phase 4 | `[ ]` | [`phase-04-game-migration.md`](./rmlui-service-and-game-scene-ui-refactor/phase-04-game-migration.md) | 迁移游戏层场景、wrapper、菜单和库存 UI |
| Phase 5 | `[ ]` | [`phase-05-game-scene-ui-controller.md`](./rmlui-service-and-game-scene-ui-refactor/phase-05-game-scene-ui-controller.md) | 提取 `GameSceneUiController` |
| Phase 6 | `[ ]` | [`phase-06-cleanup-and-tests.md`](./rmlui-service-and-game-scene-ui-refactor/phase-06-cleanup-and-tests.md) | 删除兼容壳与旧入口，补测试 |

## 关键检查点

- Phase 2 结束后，执行阶段 A 检查：
  - `src/engine/**` 中对 `getRmlUILayer()` 的直接调用为零
  - 仅允许 `GLRenderer` 的 legacy bridge 保留 `getRmlUILayer()` 声明/定义
  - 引擎层全部改走 `Context::getRmlUi()` 或 render hook
- Phase 6 开始前，执行阶段 B 检查：
  - 全仓库 `getRmlUILayer()` 调用为零
  - 可以删除兼容壳与 renderer backdoor

## 使用方式

1. 先看本索引，只确认当前阶段和全局约束。
2. 开发时只打开当前 `phase-xx.md`。
3. 完成一个 phase 后，只回写本索引状态和该 phase 文档里的完成标记。

## 当前总待办

- [x] Phase 1 完成
- [x] Phase 2 完成
- [x] Phase 3 完成
- [ ] Phase 4 完成
- [ ] Phase 5 完成
- [ ] Phase 6 完成
