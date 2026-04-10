# Milestone C / Stage 1: Quest 目录与运行时真相细化计划

## Context

Milestone C 的后续阶段都依赖同一个基础：任务必须先从“save schema 预留字段”升级成“有静态定义、有运行时 owner、有装配入口”的真实系统。

当前代码里已经有一半地基，但还没有形成闭环：

- `SaveData::quest_state` 已经预留了
  - `active_quests`
  - `completed_quests`
  - `objective_progress`
- `save_data.cpp` 与 `save_migrator.cpp` 已能序列化/反序列化这些字段
- 玩家实体已经通过 `EntityFactory::createActor("player")` 挂载核心探索态组件
  - `PlayerTag`
  - `InventoryComponent`
  - `HotbarComponent`
  - `PlayerWalletComponent`
- `GameRuntimeServices` 当前已经统一持有各种目录对象，例如 `ItemCatalog / AppearanceCatalog / RpgCatalog`

但当前仍有 4 个关键断点：

### 1. 没有 Quest 静态目录

当前项目没有 `QuestCatalog`，也没有 `assets/data/quests.json` 一类的数据入口。

这意味着后续阶段无法稳定定义：

- quest 的 `id / title / description`
- objective 的 `kind / target / required_count`
- 后续 turn-in 奖励
- quest giver 绑定的 quest id

如果 Stage 1 不先把 quest 数据模型和加载边界锁定，Stage 2 的 quest giver、Stage 3 的 battle progress 都只能临时硬编码。

### 2. 没有 Quest 运行时真相

虽然 `SaveData` 已有 `quest_state`，但当前运行时没有任何组件或 service 真正持有 quest 真相。

这会导致：

- 交互系统无法判断玩家是否已接某任务
- 战斗结算无法推进 objective progress
- UI 无法读取 active/completed quest
- `SaveService::capture()` 实际上也没有 runtime source 可读

也就是说，当前的 `quest_state` 只是 schema 占位，不是运行时系统。

### 3. `objective_progress` 还没有稳定 key 规则

`SaveData::QuestStateSaveData::objective_progress` 当前是一个扁平的 `unordered_map<string, int>`。

如果 Stage 1 不先锁定 key 规则，后面很容易出现冲突：

- 同一个 quest 下多个 objective 重名
- 不同 quest 恰好复用同名 `objective_id`
- UI 排序或配置重构后，旧 progress key 不再可恢复

因此 Stage 1 必须明确：progress key 不是裸 `objective_id`，而是稳定复合 key。

### 4. Runtime services 里还没有 QuestCatalog 装配位

当前 `GameRuntimeServices` 还没有 quest 目录对象，`game_runtime_assembler.cpp` 里也没有 `ensureQuestCatalog()`。

如果 Stage 1 不把这条装配链补齐，后续 Stage 2/3 就只能在系统里自己读 JSON 或持有散落的配置对象，这会破坏现有 catalog 生命周期边界。

因此，Stage 1 的目标不是去实现 quest giver 或 battle progress，而是先补齐 4 件基础设施：

- Quest 静态目录
- Quest 运行时真相
- 稳定 progress key 规则
- QuestCatalog 的 runtime 装配入口

## 范围

### 本阶段包含

- Quest 数据模型定义
- `QuestCatalog` 的加载与查表边界
- `assets/data/quests.json` 的最小配置结构
- `objective_progress` 的稳定复合 key 规则
- 玩家实体上的 `QuestLogComponent`
- `SaveService::capture()/apply()` 对 quest runtime truth 的桥接
- `GameRuntimeServices` / `game_runtime_assembler.cpp` 中的 `QuestCatalog` 装配
- 对应的测试补强方案

### 本阶段不包含

- `QuestGiverComponent`
- `QuestInteractionSystem`
- battle -> quest 击败计数推进
- turn-in、completed 状态迁移
- quest UI
- quest reward 写回

## 实现思路

### 1. QuestCatalog 采用独立单文件目录，而不是并入 RpgCatalog

推荐新增：

- `src/game/data/quest_data.h`
- `src/game/data/quest_catalog.h`
- `src/game/data/quest_catalog.cpp`
- `assets/data/quests.json`

推荐数据边界：

- Quest 数据独立于 `RpgCatalog`
- `QuestCatalog` 只负责静态数据加载、查表、引用校验
- 运行时接取状态、完成状态、progress 不放进 catalog

推荐最小配置结构：

