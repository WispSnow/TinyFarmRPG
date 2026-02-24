# 地图数据管线（Tiled → Runtime）约定与速查

> 目标：把“制作侧（Tiled）配置的语义”稳定映射到“运行时（ECS + SpatialIndex）可消费的数据”，并在配置出错时能快速定位。

## 1) 数据源与目录约定
- 地图（Map）：`assets/maps/*.tmj`
- 图块集（Tileset）：`assets/maps/tileset/*.tsj`
- 世界布局（World，可选）：`assets/maps/*.world`
- 地图加载配置：`assets/data/map_loading_config.json`

## 2) 运行时主链路（从入口到落点）
从运行入口看，加载链路可按三段理解：
1. **GameScene（入口）**：读取 `MapLoadingSettings`，创建/配置 `MapManager` 并触发加载。
2. **MapManager（编排）**：决定加载目标、预加载策略、快照恢复、相机配置等。
3. **LevelLoader（解析 + 建实体 + 注册语义）**：
   - 解析 `.tmj`，加载 `.tsj`
   - 遍历 layers，分派到 `loadTileLayer/loadObjectLayer/loadImageLayer`
   - 调用 `BasicEntityBuilder`（+ `game::loader::EntityBuilder`）创建实体/组件
   - 把 `TileType` 与瓦片实体写入 `SpatialIndexManager`（静态网格）

关键代码入口：
- `src/game/scene/game_scene.cpp`
- `src/game/world/map_manager.cpp`
- `src/engine/loader/level_loader.cpp`
- `src/engine/loader/basic_entity_builder.cpp`
- `src/game/loader/entity_builder.cpp`

## 3) 约定（Conventions）集中定义位置
为避免 magic string 漂移，本项目把 Tiled 关键字段约定集中到：
- 引擎层：`src/engine/loader/tiled_conventions.h`
- 游戏层：`src/game/loader/tiled_conventions.h`

> 本文档只关注"有哪些约定、在哪配置、如何排错"，不逐行讲解析实现。

## 4) 速查表：从 `.tmj/.tsj` 到运行时语义

| 制作侧位置 | 关键字段（示例） | 运行时落点 | 消费方/用途 | 代码定位 |
| --- | --- | --- | --- | --- |
| tileset tile properties（`.tsj`） | `tile_flag="SOLID,BLOCK_N"` | `engine::component::TileType` 写入 `StaticTileGrid` | 碰撞/移动/交互 | `LevelLoader::getTileType` → `SpatialIndexManager::setTileType` |
| tile layer data（`.tmj`） | `layers[].type="tilelayer" + data[] gid` | 每个非空瓦片一个实体；并在 `StaticTileGrid` 按 layer 分桶保存实体 | 渲染/自动图块/地块实体（游戏逻辑） | `LevelLoader::loadTileLayer` |
| object layer（`.tmj`）point object | `type="actor" + point=true + name="player"` | 生成角色实体 | 角色/动物出现在地图上 | `EntityBuilder::build` → `buildActor/buildAnimal` |
| object layer（`.tmj`）rect object | `type="map_trigger" + properties{...}` | `game::component::MapTrigger` + 同步到 `WorldState::MapState::triggers` | 过图/邻接触发 | `EntityBuilder::buildMapTrigger` |
| object layer（`.tmj`）rect object | `type="rest"` | `game::component::RestArea` + 进 `SpatialIndex` | 休息交互 | `EntityBuilder::buildRestArea` |
| object layer（`.tmj`）light | `type="light" + name="point/spot/emissive"` | `PointLight/SpotLight/EmissiveRect` 组件 | 光照系统 | `EntityBuilder::buildPointLight/buildSpotLight/buildEmissiveRect` |
| map properties（`.tmj`） | `ambient={red,green,blue}` | `WorldState::MapInfo.ambient_override` | 室内环境光覆盖 | `MapManager::loadMap` → `loadAmbientOverride` |
| tile properties（`.tsj`） | `obj_type="crop/tree/rock/chest"` | 生成对应组件（`CropComponent`/`ResourceNodeComponent` 等）与空间层语义 | 作物/资源点/宝箱等 | `EntityBuilder::buildFarmTag` |

