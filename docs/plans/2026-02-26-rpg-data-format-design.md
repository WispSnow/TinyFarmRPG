# RPG 数据格式重构设计（方案 2：领域化 JSON）

日期：2026-02-26  
状态：Approved（设计已确认）

## 1. 背景与目标

项目后续将扩展 JRPG 玩法（商店、任务线、回合制战斗、技能系统）。  
当前已引入 RPGMaker 初始项目数据（`for_agent/ref/data`）作为参考，但不希望继续被其编辑器数据结构绑定。

本设计目标：

1. 以 RPGMaker 数据为一次性输入，构建项目自有、可扩展的数据格式。
2. 运行时仅依赖项目内部 schema，不直接读取 RPGMaker JSON。
3. 保持“静态配置”与“动态存档”解耦，降低后续功能扩展成本。

## 2. 约束与非目标

约束：

1. 项目是开发中程序，可采用最优方案，不考虑向后兼容历史外部格式。
2. 运行时保持 C++ 侧高效访问（可继续使用 `entt::hashed_string` 作为内部索引）。
3. 数据错误应在加载阶段 fail-fast，不延迟到玩法运行时暴露。

非目标：

1. 不做长期双向兼容 RPGMaker。
2. 不在本阶段引入复杂二进制 pack 流程（后续可升级）。
3. 不把 Tiled 地图事件命令流直接并入新 RPG 规则配置体系。

## 3. 总体方案（三层模型）

采用单向三层数据模型：

1. 参考层（Reference）
   - 路径：`for_agent/ref/data/*.json`
   - 角色：仅做导入输入快照，不参与运行时读取。
2. 领域层（Domain Data，主数据）
   - 路径：`assets/data/rpg/*.json`
   - 角色：游戏静态规则配置，运行时唯一真源。
3. 状态层（Save Data）
   - 路径：存档文件（`game::save`）
   - 角色：玩家进度与运行态，仅保存动态状态，不复制静态规则表。

数据流固定为：`RPGMaker JSON -> Importer -> Domain JSON -> Runtime Catalog -> SaveState Reference`。

## 4. 目录与文件组织

新增目录：`assets/data/rpg/`

建议文件拆分（按领域，不按 RPGMaker 文件名）：

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
11. `quests.json`（首版可为空结构）
12. `shops.json`（首版可为空结构）

`manifest.json` 用于声明：

1. `schema_version`
2. 子模块 `content_version`
3. 文件列表与加载顺序
4. 可选 feature flag（例如 `enable_quest`, `enable_shop`）

## 5. ID 与引用规范

### 5.1 ID 规范

1. 配置主键统一使用语义字符串 ID（例如 `skill.fireball_lv1`）。
2. 运行时可将字符串转换为 `entt::id_type` 做索引优化。
3. 禁止使用“数组下标即 ID”与稀疏数组 `null` 占位。

### 5.2 跨表引用规范

1. 所有引用字段显式保存字符串 ID（`class_id`, `skill_ids`, `drop.item_id`）。
2. 启动加载时执行全量引用校验。
3. 引用不存在即加载失败（fail-fast），并输出可定位报错。

## 6. RPGMaker 一次性导入映射策略

导入原则：保留玩法语义，丢弃编辑器噪音。

保留并结构化映射：

1. `Actors/Classes`：基础属性、成长、学习技能、初始装备。
2. `Skills/Items`：作用域、消耗、命中、效果列表。
3. `States`：持续回合、移除条件、属性修饰。
4. `Enemies/Troops`：基础属性、行动模式、掉落、编组关系。
5. `System`：元素类型、技能类型、装备类型等核心枚举定义。

降级或丢弃：

1. 稀疏数组空位、占位条目（如 `"-----保留"`）。
2. 纯编辑器 UI 文本与测试配置（对运行时无价值者）。
3. 未定义语义的 `note` 文本（除非后续明确解析规则）。

导入产物：

1. `assets/data/rpg/*.json`
2. `assets/data/rpg/import_report.json`（映射/跳过/告警统计）

## 7. 校验与错误处理

校验分三层：

1. 结构校验：字段类型、必填项、枚举值。
2. 语义校验：数值范围、互斥规则、列表唯一性。
3. 引用校验：跨表 ID 引用完整性。

错误处理规则：

1. 配置错误阻断加载，不做静默容错。
2. 日志需包含：文件名、对象 ID、字段路径、错误原因。
3. 导入阶段允许告警；运行时加载阶段仅允许“0 error”通过。

## 8. 与存档系统的边界

存档继续使用独立 `save_schema_version`（当前 `SAVE_SCHEMA_VERSION = 3`），与配置 schema 解耦：

1. 领域层（静态）变化不自动触发存档版本升级。
2. 仅在“存档解释语义”变化时升级存档 schema。
3. 存档中只保存对静态表的引用 ID 与进度数据，不冗余静态配置副本。

## 9. 加载链路（运行时）

建议加载顺序：

1. 读取 `manifest.json`
2. 加载基础枚举类数据（`states`, `skills`, `items`）
3. 加载角色与敌方生态（`classes`, `actors`, `enemies`, `troops`）
4. 加载玩法扩展数据（`quests`, `shops`）
5. 执行全量校验
6. 构建各 Catalog 并发布给系统

实现风格与现有 `ItemCatalog` 对齐：  
`load -> parse -> normalize -> validate -> index`。

## 10. 测试策略

1. 单元测试
   - 每个 Catalog 的解析、默认值、错误输入覆盖。
2. 数据一致性测试
   - 重复 ID、缺失引用、非法枚举、循环引用（如有）检测。
3. 集成测试
   - 最小战斗数据集可完整执行 `BattleSession` 核心流程。
4. 回归测试
   - 存档加载对静态配置引用稳定，拒绝无效 ID。

## 11. 分阶段落地建议

1. Phase 1：定义 `manifest + skills/states/enemies/troops` schema 与 loader。
2. Phase 2：完成 RPGMaker -> Domain 的一次性导入脚本与报告。
3. Phase 3：接入战斗系统读取 Domain 数据，替换硬编码 battle sample。
4. Phase 4：扩展 `quests/shops` 并将状态写入 `quest_state/combat_state`。

## 12. 结论

采用“领域化 JSON（方案 2）”可在保持实现简洁的同时，显著降低 RPG 玩法扩展期的数据耦合风险。  
RPGMaker 数据被限定为一次性迁移来源；运行时与存档均围绕项目内部 schema 演进，满足后续 JRPG Demo 扩展目标。
