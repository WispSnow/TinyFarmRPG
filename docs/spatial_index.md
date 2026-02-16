# 空间索引（Spatial Index）使用约定与排错指南

> 目标：让玩法系统用“查询”替代“遍历”，并且在出现碰撞/交互异常时能快速定位问题。

## 1) 为什么需要两张网格
TinyFarm 的空间索引由两部分组成：
- **静态网格（StaticTileGrid）**：回答“某个瓦片/区域是否阻挡、是否带属性、瓦片上挂了哪些静态实体？”
- **动态网格（DynamicEntityGrid）**：回答“某个区域附近有哪些动态碰撞体候选？”（broadphase）

它们通过 `SpatialIndexManager` 统一对外暴露：玩法侧只依赖 `SpatialIndexManager`，不需要直接碰底层网格。

## 2) 数据结构与职责边界

### 2.1 StaticTileGrid（静态瓦片网格）
- 存储：每个瓦片一个 `TileCellData`：
  - `TileType` 位标志（`SOLID/BLOCK_N/S/W/E/HAZARD/WATER/INTERACT/ARABLE/OCCUPIED`）
  - “按 layer 分桶”的瓦片实体（同一瓦片、同一 layer 最多一个实体）
- 典型用途：
  - 角色移动：查询 `SOLID` 或方向阻挡（薄墙）
  - 玩法判定：例如耕地是否 `ARABLE`、格子是否 `OCCUPIED`
  - “瓦片层实体”：soil/wet/crop/rest/rock 等（由游戏逻辑注册）

相关代码：
- `src/engine/spatial/static_tile_grid.h`
- `src/engine/spatial/static_tile_grid.cpp`

### 2.2 DynamicEntityGrid（动态实体网格）
- 存储：把带碰撞器的实体按均匀网格 cell 分桶（一个实体可跨多个 cell）。
- 典型用途：
  - 返回候选集合（broadphase）：`queryEntities(Rect / Circle / At)`
  - 真实相交过滤（narrowphase）由 `SpatialIndexManager` 完成（需要读取 Collider 组件形状）

相关代码：
- `src/engine/spatial/dynamic_entity_grid.h`
- `src/engine/spatial/dynamic_entity_grid.cpp`

### 2.3 SpatialIndexManager（统一入口）
对外 API 分三类：
- **静态查询**：`isSolidAt/hasSolidInRect/getTileEntityAtWorldPos/...`
- **动态查询**：
  - broadphase：`queryColliderCandidates*`
  - narrowphase：`queryColliders*`（会读取 registry 里的 `AABBCollider/CircleCollider` 做真实相交）
- **组合查询**：`checkCollision`（同时返回静态碰撞 + 动态碰撞列表）

相关代码：
- `src/engine/spatial/spatial_index_manager.h`
- `src/engine/spatial/spatial_index_manager.cpp`

## 3) 运行时更新约定（ECS）

### 3.1 “谁会进入动态网格”
- 约定：**拥有 `SpatialIndexTag` 的实体必须同时拥有碰撞器（`AABBCollider` 或 `CircleCollider`）**。
- 若出现 `SpatialIndexTag` 但没有 Collider：`SpatialIndexSystem` 会告警，并清掉 `TransformDirtyTag`（避免每帧重复扫描）。

相关代码：
- `src/engine/component/tags.h`
- `src/engine/system/spatial_index_system.cpp`

### 3.2 “什么时候更新动态网格”
- 约定：实体位置发生变化时，需要设置 `TransformDirtyTag`。
- `SpatialIndexSystem` 只更新带 `TransformDirtyTag` 的实体，并在更新后移除该 tag。

## 4) 坐标与边界规则（避免 off-by-one）
- **Point → tile/cell**：使用 `floor` 映射；落在边界上会归到“右/下”的格子（半开区间）。
- **Rect 查询**：把 Rect 视为半开区间 `[pos, pos+size)`；计算覆盖范围时会对 max 端做轻微缩减（epsilon），避免“刚好贴边”意外算进下一格。
- **World bounds**：动态网格的世界范围同样按半开区间处理；坐标恰好等于 `world_bounds_max` 会视为越界。

## 5) 排错 checklist（从现象到定位）

当你遇到“碰撞/交互/耕作判定不符合预期”时：
1. 打开 `Spatial Index` 调试面板 → **静态网格**：
   - 开启网格线/高亮 SOLID/属性，确认 TileType 标志是否符合预期
   - 用“单元格查询”输入 tile 坐标，核对 flags 与该格子的 tile entities
2. 切到 **动态网格**：
   - 开启网格线/高亮有实体的 cell/实体包围盒，确认目标实体是否被正确注册
   - 如果实体移动后仍停留在旧 cell：检查是否正确设置了 `TransformDirtyTag`
3. 用 **综合查询**：
   - 对比 `candidates（broadphase）` 与 `overlaps（narrowphase）` 数量
   - candidates 很大：考虑调整动态 cell size 或检查实体是否跨过多 cell
4. 回到系统顺序：
   - 确认 `SpatialIndexSystem` 更新发生在“移动系统之后”（保证网格是最新的）