```json
{
  "schema_version": 1,
  "quests": [
    {
      "id": "quest.village.goblin_cleanup",
      "title": "Goblin Cleanup",
      "description": "Defeat goblins near the village.",
      "objectives": [
        {
          "id": "kill_goblins",
          "kind": "defeat_enemy_count",
          "enemy_id": "enemy.goblin",
          "required_count": 3
        }
      ],
      "rewards": {
        "gold": 50,
        "items": [
          { "item_id": "potion", "count": 2 }
        ]
      },
      "giver_text": {
        "offer": "Can you help us drive away the goblins?",
        "progress": "We still need more help.",
        "ready_to_turn_in": "You did it? That's a relief.",
        "completed": "Thank you again."
      }
    }
  ]
}
```

本阶段只锁定数据结构，不要求立刻消费全部字段。

原因：

- quest 本质上跨交互、战斗、UI、存档多个子系统，不应耦合进 `RpgCatalog`
- 当前项目里 `ItemCatalog`、`AppearanceCatalog` 已经证明“独立目录 + runtime service 持有”是稳定模式
- Stage 1 先用单文件最稳，后续 quest 数量变大时再拆 manifest 也更可控

### 2. Quest 数据模型先只支持 MVP 需要的字段

推荐在 `quest_data.h` 中定义最小结构：

- `QuestObjectiveKind`
- `QuestObjectiveData`
- `QuestRewardItemData`
- `QuestRewardData`
- `QuestGiverTextData`
- `QuestData`

推荐约束：

- 本阶段只支持 `QuestObjectiveKind::DefeatEnemyCount`
- `QuestObjectiveData` 在 C++ 中采用“flat struct + kind tag”的最简表达，不提前引入 `std::variant`、继承树或额外 objective 子类型容器
- 推荐字段直接收敛在 `QuestObjectiveData` 内，例如 `id / kind / enemy_id / required_count`
- `QuestData` 保留 `rewards` 和 `giver_text`，即使 Stage 4/Stage 2 才真正消费
- quest / objective 都保留原始 string id
- catalog 内部同时缓存 `entt::id_type` hash 以复用现有查表风格
- quest id 与 objective id 都禁止包含 progress key helper 使用的保留分隔符

推荐的最小 C++ 表达：

```cpp
struct QuestObjectiveData {
    std::string id{};
    QuestObjectiveKind kind{QuestObjectiveKind::DefeatEnemyCount};
    std::string enemy_id{};
    int required_count{0};
};
```

推荐接口：

- `QuestCatalog::loadFromFile(std::string_view file_path)`
- `QuestCatalog::findQuest(std::string_view id) const`
- `QuestCatalog::findQuest(entt::id_type id_hash) const`
- `QuestCatalog::listQuests() const`
- `QuestCatalog::validateReferences(const RpgCatalog*, const ItemCatalog*, std::string& out_error) const`
- `QuestCatalog::hashId(std::string_view id)`

原因：

- 后续 Stage 2 会用 `quest_id` 做 giver 绑定
- 后续 Stage 3 会用 objective 的 `enemy_id` 与 `source_enemy_id` 对接
- 后续 Stage 4 会消费奖励字段
- 既然 Milestone C 当前只有一种 objective，flat struct 比 variant/继承更轻、更稳，也更符合“先做最优最小方案”的要求
- 现在一次把静态结构立稳，后面各阶段就不需要反复改 schema

### 3. `objective_progress` 统一使用复合 key helper

推荐新增一个轻量 helper，统一生成 progress key：

- 逻辑语义：`makeQuestObjectiveProgressKey(quest_id, objective_id)`

推荐规则：

- key 由 `(quest_id, objective_id)` 组成
- 使用保留分隔符，例如 `quest_id + "::" + objective_id`
- quest id 与 objective id 都禁止包含保留分隔符
- 所有业务代码禁止手写字符串拼接，必须走统一 helper

推荐原因：

- `SaveData::quest_state.objective_progress` 已经是 flat map，没必要在 Stage 1 改 schema
- 复合 key 能彻底避免多 quest / 多 objective 的命名冲突
- helper 统一后，后续 domain / save / UI 都不会各自发明一套 key 规则

本阶段不建议改成：

- `unordered_map<quest_id, unordered_map<objective_id, int>>`
- `objective_id` 全局唯一约束

原因：

- 第一种要改 save schema，收益不够
- 第二种把“全局唯一”的负担强行压给配置作者，不如复合 key 稳

因此，`QuestCatalog::loadFromFile()` 的校验项必须显式包含：

- quest id 非空
- objective id 非空
- 同一 quest 下 objective id 不重复
- quest id 不包含保留分隔符
- objective id 不包含保留分隔符

### 4. Quest 运行时真相挂在玩家实体上

推荐新增：

- `src/game/component/quest_log_component.h`

推荐结构保持最小：

