# 蓝图系统（Blueprints）约定与速查

> 目标：让“加新内容主要改数据，而不是改核心逻辑”这件事可落地、可排错、可扩展。

## 1) 数据文件（制作侧）
- 角色/NPC：`assets/data/actor_blueprint.json`
- 动物：`assets/data/animal_blueprint.json`
- 作物：`assets/data/crop_config.json`
- 资源映射（音效 key → 文件路径）：`assets/data/resource_mapping.json`

## 2) 运行时主链路（启动期加载 → 运行期装配）
核心心智模型：
- **BlueprintManager**：把 JSON 解析成“可查询的蓝图表”（`id → blueprint struct`）
- **EntityFactory**：把蓝图装配成 ECS 实体（组件组合集中在工厂）
- **调用点**：
  - 加载期：`EntityBuilder::buildActor/buildAnimal`（map object 的 `name` → blueprint key）
  - 玩法期：`FarmSystem::plantSeed`（调用 `EntityFactory::createCrop`）

建议定位入口：
- `src/game/scene/game_scene.cpp`（`ensureBlueprintManager()` / `initFactory()`）
- `src/game/factory/blueprint_manager.h/.cpp`
- `src/game/factory/entity_factory.h/.cpp`

## 3) Schema 约定（最小集）

### 3.1 `actor_blueprint.json`（顶层 object map）
形态：
```json
{
  "<actor-id>": { "...fields..." },
  "<actor-id-2>": { "...fields..." }
}
```
常用字段（按重要性）：
- `sprite`（必需）：静态精灵（站立显示）
- `animations`（必需）：动画表（idle/walk/hoe/...）
- `speed`（可选，默认 100）
- `sounds`（可选）：动作音效触发配置（见 3.4）
- `wander_radius`（可选，默认 0；>0 表示 NPC 会游走）
- `dialogue_id`（可选）：对话脚本 id
- `interact_distance`（可选，默认 80）

### 3.2 `animal_blueprint.json`（顶层 object map）
与 actor 基本一致，额外字段：
- `sleep_at_night`（可选，默认 true）

### 3.3 `crop_config.json`（数组风格）
形态：
```json
{
  "crops": [
    {
      "type": "<crop-type>",
      "stages": [ { "stage": "...", "days_required": 1, "sprite": { ... } } ],
      "harvest_item_id": "<item-id>"
    }
  ]
}
```
要点：
- `type` 需要能被 `game::defs::cropTypeFromString()` 识别，否则会被跳过并 warn。
- `stages[].stage` 需要能被 `game::defs::growthStageFromString()` 识别。

### 3.4 `sprite` 与 `animations` 的最小结构
`sprite`（必需字段）：
- `path`（string，资源路径）
- `src_size.width/height`（number）

可选字段：
- `position.x/y`（默认 0）
- `dst_size.width/height`（默认 = src_size）
- `pivot.x/y`（默认 0）
- `flip_horizontal`（默认 false）

`animations.<anim>`（最小字段）：
- `path`
- `position.x/y`
- `src_size.width/height`
- `frames`（int array）
- `duration`（ms per frame）
- `direction`（string array，可选）

## 4) 命名约定（非常重要）

### 4.1 Blueprint key：谁来用？
- 地图对象（Tiled object layer）：
  - `type="actor"` / `type="animal"`
  - `name="<blueprint-key>"`
- 运行时：`EntityBuilder` 会把 `name` 哈希成 `entt::id_type`，作为 `BlueprintManager` 的查询 key。

### 4.2 动画 id：`<anim>_<dir>` + 自动镜像
当动画条目包含：
```json
"direction": ["down", "up", "right"]
```
解析后会生成：
- `idle_down / idle_up / idle_right`
- 若缺少 `left`，会用 `right` 自动生成镜像 `left`（反之亦然）

注意：`EntityFactory` 默认会用 `idle_down` 作为初始动画（因此至少要能生成该 id）。

### 4.3 音效 key：必须存在于 `resource_mapping.json`
`sounds.<trigger>.sound` 的值是“语义 key”（例如 `cow_moo`）：
- 需要在 `assets/data/resource_mapping.json` 的 `sound` 段落中有对应条目
- 否则运行时无法映射到真实音频文件（可用 Debug 面板快速定位）

## 5) 扩展步骤（常见）

### 5.1 加一个 NPC / 动物（纯数据驱动）
1. 在 `actor_blueprint.json` / `animal_blueprint.json` 添加新条目（key 与 name 建议一致）
2. 在地图 `.tmj` 对应 object layer 放一个 point object：
   - `type="actor"` 或 `type="animal"`
   - `name="<你刚刚新增的 key>"`
3. 确认资源文件路径存在（贴图）+ 音效 key 已映射（如有 sounds）

### 5.2 加一个新字段（需要改代码）
1. 在 `src/game/factory/blueprint.h` 增加字段
2. 在 `src/game/factory/blueprint_manager.cpp` 解析并填充字段
3. 在 `src/game/factory/entity_factory.cpp` 装配到组件（或供系统消费）
4. 补全本文件文档 + 示例（避免 schema 漂移）

## 6) 排错 Checklist（推荐顺序）
1. **启动期加载失败**：看日志（通常会提示哪个配置文件无法打开 / JSON 解析失败）
2. **实体没生成**：检查 map object 的 `type/name`；再用 Debug 面板查 blueprint 是否存在
3. **贴图缺失**：检查 `path` 是否存在于磁盘
4. **音效不触发/无声**：检查 `sounds.*.sound` 是否在 `resource_mapping.json/sound` 中

## 7) Debug 面板
- `Blueprint Inspector`：已加载蓝图数量、按 key 查询、缺失资源引用统计
- 入口：`GameScene::registerDebugPanels()`
