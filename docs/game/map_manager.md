# MapManager（地图生命周期、预加载、快照与离线推进）约定与排错

> 目标：把“切图”当作一笔可控的事务（unload → load → restore），并用工程化手段解决三类问题：**卡顿**（预加载）、**状态丢失**（快照）、**玩家不在场时的世界变化**（离线推进）。

## 1) MapManager 的职责边界（一句话）
`MapManager` 是地图生命周期编排器：决定 **何时卸载/加载/恢复**，并把“地图文件构建（LevelLoader）”与“地图动态状态（snapshot/offline）”组合成可复现的运行时行为。

不属于它的职责：
- 不决定“何时切图”（那是 `MapTransitionSystem` 的职责）
- 不实现 tile/object 的解析细节（那是 `LevelLoader + EntityBuilder` 的职责）
- 不负责存档文件格式（那是 `SaveService` 的职责）

## 2) 一次切图 = 一笔事务（transaction）
建议把 `loadMap(...)` 读成“提交一笔事务”，它大致包含这些阶段：

1. **准备阶段（准备 MapState）**
   - 通过 `WorldState` 获取 `MapState`（`file_path/name` 等）
   - 清理本地图运行期收集的 `triggers`（每次加载后由 `EntityBuilder` 重新同步）
   - external map 会在这里读取 `ambient_override`（室内环境光）

2. **卸载阶段（unload）**
   - `snapshotMap(current)`：把当前地图的动态实体写入快照（见第 4 节）
   - `destroyEntitiesByMap(current)`：按 `MapId` 销毁非玩家实体，避免残留

3. **加载阶段（load）**
   - 创建 `LevelLoader`
   - 注入 `game::loader::EntityBuilder`（让 loader 能构建游戏语义，如 map_trigger）
   - `LevelLoader::loadLevel(file_path)`：构建本地图的 ECS 实体与组件

4. **后处理阶段（post-load / restore）**
   - 计算并记录 `current_map_pixel_size_`（map_size * tile_size）
   - 配置相机边界（limit bounds / zoom range）
   - `restoreSnapshot(map_id)`：把动态状态恢复回 ECS，并重建空间索引
   - `applyPendingResourceNodes / applyOpenedChests`：处理“资源/宝箱”这类需要与地图文件合并的状态

5. **预加载阶段（preloading）**
   - 如果配置为 `neighbors`：预加载邻接地图与触发器目标地图（依赖 WorldState 的世界图信息，详见 `docs/world_state.md`）

## 3) 预加载（Preload）：降低切图卡顿
配置文件：`assets/data/map_loading_config.json`
- `preload.mode`：
  - `off`：不预热（切图时更可能卡顿，但启动更快）
  - `neighbors`：只预热“当前地图的邻接 + 触发器目标”
  - `all`：启动期把 `WorldState` 里已知地图全部预热
- `log_timings`：打印加载阶段计时（用于对比策略差异）

关键入口：
- `GameScene::initMapManager()`：读取配置并设置到 `MapManager`
- `MapManager::preloadAllMaps()`：全量预热
- `MapManager::preloadRelatedMaps(map_id)`：邻接/触发器目标预热

预加载的本质：
> 提前让 `.tmj/.tsj` 的 JSON、tileset、纹理等进入缓存，减少“第一次切换到某地图”的尖峰成本。

## 4) 快照（Snapshot）：保存“地图动态状态”
本项目把“地图内会变化的状态”作为快照的一部分，写入 `WorldState::MapState::persistent.snapshot`：
- 已耕地（AutoTile rule：`soil_tilled`）
- 已浇水（AutoTile rule：`soil_wet` + `WaterWetTag`）
- 作物实体（CropComponent）
- （可选）资源节点（ResourceNodeComponent，受 flags 控制）

写入时机：
- `unloadCurrentMap()` 会先 `snapshotMap(current_map_id_)`
- 某些操作（例如资源节点变更）也会触发写回（实现上由具体系统/MapManager 调用）

恢复时机：
- `loadMap(...)` 在地图构建后会 `restoreSnapshot(map_id)`

关键点（很容易忽略）：
> 恢复快照不仅是“把实体读回来”，还要把这些实体重新注册到 `SpatialIndex`，否则碰撞/查询会失效。

## 5) 离线推进（Offline Advance）：玩家不在场时世界也会变化
触发来源：
- `TimeSystem` 跨天时 enqueue `DayChangedEvent`
- `MapManager::onDayChanged` 订阅该事件，对 **非当前地图** 执行 `advanceOffline`

实现方式（当前 demo 的做法）：
- 把快照读入一个临时 `entt::registry`
- 调用 `simulateOfflineDays(...)`（示例：作物按天推进）
- 再把临时 registry 写回快照，并更新 `last_updated_day`

这是一种通用但偏重的实现：
- 优点：实现简单，能复用“运行时组件”的逻辑
- 缺点：地图/实体变多后成本会变大（可作为扩展讨论）

## 6) 常见排错 checklist

### 6.1 切图后出现“残留实体”
- 第一优先：确认新建实体是否都带 `MapId`（地图归属标签）
- MapManager 会在加载后 warn “疑似地图实体缺少 MapId”（出现时建议立即定位哪个创建点漏了 `attachMapId/decorateExternalEntity`）
- 关键线索：
  - `EntityBuilder::attachMapId / decorateExternalEntity`
  - `MapManager::destroyEntitiesByMap`

### 6.2 切回地图后“耕地/作物/资源状态丢了”
- 检查该状态是否属于快照覆盖范围（第 4 节）
- 检查是否在卸载时成功 `snapshotMap`
- 检查恢复后是否重建了 `SpatialIndex`（否则可能“看得到但交互/碰撞不对”）

### 6.3 离线推进不生效（回到地图作物没变）
- 确认 `DayChangedEvent` 是否真的发生（推进一天必须触发）
- 确认该地图不是当前地图（当前地图通常由实时系统更新；离线推进针对“玩家不在场”）
- 确认该地图有有效快照（`snapshot.valid=true`），且 `last_updated_day` 合理

### 6.4 预加载看不到效果/仍然卡顿
- 检查 `assets/data/map_loading_config.json` 的 `preload.mode` 是否为期望值
- 开启 `log_timings` 对比 `off/neighbors/all` 的加载耗时差异
- 确认“目标地图”是否属于预加载集合：
  - `neighbors` 只预热邻接与触发器目标（依赖 WorldState 的 world graph 信息，详见 `docs/world_state.md`）
  - external map 可能需要先被 `ensureExternalMap` 注册，才会进入 `maps()` 遍历

