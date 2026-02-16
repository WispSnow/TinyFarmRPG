# WorldState（世界布局、邻接与跨图触发器）约定与排错

> 目标：让“制作侧（`.world/.tmj`）的布局与触发器配置”稳定落到运行时（`WorldState → MapManager → MapTransitionSystem`）的切图逻辑上，并在出错时能快速定位原因。

## 1) 数据源与路径约定
- 世界布局（World，Tiled world）：`assets/maps/*.world`
- 地图（Map，Tiled JSON map）：`assets/maps/*.tmj`
- `WorldState::loadFromWorldFile(..., maps_root)` 的默认 `maps_root` 为 `assets/maps/`，`fileName` 会直接拼接到该根目录下。

## 2) `.world` 最小 schema（本项目用到的字段）
`.world` 本质是 JSON。运行时只依赖：
- 顶层 `maps`：数组
- `maps[i].fileName`：地图文件名（通常为 `xxx.tmj`）
- `maps[i].x / y / width / height`：**像素**单位（px），用于推导邻接关系

示例（`assets/maps/farm-rpg.world`）：
```json
{
  "maps": [
    { "fileName": "home_exterior.tmj", "x": 0,   "y": 0, "width": 560, "height": 400 },
    { "fileName": "town.tmj",          "x": 560, "y": 0, "width": 560, "height": 400 }
  ]
}
```

运行时命名规则：
- `map_name = stripExtension(fileName)`（如 `home_exterior.tmj` → `home_exterior`）
- `map_id = entt::hashed_string(map_name).value()`

## 3) 邻接推导规则（像素对齐、边界相接）
邻接在加载 `.world` 后一次性推导（`WorldState::resolveAdjacency`），规则非常“硬”：
- **同一行（same row）**：`A.y == B.y`
  - 若 `A.x + A.width == B.x` → `A.east = B`
  - 若 `B.x + B.width == A.x` → `A.west = B`
- **同一列（same col）**：`A.x == B.x`
  - 若 `A.y + A.height == B.y` → `A.south = B`
  - 若 `B.y + B.height == A.y` → `A.north = B`

也就是说：
- 单位是 **像素**，要求 **严格对齐**（没有 gap/overlap）
- 只支持“正交贴边”的 4 邻接（N/S/E/W），不支持斜角/重叠

### 3.1 开发侧查询（世界图 API）
为减少上层反复手写循环/if，本项目提供了两个轻量查询：
- `WorldState::neighborsOf(map_id)` → `NeighborInfo{north/south/east/west}`
- `WorldState::outgoingTriggers(map_id)` → `span<TriggerInfo>`（当前地图加载后由 `EntityBuilder` 写入）

## 4) External Map（不在 `.world` 里的地图）
有些地图（常见：室内/副本/临时关卡）不一定放进 `.world`。本项目用“按需注册”的方式支持它们：
- `WorldState::ensureExternalMap(name)`：
  - 若已存在则直接返回 `map_id`
  - 否则创建一个 `MapState`，并设置：
    - `in_world = false`
    - `file_path = maps_root + name + ".tmj"`（例如 `assets/maps/home_interior.tmj`）

典型触发路径：
- `MapTransitionSystem::handleTriggerTransition`：触发器切图时若目标 map 不存在，会 `ensureExternalMap(target_map_name)`
- `MapManager::preloadRelatedMaps`：预加载触发器目标 map 时同样会 `ensureExternalMap(...)`

## 5) `map_trigger`（跨图触发器）properties 约定
制作侧位置：`.tmj` 的 object layer 中，rect object：
- `type = "map_trigger"`

必需 properties（见 `src/game/loader/tiled_conventions.h`）：
| name | type | 示例 | 含义 |
| --- | --- | --- | --- |
| `self_id` | int | `1` | 当前地图内的“入口点”编号 |
| `target_id` | int | `2` | 目标地图内要对接的“入口点”编号 |
| `target_map` | string | `home_interior` | 目标地图名（**不含**`.tmj`） |
| `start_offset` | string | `top/down/left/right` | 切过去后出生点在目标触发器 rect 的哪一侧 |

运行时落点：
- `EntityBuilder::buildMapTrigger`
  - 创建 `game::component::MapTrigger`
  - 同步一份 `TriggerInfo` 到 `WorldState::MapState::triggers`（用于“跨地图查询/预加载”）
- `MapTransitionSystem`（触发器切图）
  - 在目标地图中查找 `self_id == target_id` 的触发器
  - 用 `start_offset` 计算出生点（`computeOffsetPosition`）

## 6) 排错 checklist（最常见的坑）
### 6.1 `.world` 邻接不生效
- 检查 `x/y/width/height` 是否是 **像素**，且彼此是“贴边相接”（严格相等）
- 检查 `.world` 的 `fileName` 是否正确（能拼出真实存在的 `assets/maps/*.tmj`）
- 看日志：`WorldState: 成功加载 world 文件 ...` 与邻接双向校验 warn（`resolveAdjacency` 会对非对称邻接给出提示）

### 6.2 触发器切图失败 / 切过去找不到落点
- 检查当前地图的 `map_trigger` 是否填写了全部必需 properties
- 检查 `target_map` 是否 **不含**`.tmj`，并且 `assets/maps/<target_map>.tmj` 存在（external map 会按该规则拼路径）
- 检查目标地图是否存在 `self_id == target_id` 的对应触发器（用于落点匹配）
- 如果看起来“进门了但没触发”，先确认玩家点坐标是否真的进入触发器 rect（当前 inside 判定基于 `Transform.position_` 点）