```cpp
namespace game::component {

struct QuestLogComponent {
    std::vector<std::string> active_quests{};
    std::vector<std::string> completed_quests{};
    std::unordered_map<std::string, int> objective_progress{};
};

} // namespace game::component
```

设计约束：

- `QuestLogComponent` 是 quest progression 的唯一运行时真相
- 不扩成“大而全剧情 flag 容器”
- 不把 quest 状态塞进 `ActorComponent`
- active/completed 保留 `vector<string>`，保持接取顺序与 UI 展示顺序一致

原因：

- quest progression 和 inventory / wallet 一样，都属于探索态玩家真相
- 把 quest log 挂在 player entity 上，后续系统只需要找玩家实体即可
- 直接沿用 `SaveData::quest_state` 的数据形状，capture/apply 会更简单

### 5. 玩家创建路径直接初始化 QuestLogComponent

推荐在 `EntityFactory::createActor()` 的 `actor_name_id == "player"_hs` 分支中新增：

- `registry_.emplace<game::component::QuestLogComponent>(entity);`

让 player entity 天然具备：

- `PlayerTag`
- `InventoryComponent`
- `HotbarComponent`
- `PlayerWalletComponent`
- `QuestLogComponent`

推荐原因：

- 新开局时就有稳定 quest runtime owner，不需要额外 bootstrap 服务
- `SaveService::apply()` 可以直接覆盖 quest log，而不是判断是否首次创建
- 后续 `QuestInteractionSystem`、battle settlement、quest UI 都能默认 quest log 存在

### 6. SaveService 直接桥接 quest runtime truth，不改 schema

当前 `SaveData`、`save_data.cpp`、`save_migrator.cpp` 已经具备 quest schema。

因此 Stage 1 不需要修改：

- `src/game/save/save_data.h`
- `src/game/save/save_data.cpp`
- `src/game/save/save_migrator.cpp`
- `SAVE_SCHEMA_VERSION`

真正需要改的是 `SaveService::capture()` 与 `SaveService::apply()`。

#### `capture()` 推荐方案

- 读取玩家实体上的 `QuestLogComponent`
- 写入 `out.quest_state.active_quests`
- 写入 `out.quest_state.completed_quests`
- 写入 `out.quest_state.objective_progress`

推荐把 `QuestLogComponent` 视为硬约束：

- 若玩家缺少 `QuestLogComponent`，`capture()` 直接失败并写出 `out_error`

原因：

- quest progression 是玩家长期状态，不应像非关键 UI 缓存那样静默回退
- 既然 Stage 1 已经把 component 初始化责任放进 `EntityFactory::createActor("player")`，那 `SaveService` 就应该把它当作 invariant，而不是容忍丢失
- 当前项目允许自由重构，不需要为了兼容旧测试而保留危险的 silent fallback

已知风格差异：

- 当前 `PlayerWalletComponent` 在 `SaveService::capture()` 中仍是“缺失则写 `gold = 0` + warn”的软回退
- 这与 `QuestLogComponent` 的硬失败策略、以及 `InventoryComponent / HotbarComponent` 的硬失败策略不一致
- 该不一致是 Milestone B 遗留问题，不属于 Stage 1 直接修改范围，但后续应统一保存链路的 invariant 风格

#### `apply()` 推荐方案

在定位到 player entity 后：

- `emplace_or_replace<QuestLogComponent>(player, QuestLogComponent{...})`

恢复内容：

- `active_quests`
- `completed_quests`
- `objective_progress`

说明：

- `apply()` 继续负责“save model -> runtime truth”桥接
- 本阶段不需要在 `apply()` 时额外触发 quest UI 或 quest notification
- 也不需要在 `apply()` 时做 catalog 级合法性过滤；配置和存档合法性由 loader/validation 保证

### 7. QuestCatalog 生命周期由 GameRuntimeServices 托管

推荐修改：

- `src/game/runtime/system_bundle.h`
- `src/game/runtime/game_runtime_assembler.cpp`

新增：

- `std::shared_ptr<game::data::QuestCatalog> quest_catalog;`

推荐在 assembler 中新增：

- `ensureQuestCatalog(game::runtime::GameRuntimeServices& services)`

推荐调用顺序：

1. `ensureItemCatalog()`
2. `ensureAppearanceCatalog()`
3. `ensureVfxCatalog()`
4. `ensureRpgCatalog()`
5. `ensureQuestCatalog()`

QuestCatalog 装配流程：

- 创建 `QuestCatalog`
- `loadFromFile("assets/data/quests.json")`
- `validateReferences(services.rpg_catalog.get(), services.item_catalog.get(), out_error)`

原因：

