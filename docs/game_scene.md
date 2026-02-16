# GameScene（组合根、初始化顺序与系统编排）约定与排错

> 目标：把 `GameScene` 看作“组合根（composition root）”，用**明确的装配顺序**与**固定的 update 顺序**表达系统间依赖，避免常见的“顺序 bug”（UI 不更新、时间不动、空间查询不一致、切图抖动等）。

## 1) GameScene 的职责边界（一句话）
`GameScene` 负责把“一个可运行的游戏闭环”装配出来：创建/持有关键对象（world/map/services/systems/UI/debug），并决定它们的初始化与更新顺序。

不属于它的职责：
- 不写玩法实现细节（那是各个 `System` 的职责）
- 不解析地图文件（那是 `LevelLoader + EntityBuilder` 的职责）
- 不决定“何时切图”（那是 `MapTransitionSystem` 的职责）
- 不定义存档格式（那是 `SaveService` 的职责）

## 2) 启动期：装配顺序（`GameScene::init`）
建议把 `init()` 读成一条“流水线”，每一步都在为后面的步骤提供前置条件：

1. **数据与基础设施**
   - `ensureBlueprintManager / ensureItemCatalog`：加载“数据驱动”的前置资源
   - `createCollisionResolver`：为移动/碰撞系统准备碰撞解析器

2. **registry.ctx 与 world-level 数据**
   - `initRegistryContext`：把 `GameTime` 放进 `registry.ctx()`（很多系统依赖它）
   - `initWorldState`：加载 `.world`，并把 `WorldState*` 放入 `registry.ctx()`

3. **地图服务（map services）**
   - `initFactory`：创建实体工厂（数据 + 资源 + spatial）
   - `initMapManager`：创建 `MapManager` 并读取地图加载配置（预加载模式等）
   - `initSaveService`：创建存档服务（后续可能读档）

4. **初始地图（baseline）**
   - `loadLevel`：加载初始地图（无论是新游戏还是读档前的 baseline）
   - `configureCamera`：设置相机初始 zoom（bounds 由 `MapManager` 统一配置）

5. **系统装配**
   - `initSystems`：创建系统对象与输入绑定（system 间依赖由 update 顺序表达）

6. **UI 与 Debug**
   - `initUI`：创建 UI（Inventory/Hotbar/Clock/Tooltip/Fade…）
   - `registerDebugPanels`：注册调试面板（Map Inspector / Inventory / Time…）

7. **事件监听与首帧一致性**
   - 连接 `InventoryChanged/HotbarChanged/HotbarSlotChanged` 用于驱动 UI
   - 若需要读档：在监听与 UI 就绪后调用 `SaveService::loadFromFile`，保证读档触发的事件不会“丢给空气”
   - 最后做一次 `InventorySyncRequest/HotbarSyncRequest`，确保首帧 UI 有稳定的 full sync

## 3) 运行期：更新顺序（`GameScene::update`）
`update` 的顺序不是随意的：它是在用“先后关系”表达依赖。

一个心智版阶段划分：
```text
1) 清理：RemoveEntitySystem
2) 过渡冻结：MapTransition active -> 只推进 transition + UI
3) 时间/光照：TimeSystem -> Light systems
4) 行为决策：输入/AI/对话/状态机
5) 运动与切图提交：MovementSystem -> MapTransitionSystem
6) 依赖位置的系统：SpatialIndexSystem -> 交互 -> CameraFollow -> 动画
7) UI：Scene::update -> UIManager
```

关键原则：
- **移动后更新空间索引**：否则空间查询会读到旧位置
- **切图过渡期间冻结世界**：避免切图瞬间的误操作与状态抖动
- **UI 放在最后**：UI 读的是“本帧已结算后的世界状态”

## 4) 常见顺序 bug 排错 checklist

### 4.1 UI 不更新 / 打开背包没有内容
- 确认 UI 是否在 `Scene::update` 路径中被更新（`GameScene::update` 末尾需要调用 `Scene::update`）
- 确认 `InventoryChanged/HotbarChanged` 的监听是否已连接
- 确认是否触发了至少一次 `InventorySyncRequest/HotbarSyncRequest`（full sync）

### 4.2 时间不动 / 昼夜不切换
- 确认 `registry.ctx()` 是否有 `GameTime`
- 确认 `TimeSystem` 是否在 update 早期被调用（其他系统依赖“最新时间”）

### 4.3 空间查询不一致（交互/碰撞“像是慢一拍”）
- 确认 `MovementSystem` 在前、`SpatialIndexSystem` 在后
- 确认 “恢复快照/读档” 后有触发一次索引重建或同步（详见 `docs/map_manager.md` 的快照恢复链路）

### 4.4 切图抖动 / 过渡期间仍能操作
- 确认 `MapTransitionSystem` active 时，世界逻辑被短路（只更新 transition + UI）
- 确认输入相关系统不会在过渡期间继续提交动作（例如 inventory/pause 的按键处理）

