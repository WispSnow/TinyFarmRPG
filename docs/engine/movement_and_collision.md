# 移动与碰撞解析（Movement + Collision）约定与排错指南

> 目标：把“输入/AI 给出的目标位移”稳定地转成“可行位移”，并在出现卡边/穿墙/互相穿模时能快速定位原因。

## 1) 运动链路（从 velocity 到 position）
TinyFarm 的移动链路可以按三段式理解：
1. **意图层**：玩家控制/AI 等系统只负责写 `VelocityComponent.velocity_`
2. **积分层**：`MovementSystem` 用 `target_pos = current_pos + v * dt` 得到目标位置
3. **解析层**：若实体带碰撞器，则用 `CollisionResolver::resolveMovement()` 把 `target_pos` 解析成 `resolved_pos` 并写回 `TransformComponent.position_`

关键代码：
- `src/engine/system/movement_system.h`
- `src/engine/system/movement_system.cpp`
- `src/engine/spatial/collision_resolver.h`
- `src/engine/spatial/collision_resolver.cpp`

## 2) 静态阻挡语义：SOLID vs 薄墙（BLOCK_*）
静态阻挡来自 `StaticTileGrid` 的 `TileType` 位标志：
- **SOLID**：不可进入的“实心墙体”（由 `BLOCK_N/S/W/E` 四向组合得到）
- **薄墙/方向边界（BLOCK_*）**：阻挡“跨过某条 tile 边界”，但 tile 本身未必是实心

碰撞解析中，薄墙/边界使用 sweep 裁剪：
- `StaticTileGrid::sweepHorizontal/sweepVertical`
- 统一入口：`SpatialIndexManager::resolveStaticSweep`

辅助信息（用于调试/可视化）：
- `SweepResult.hit_solid`：命中 SOLID 边界
- `SweepResult.hit_thin_wall`：命中薄墙边界
- `SweepResult.hit_info`：命中的 boundary row/col 与 sweep 覆盖的 tile span

相关代码：
- `src/engine/spatial/static_tile_grid.h`
- `src/engine/spatial/static_tile_grid.cpp`
- `src/engine/spatial/spatial_index_manager.h`
- `src/engine/spatial/spatial_index_manager.cpp`
- `src/engine/component/tilelayer_component.h`

## 3) 动态阻挡：broadphase 候选 → narrowphase 相交
动态实体阻挡来自动态网格（详见 `docs/spatial_index.md`）：
- broadphase：`SpatialIndexManager::queryColliderCandidates*`（返回候选实体）
- narrowphase：基于真实形状做相交判断（AABB/圆）

注意：动态网格是“分桶索引”，候选集合依赖实体在网格中的 cell 归属；如果 entity 的 Transform 变了但 dynamic grid 没同步，可能出现“候选漏掉 → 碰撞漏检”。

本项目的默认策略：
- `SpatialIndexSystem`：移动系统之后批量同步（`TransformDirtyTag` 驱动）
- `MovementSystemOptions.sync_dynamic_grid_during_movement`：在移动前/后为当前实体做一次就地同步，减少“本帧移动实体互相漏检”（适用于多实体同帧移动）

相关代码：
- `src/engine/system/spatial_index_system.cpp`
- `src/engine/spatial/dynamic_entity_grid.*`
- `src/engine/spatial/collider_shape.h`

## 4) 坐标与边界规则（避免卡边/穿墙）
- **坐标系**：本项目中 `move_down` 使 `velocity.y += 1`，即世界坐标 **Y 正方向向下**（屏幕坐标系）。
- **Rect 语义**：Rect 按半开区间 `[pos, pos+size)` 处理；最大端会用 epsilon 缩减，避免“刚好贴边”误算进下一格。
- **Touch 是否算碰撞**：目前 overlap 判定倾向于把“恰好贴边/相切”视为碰撞（例如 circle-circle 用 `<=`）；需要更“丝滑”的手感时可调整为严格 `<`（但要小心穿透）。

## 5) 排错 checklist（从现象到定位）
1. **穿墙/薄墙无效**
   - 打开 `Spatial Index` 调试面板 → 静态网格：启用“显示薄墙（BLOCK_* 边界）”
   - 检查对应 tile 是否真的设置了 `BLOCK_*` 标志
2. **动态实体互相穿模（尤其同帧移动）**
   - 确认实体带 `SpatialIndexTag` + Collider
   - 确认 `MovementSystemOptions.sync_dynamic_grid_during_movement` 未被关闭
3. **实体移动后仍停留在旧 cell（动态网格高亮不跟随）**
   - 检查是否正确设置了 `TransformDirtyTag`（或是否被提前清除）
   - 确认 `SpatialIndexSystem` 在移动系统之后更新
4. **“离墙一段距离停住”或“贴边抖动”**
   - 检查 dt 波动与速度是否过大（大步长更容易暴露边界/epsilon 问题）
   - 用调试面板对比 candidates（broadphase）与 overlaps（narrowphase）是否异常