- `DefeatEnemyCount` objective 需要校验 `enemy_id`
- 奖励里的 item 需要校验 `item_id`
- QuestCatalog 与其他 catalog 一样由 runtime services 生命周期托管，RAII 边界最清晰

### 8. 测试先覆盖目录、runtime truth 和 save bridge

推荐新增/修改的测试：

- 新增 `tests/game/quest_catalog_test.cpp`
  - quest 文件加载成功
  - 重复 quest id 失败
  - 重复 objective id 在同一 quest 下失败
  - quest id 或 objective id 包含保留分隔符时加载失败
  - 未知 `enemy_id` / `item_id` 的引用校验失败
  - progress key helper 生成稳定复合 key
- 修改 `tests/game/save_service_async_test.cpp`
  - capture 能写出 quest_state
  - apply 能恢复 quest_state 到 `QuestLogComponent`
- 新增 `tests/game/entity_factory_player_quest_log_test.cpp`
  - `createActor("player")` 会初始化 `QuestLogComponent`

关于测试文件组织：

- 当前 `tests/game` 下没有现成的 `entity_factory` 专项测试文件，只有 `blueprint_manager_smoke_test.cpp`
- 因此 Stage 1 先新增独立的 `entity_factory_player_quest_log_test.cpp` 是合理的
- 若后续出现更多 `EntityFactory` 相关断言，再合并为统一的 factory test 文件即可

本阶段不建议把测试重点放在：

- quest giver 交互
- battle 击败推进
- quest UI

原因：

- 这些都属于后续阶段
- Stage 1 最重要的是把“目录 + runtime truth + save bridge”打稳

## 需要新增的文件

- `assets/data/quests.json`
- `src/game/data/quest_data.h`
- `src/game/data/quest_catalog.h`
- `src/game/data/quest_catalog.cpp`
- `src/game/component/quest_log_component.h`
- `tests/game/quest_catalog_test.cpp`
- `tests/game/entity_factory_player_quest_log_test.cpp`

以下文件预计需要修改：

- `src/game/runtime/system_bundle.h`
- `src/game/runtime/game_runtime_assembler.cpp`
- `src/game/factory/entity_factory.cpp`
- `src/game/save/save_service.cpp`
- `tests/game/save_service_async_test.cpp`

## 实现步骤

### Step 1

定义 quest 数据模型与 `assets/data/quests.json` 的最小 schema。

说明：

- 锁定 `QuestData / QuestObjectiveData / QuestRewardData / QuestGiverTextData`
- 只支持 `DefeatEnemyCount`
- 锁定 progress key helper 与保留分隔符规则

### Step 2

实现 `QuestCatalog` 的加载、查表与引用校验。

说明：

- 负责 quest 文件读取
- 检查重复 id、非法字段、保留分隔符冲突和引用错误
- 提供后续阶段可复用的查询 API

### Step 3

新增 `QuestLogComponent` 并接入玩家创建路径。

说明：

- 玩家实体初始化时直接挂载 quest log
- 锁定它是唯一 runtime truth

### Step 4

把 `QuestCatalog` 接入 `GameRuntimeServices` 与 assembler。

说明：

- 新增 `quest_catalog` 服务位
- 新增 `ensureQuestCatalog()`
- 跟随 runtime 生命周期统一管理

### Step 5

让 `SaveService::capture()/apply()` 桥接 `QuestLogComponent` 与 `SaveData::quest_state`。

说明：

- capture 从 runtime truth 写入存档
- apply 从存档恢复 runtime truth
- 不改 schema version

### Step 6

补齐 Stage 1 回归测试。

说明：

- 覆盖目录加载
- 覆盖 player bootstrap
- 覆盖 save roundtrip

## ToDo

- [ ] 定义 quest 最小静态数据模型与 `quests.json` schema
- [ ] 实现 `QuestCatalog` 的加载、查表与引用校验
- [ ] 定义复合 progress key helper，并锁定保留分隔符规则
- [ ] 在 `QuestCatalog::loadFromFile()` 中校验 quest/objective id 不包含保留分隔符
- [ ] 新增 `QuestLogComponent`
- [ ] 在 `EntityFactory::createActor("player")` 中初始化 quest log
- [ ] 在 `GameRuntimeServices` 中新增 `quest_catalog`
- [ ] 在 `game_runtime_assembler.cpp` 中新增 `ensureQuestCatalog()`
- [ ] 在 `SaveService::capture()` 中写出 `quest_state`
- [ ] 在 `SaveService::apply()` 中恢复 `QuestLogComponent`
- [ ] 补充 quest catalog / player bootstrap / save roundtrip 测试

## 需要确认

- 暂无阻塞问题。
- 默认按“单文件 `assets/data/quests.json` + `QuestCatalog` 独立加载”的方案推进。