### 4.1 Object Types 速查（`EntityBuilder` 扩展点）

#### `actor`（point object）
- **必需字段**：`type="actor"` + `point=true` + `name="<actor-id>"`
- **语义**：创建角色实体；其中 `name="player"` 会触发“复用玩家实体”的分支（如果启用该策略）。

#### `animal`（point object）
- **必需字段**：`type="animal"` + `point=true` + `name="<animal-id>"`
- **语义**：创建动物实体（细节由 `EntityFactory` 负责）。

#### `map_trigger`（rect object）
- **必需字段**：`type="map_trigger"` + `width/height > 0`
- **必需 properties**：
  - `self_id`（int）
  - `target_id`（int）
  - `target_map`（string，地图名**不含** `.tmj`，例如 `home_exterior`；实际文件路径由 `WorldState` 统一拼接）
  - `start_offset`（string）：`left/right/top/up/bottom/down`
- **语义**：创建 `MapTrigger`，并同步 `TriggerInfo` 到 `WorldState::MapState::triggers`（用于预加载/跨地图查询）。

#### `rest`（rect object）
- **必需字段**：`type="rest"` + `width/height > 0`
- **语义**：创建 `RestArea`，并把覆盖到的瓦片注册为 `INTERACT`（静态网格）。

#### `light`（object type 固定为 `light`；具体由 `name` 决定）
- `name="point"`（point object）
  - **必需 properties**：`radius`（float）
  - **可选 properties**：`night_only/day_only`（bool；两者同时为 true 时将始终隐藏，并给出 warn）
- `name="spot"`（point object）
  - **必需 properties**：`spot`（class，字段：`radius/inner_deg/outer_deg/direction_deg`）
  - **可选 properties**：`night_only/day_only`（bool）
- `name="emissive"`（rect object）
  - **可选 properties**：`intensity`（float，默认 1.0）
  - **可选 properties**：`night_only/day_only`（bool）

### 4.2 最小可复现样例（直接指向现有资源）
- `assets/maps/home_interior.tmj`
  - `map_trigger`：object `id=1`（`target_map="home_exterior"`, `start_offset="top"`）
  - `rest`：object `id=4`
  - `light(point)`：object `id=10`（`day_only=true`, `radius=64`）
- `assets/maps/town.tmj`
  - `light(spot)`：object `id=23`（`night_only=true`, `spot={radius:64, inner_deg:25, outer_deg:45, direction_deg:90}`）
  - `map_trigger`：object `id=26`（`target_map="school"`, `start_offset="bottom"`）

## 5) 可观测与排错

### 5.1 Debug 面板（推荐）
- `Map Inspector`：当前地图概览、`MapLoadingSettings` 与预加载状态、tile flags 分布、对象数量、unknown token 报告、JSON cache 状态

入口：`GameScene::registerDebugPanels()`。

### 5.2 常见问题清单
1. **瓦片阻挡没生效**
   - 检查 tileset 的 `tile_flag` 是否拼写正确（未知 token 会在 `Map Inspector → Unknown Tokens → tile_flag` 和日志中出现）。
2. **预加载模式写错导致“突然全量预热”**
   - `MapLoadingSettings` 对未知 `preload.mode` 会 warn 并回退为 `off`（更保守）。
3. **关卡对象没生成**
   - 检查 object 的 `type/name/point` 是否符合约定（未知 `object.type`/`light.name` 会出现在 `Map Inspector → Unknown Tokens`）。
4. **切图后读盘变慢**
   - `.tmj/.tsj` JSON 有全局缓存（可在 `Map Inspector → Tiled JSON Cache` 查看/清理）。

## 6) 扩展点（该改哪里）
- 新增 tile 语义：优先通过 `tile_flag` 扩展 `TileType`（引擎层），并在玩法系统消费（游戏层）。
- 新增 object type：改 `src/game/loader/entity_builder.cpp`（游戏层扩展点）。
- 新增调试与验证：优先加 Debug 面板 + 结构化报告，让“加载”变成可观测系统。
