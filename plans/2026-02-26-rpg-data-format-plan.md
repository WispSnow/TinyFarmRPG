# RPG 数据格式重构计划（整合版）

## 元信息
- 日期：`2026-02-26`
- 状态：`Planned`
- 范围：`仅数据格式与加载链路，不改战斗结算规则`
- 输入参考：
1. `for_agent/ref/data/*`（RPGMaker 参考数据）
2. `plans/tmp/rpg_data_format_plan.md`（Claude Code 草案）

## 1. 已确认决策

1. RPGMaker 数据只作为参考或一次性导入来源，不作为长期运行时格式。
2. 导入流程必须是独立工具，不放入游戏主程序运行路径。
3. 项目内部 schema 不要求与 RPGMaker 一一对应，可按项目需求精简或扩展。
4. 计划文档统一放在 `plans/`，不再放在 `docs/plans/`。

## 2. 对 Claude 草案的综合分析

可直接吸收的内容：

1. 统一类型层思路：`rpg_types` + `rpg_data` + `rpg_catalog`。
2. 强类型枚举、Trait/Effect 结构化建模、统一查表入口。
3. 补齐 catalog 与公式相关单测的测试优先策略。

需要调整的内容：

1. 将“RPGMaker -> 项目数据”的逻辑从 Runtime 移出，改为 `tools/` 独立导入工具。
2. 运行时加载的目标格式改为 `assets/data/rpg/*`（项目主数据），而非直接依赖导入输入形态。
3. 首阶段不改 `battle_types`、`item_catalog` 的行为结构，避免“数据格式任务”变成“战斗重构任务”。

暂缓项（后续阶段）：

1. `BattleActionType` 大扩展（Skill/Item/Guard/Escape）。
2. `BattleUnit` 8 属性化落地到战斗核心逻辑。
3. 公式执行在战斗结算中的全量接入。

## 3. 目标架构

采用三层单向模型：

1. `Reference Layer`：`for_agent/ref/data/*`（只读参考输入）
2. `Domain Layer`：`assets/data/rpg/*`（运行时唯一静态真源）
3. `Save Layer`：`game::save`（仅动态进度，引用 Domain ID）

固定数据流：

`RPGMaker JSON -> tools/rpg_importer -> assets/data/rpg -> RpgCatalog -> Gameplay Systems`

## 4. 数据目录与格式约定

主目录：`assets/data/rpg/`

首批文件：

1. `manifest.json`
2. `actors.json`
3. `classes.json`
4. `skills.json`
5. `states.json`
6. `items.json`
7. `weapons.json`
8. `armors.json`
9. `enemies.json`
10. `troops.json`
11. `quests.json`（可先空）
12. `shops.json`（可先空）

统一规则：

1. 主键全部使用语义字符串 ID（示例：`skill.fireball_lv1`）。
2. 禁止稀疏数组和 `null` 占位 ID。
3. 运行时可做 `entt::hashed_string` 转换，但 JSON 中保留字符串引用。
4. 所有跨表引用在加载阶段做完整校验并 fail-fast。

## 5. 工程拆分

### 5.1 运行时（游戏程序）

新增文件：

1. `src/game/data/rpg_types.h`
2. `src/game/data/rpg_types.cpp`
3. `src/game/data/rpg_data.h`
4. `src/game/data/rpg_catalog.h`
5. `src/game/data/rpg_catalog.cpp`

职责：

1. 只加载 `assets/data/rpg/*`。
2. 不关心 RPGMaker 源文件细节。
3. 提供统一 `findXxx` 查询接口与验证错误报告。

### 5.2 导入工具（独立）

新增目录建议：`tools/rpg_importer/`

输出目标：

1. `assets/data/rpg/*.json`
2. `assets/data/rpg/import_report.json`
3. `assets/data/rpg/validation_report.json`

职责：

1. 读取 `for_agent/ref/data/*`。
2. 映射为项目 schema（允许精简与扩展）。
3. 对无法自动映射项给出告警与 TODO 标记。

## 6. 分阶段实施计划

### Phase 0：Schema 冻结与契约落盘

任务：

1. 定义 `manifest` 字段与模块清单。
2. 定义 `rpg_types` 枚举集合及字符串转换规则。
3. 定义 `rpg_data` 核心结构（Class/Skill/State/Enemy/Troop/Item 等）。

产出：

1. 本计划文档（当前）
2. `assets/data/rpg/` 文件模板（空内容可）

### Phase 1：Runtime Catalog 基础能力

任务：

1. 实现 `RpgCatalog` 多文件加载与基础索引。
2. 加入结构校验、语义校验、引用校验。
3. 新增单测：加载成功/缺字段/非法引用/重复 ID。

产出：

1. `src/game/data/rpg_*` 文件
2. `tests/game/rpg_catalog_test.cpp`

说明：

1. 本阶段不改战斗行为，不改 ItemCatalog 业务语义。

### Phase 2：独立导入工具

任务：

1. 在 `tools/rpg_importer` 实现一次性导入 CLI。
2. 完成 RPGMaker 主要表映射（Actors/Classes/Skills/States/Items/Weapons/Armors/Enemies/Troops/System）。
3. 生成 `import_report.json` 与 `validation_report.json`。

产出：

1. 可重复执行的导入命令
2. 可审计导入报告

### Phase 3：战斗数据接线（仅“读数据”，不改算法）

任务：

1. 把现有战斗样例数据切换为 `RpgCatalog` 读取。
2. 保持当前 `BattleSession/TurnCore` 行为不变。
3. 新增最小集成测试，验证“配置变更可驱动战斗入参”。

产出：

1. 战斗入口不再硬编码样本单位
2. 集成测试覆盖基础读取链路

### Phase 4：功能扩展（后续任务，不在本计划内实现）

预留方向：

1. 技能/物品动作类型扩展
2. 8 参数体系接入战斗公式
3. 任务与商店状态写入 `quest_state/combat_state`

## 7. 验收标准（DoD）

1. 游戏运行时不读取 `for_agent/ref/data/*`。
2. 导入流程可独立执行，不依赖游戏启动过程。
3. `assets/data/rpg/*` 可通过完整校验并被 `RpgCatalog` 加载。
4. 缺失引用/非法枚举/重复 ID 会在加载期明确报错。
5. 存档层仅保存动态状态，不复制静态规则配置。

## 8. 风险与缓解

风险：

1. RPGMaker 部分字段语义复杂，自动映射不完整。
2. 早期过度追求“完整兼容”导致 schema 被历史包袱反向塑形。
3. 数据格式任务膨胀成战斗系统重构，影响交付节奏。

缓解：

1. 导入报告显式标注“已映射/跳过/需人工处理”。
2. 以项目玩法需求驱动 schema，而非对齐外部工具字段。
3. 严格按 Phase 切分，先数据再行为。

## 9. 下一步执行建议

1. 先落 Phase 1：`rpg_types/rpg_data/rpg_catalog` 与测试。
2. 再做 Phase 2：独立导入工具与报告。
3. 最后接 Phase 3：战斗入口改为读 catalog。
