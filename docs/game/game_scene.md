# GameScene（组合根、初始化顺序与 UI 装配）约定与排错

> 目标：把 `GameScene` 看作“游戏玩法 + 世界服务 + RmlUi HUD”的组合根，用明确的装配顺序和更新阶段表达依赖，避免 UI 不同步、锚点抖动、切图时序错乱等问题。

## 1) GameScene 的职责边界
`GameScene` 负责装配一个可运行的 gameplay 闭环：world、services、scheduler、输入绑定，以及 gameplay HUD 的组合控制器 `GameSceneUiController`。

不属于它的职责：
- 不直接实现玩法细节
- 不直接实现 Hotbar / Tooltip / DialogueBubble 的 DOM 细节
- 不直接实现 InventoryMenu 的具体交互
- 不解析地图文件和存档格式

## 2) 启动期：装配顺序（`GameScene::init`）

建议把 `init()` 理解成一条固定流水线：

1. 基础数据与服务
   - Blueprint、ItemCatalog、碰撞/地图/存档相关基础设施

2. `registry.ctx()` 与 world-level 数据
   - `GameTime`
   - `WorldState`

3. 地图与工厂
   - 实体工厂、`MapManager`、初始地图加载、相机初始配置

4. 系统装配
   - 由 `GameRuntimeAssembler` 组装 gameplay systems
   - `GameScene` 进入 `Gameplay` 输入上下文并绑定场景级动作

5. UI 装配
   - 创建 `GameSceneUiController`
   - 由它进一步创建：
     - `HotbarUI`
     - `TimeClockHud`
     - overlay prompt bar
     - `ItemTooltipUI`
     - `DialogueBubbleController` + 3 个 `DialogueBubbleView`
     - `RmlScreenFade`

这里要特别注意：
- 这些 UI 不是必须合并进同一个 `.rml`
- 当前实现本来就是多个独立文档并存，共享同一个 `scene_instance_id`
- `owner_scene_id` 只负责把它们归到同一个 Scene 名下，便于 Scene 退出时统一回收

6. 调试面板
   - 注册 ImGui debug panel

7. 事件监听与首帧同步
   - 连接：
     - `HotbarChanged`
     - `HotbarSlotChanged`
     - 战斗进入/结束事件
   - 若需要读档，必须在 UI 和监听就绪后再执行 `SaveService::loadFromFile(...)`
   - 首帧为玩家 enqueue：
     - `InventorySyncCommand`
     - `HotbarSyncCommand`

说明：
- `InventoryMenuScene` 不是 `GameScene` HUD 的一部分，而是后续通过 `pushScene(...)` 打开的独立覆盖 Scene
- `GameScene` 自己不直接监听 `InventoryChanged`，因为常驻 HUD 只关心 hotbar 与 prompt bar
- `GameScene` 也不是“一个 Scene 只配一个 controller”；常驻 HUD 内部本来就是多个独立模块，各自按需要持有 controller 或直接操作文档

## 3) 运行期：固定逻辑、帧表现与 retained UI 的分工

### 3.1 `fixedUpdate(delta_time)`
- gameplay scheduler 在这里推进
- 这是真正的玩法逻辑阶段
- 系统先后关系由 scheduler 定义，而不是由 `GameScene::update()` 手写串起来

### 3.2 `update(delta_time)`
- 这里只做帧表现层更新
- 当前主要包括：
  - `VfxService::update(delta_time)`
  - `GameSceneUiController::update(delta_time)`
  - `Scene::update(delta_time)`

也就是说：
- HUD 文本、tooltip、screen fade、clock hand 这类表现更新在这里做
- gameplay 系统逻辑已经不在这里推进

### 3.3 `prepareUi(interpolation_alpha)`
- 这是 retained UI 在当前项目中的关键阶段
- `GameScene` 会：
  - 对相机位置做插值
  - 调用 `ui_controller_->refreshAnchoredWidgets(camera, alpha)`
  - 刷新 dialogue bubble 等 world-anchor UI 的屏幕位置

随后 `GameApp::render()` 会执行：
1. `scene_manager_->prepareUi(alpha)`
2. `rmlui_runtime_->update()`
3. `scene_manager_->render(alpha)`

因此：
- 世界锚点 UI 的位置刷新必须发生在 `prepareUi()`，而不是 `render()`

## 4) UI 相关的关键链路

### 4.1 常驻 HUD
- `HotbarChanged` / `HotbarSlotChanged`
  - `GameScene` 接收事件
  - 转发给 `GameSceneUiController`
  - 再投影到 `HotbarUI`

### 4.2 打开 InventoryMenu
- `GameScene` 负责 push `InventoryMenuScene`
- InventoryMenu 自己读取玩家组件做初始同步，并监听：
  - `InventoryChanged`
  - `HotbarChanged`

### 4.3 Dialogue / Tooltip / Fade
- Dialogue bubble：
  - 由 gameplay 事件驱动
  - 由 `DialogueBubbleController` 路由到 3 个 channel
- Tooltip：
  - 由 `HotbarUI` 或 `InventoryMenuScene` 根据 hover 状态驱动
- Fade：
  - 由 `GameSceneUiController` 暴露 `IScreenFade*`

## 5) 常见排错 checklist

### 5.1 HUD 不更新 / 热键栏没有内容
- 确认 `GameSceneUiController` 初始化成功
- 确认 `HotbarChanged` / `HotbarSlotChanged` 监听已连接
- 确认首帧或显隐切换后有 `HotbarSyncCommand`

### 5.2 打开背包没内容
- 确认 `InventoryMenuScene::initUI()` 成功加载文档
- 确认玩家实体同时带有 `InventoryComponent` / `HotbarComponent`
- 确认菜单进入后完成了本地 `syncFromInventory()` / `syncHotbarFromInventory()`

### 5.3 Dialogue bubble 位置抖动或滞后一帧
- 确认 world-anchor 刷新发生在 `prepareUi(alpha)`
- 确认使用了插值相机位置，而不是 render 后再补改 DOM

### 5.4 切图或过渡时 UI 状态错乱
- 确认 HUD 的 frame update 仍在运行
- 确认 `RmlScreenFade` phase 与 `transitionend` 正常推进
- 确认过渡期间输入上下文和 Scene 栈状态符合预期
