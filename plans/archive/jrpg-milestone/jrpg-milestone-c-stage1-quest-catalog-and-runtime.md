# Milestone C / Stage 1: Quest 目录与运行时真相细化计划

## 实现思路

- 新增独立 `QuestCatalog`，不并入 `RpgCatalog`；静态定义统一放在 `assets/data/quests.json`。
- `QuestCatalog` 只负责静态数据加载、查表和引用校验；运行时接取状态、完成状态、progress 不进 catalog。
- objective MVP 先只支持 `DefeatEnemyCount`；`QuestObjectiveData` 采用 `flat struct + kind tag`，不提前引入 `variant` 或继承层次。
- `objective_progress` 保持现有 flat map，不改 save schema；统一通过 `makeQuestObjectiveProgressKey(quest_id, objective_id)` 生成复合 key。
- 复合 key 的保留分隔符固定为 `::`；`quest_id` 和 `objective_id` 都禁止包含该分隔符。
- 玩家实体上的 `QuestLogComponent` 作为唯一 runtime truth，持有：
  - `active_quests`
  - `completed_quests`
  - `objective_progress`
- `EntityFactory::createActor("player")` 直接初始化 `QuestLogComponent`，不增加额外 bootstrap service。
- `SaveService::capture()/apply()` 直接桥接 `QuestLogComponent` 与 `SaveData::quest_state`；`capture()` 把 `QuestLogComponent` 视为硬依赖。
- `GameRuntimeServices` 新增 `quest_catalog`，由 `GameRuntimeAssembler` 统一装配，并在装配时完成 `RpgCatalog + ItemCatalog` 引用校验。
- 本阶段不包含 `QuestGiverComponent`、`QuestInteractionSystem`、battle progress、turn-in、quest UI。

## 需要新增的文件

- `src/game/data/quest_data.h`
- `src/game/data/quest_catalog.h`
- `src/game/data/quest_catalog.cpp`
- `src/game/component/quest_log_component.h`
- `assets/data/quests.json`
- `tests/game/quest_catalog_test.cpp`

## 实现步骤

### Step 1. 定义 Quest 静态数据模型

- 在 `quest_data.h` 中定义 `QuestObjectiveKind`、`QuestObjectiveData`、`QuestRewardItemData`、`QuestRewardData`、`QuestGiverTextData`、`QuestData`。
- `QuestObjectiveData` 先只覆盖 `DefeatEnemyCount` 所需字段：`id / kind / enemy_id / required_count`。
- 同时提供 progress key helper，并锁定 `::` 为保留分隔符。

### Step 2. 实现 QuestCatalog

- `QuestCatalog` 提供 `loadFromFile / findQuest / listQuests / validateReferences / hashId`。
- 加载阶段校验：
  - `schema_version`
  - quest id / objective id 非空
  - 同一 quest 下 objective id 不重复
  - quest id / objective id 不包含保留分隔符
  - `required_count > 0`
- 引用校验阶段验证：
  - `enemy_id` 能在 `RpgCatalog` 中查到
  - 奖励 `item_id` 能在 `ItemCatalog` 中查到

### Step 3. 建立玩家运行时真相

- 新增 `QuestLogComponent` 并挂到玩家实体。
- 组件保持最小，只持有 `active_quests / completed_quests / objective_progress`。
- 不把 quest 状态塞进 `ActorComponent`，也不新增独立 quest runtime service。

### Step 4. 接通存档桥接

- `SaveService::capture()` 从玩家 `QuestLogComponent` 写回 `quest_state`。
- `SaveService::apply()` 用 `quest_state` 覆盖玩家 `QuestLogComponent`。
- `capture()` 缺少 `QuestLogComponent` 时直接失败；`PlayerWalletComponent` 现有软回退差异不在本阶段统一。

### Step 5. 接入 Runtime 装配

- `GameRuntimeServices` 新增 `quest_catalog`。
- `GameRuntimeAssembler` 新增 `ensureQuestCatalog()`，统一加载 `assets/data/quests.json` 并完成引用校验。
- 构建脚本补上 `quest_catalog.cpp` 与对应测试文件。

### Step 6. 补齐测试

- `QuestCatalog` 覆盖成功加载、重复 id、保留分隔符、缺失 `title`、非法 `description`、坏引用。
- `EntityFactory` 覆盖玩家自动带 `QuestLogComponent`。
- `SaveService` 覆盖 `quest_state` 写出、读档恢复、缺失 `QuestLogComponent` 时失败。
- runtime assembly source test 覆盖 `ensureQuestCatalog()` 接线存在。

## ToDo

- [x] 新增 Quest 静态数据模型与 progress key helper
- [x] 新增 `QuestCatalog` 与 `assets/data/quests.json`
- [x] 锁定 `objective_progress` 复合 key 规则
- [x] 新增 `QuestLogComponent` 并挂到玩家实体
- [x] 让 `SaveService::capture()/apply()` 直接桥接 quest runtime truth
- [x] 在 `GameRuntimeServices / GameRuntimeAssembler` 接入 `quest_catalog`
- [x] 补齐 QuestCatalog、EntityFactory、SaveService、runtime assembly 相关测试
