## 一、现状速览

| 维度 | 现状 |
|------|------|
| 入口/导航 | [README](docs/README.md) + [learning-path](docs/tutorial/learning-path.md) 完整，分类 README 齐全 |
| 引擎层 | 18 篇，覆盖启动/循环/ECS/输入/资源/渲染/UI/VFX/音频/空间/文本/调试 |
| 游戏层 | 18 篇，覆盖 GameScene/调度/装配/数据目录/地图/世界/交互/农场/存档 |
| 玩法层 | 6 篇，覆盖任务/商店/队伍装备/外观/设置/战斗 |
| 教程 | learning-path、debugging、Lua 双指南、14 篇多线程系列 |
| 与代码同步度 | P0 类过时问题已修；少数 Feb 旧文档可能有局部漂移 |

## 二、高优先级（过时或重要缺失）

### A. 与代码可能漂移的旧文档（mtime ≤ Feb / Apr，code May 27 改过）

需要逐篇 spot-check 后局部更新：

- [engine/ecs.md](docs/engine/ecs.md)（Feb）— ECS 约定是基石，必须与现状一致
- [engine/scenes.md](docs/engine/scenes.md)（Feb）— Scene 栈 + 覆盖式场景
- [engine/resources.md](docs/engine/resources.md)（Feb）— ResourceManager 改过否
- [engine/audio_system.md](docs/engine/audio_system.md)（Feb）— 与 `AudioCueCatalog` 的关系
- [engine/spatial_index.md](docs/engine/spatial_index.md)（Feb）
- [engine/loop_timing_contract.md](docs/engine/loop_timing_contract.md)（Feb）
- [game/system_scheduler.md](docs/game/system_scheduler.md)（Feb，14KB）— 调度器是核心
- [game/blueprints.md](docs/game/blueprints.md)（Feb）— 与 `src/game/factory/` 现状
- [game/world_state.md](docs/game/world_state.md)（Feb）
- [game/time_and_lighting.md](docs/game/time_and_lighting.md)（Feb）
- [game/async_preload_pipeline.md](docs/game/async_preload_pipeline.md)（Feb）
- [game/inventory_hotbar.md](docs/game/inventory_hotbar.md)（Apr）
- [engine/input_system.md](docs/engine/input_system.md)（Apr，26KB 偏大）

### B. 明显缺失的"模块入门"文档

对应课程大纲指名而 docs 没单独成篇的：

- **`docs/game/domain-services.md`** — `src/game/domain/` 下 8 个 service（Inventory/Equipment/Quest/Shop/PartyRest/ActorProgression/QuestTurnIn/QuestBattleProgress）是 L03 的核心，目前只在 overview 一笔带过
- **`docs/engine/script_host.md`** — 引擎脚本宿主内核（`src/engine/script/` 7 个文件），现有 Lua 文档讲的是"绑定"和"内容编写"，少了"宿主架构"这一层
- **`docs/game/battle-internals.md`** — `src/game/battle/` 有 21 个文件（TurnCore/Session/ActionResolver/AIPlanner/Formula/RewardResolver/UnitFactory…），目前都压在一篇 25KB 的 [turn-based-battle.md](docs/gameplay/turn-based-battle.md) 里，学生很难按需切入
- **`docs/game/audio_cue_catalog.md`** — 课程 L10 明确要讲"数据驱动音频"，但 docs 里没有
- **`docs/build_and_run.md`** — CMakePresets/Ninja/平台依赖目前只散落在 [debugging.md](docs/tutorial/debugging.md)，缺独立的"如何把项目跑起来"

### C. 课程↔文档映射

- **`docs/tutorial/course-mapping.md`**（或在 `learning-path.md` 里加一节）— 把 [tinyfarmrpg-course-outline.md](lecture_plans/tinyfarmrpg-course-outline.md) 的 L01–L27 每讲映射到对应 docs，学生看完一讲能立刻找到深读材料

## 三、中优先级（教学体验增强）

- **"动手做"型教程缺位**：现有 docs 多是参考型，缺 step-by-step：
  - `tutorial/howto-add-npc.md`（含 Tiled + Lua + blueprint）
  - `tutorial/howto-add-quest.md`
  - `tutorial/howto-add-item-and-shop.md`
  - `tutorial/howto-add-skill-and-enemy.md`
- **代码走读**：[learning-path.md](docs/tutorial/learning-path.md) 给了顺序，但没有"打开 GameScene.cpp 逐段读"的导览。可加 `tutorial/code-walkthrough-game-scene.md`
- **设计决策记录**：为什么用 Scene 栈+覆盖、为什么 Lua 只做内容、为什么 domain service 集中写入。可加 `docs/design-decisions.md`（小型 ADR）

## 四、低优先级

- `docs/testing/` 只覆盖 UI 回归；可补 `testing/unit-and-integration.md` 说明 `tests/` 目录怎么跑、怎么加 case
- `engine/input_system.md` 26KB 偏长，可拆分（鉴于 L06 要单独讲输入）
- `config/` 目录（窗口/输入/渲染/音频/文本）没有索引文档
- `assets/data/` 文件结构可在 [data-catalogs.md](docs/game/data-catalogs.md) 末尾加一张"文件树+各 json 用途"

---

## 建议执行路径

如果你认同上面的清单，我建议**分两步走**：

1. **第一步（高 ROI）**：A 类逐篇 spot-check + B 类前 3 篇新增（domain-services / script_host / battle-internals）
2. **第二步**：B 类剩余 + C 类映射文档；中优先级看时间