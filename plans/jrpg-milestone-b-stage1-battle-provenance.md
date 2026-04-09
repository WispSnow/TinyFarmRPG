# Milestone B / Stage 1: BattleUnit 来源信息与金币真相细化计划

## Context

Milestone B 的后续两条主线都依赖同一个基础：

- 敌方 AI 需要从当前行动敌人稳定反查 `EnemyData::actions_`
- 战斗奖励需要从被击败敌人稳定反查 `EnemyData::gold_reward_ / drops_ / exp_reward_`

但当前运行时代码存在两个关键断点：

### 1. `BattleUnit` 丢失目录来源

当前 `buildBattleUnitsFromCatalog()` 会把 actor/class/enemy/troop 数据装配成 `BattleUnit`，但战斗内只保留了：

- 数值属性
- `side`
- `name`
- `skill_ids`

也就是说，一旦进入 `BattleSession`，就无法再可靠知道：

- 这个玩家单位来自哪个 `actor.id`
- 这个敌方单位来自哪个 `enemy.id`

这会直接阻塞 Milestone B 后续：

- AI 无法从 `source_enemy_id -> EnemyData::actions_` 读取权重
- 奖励无法从 `source_enemy_id -> EnemyData::gold_reward_ / drops_` 做稳定汇总

更重要的是，`GameScene::onEnterBattleCommand()` 还有两条入口路径：

- 通过 `buildBattleUnitsFromCatalog()` 现场构造单位
- 直接使用 `EnterBattleCommand.player_units / enemy_units`

因此 Stage 1 必须明确“来源字段为空”的合法语义，而不是假定所有 `BattleUnit` 都来自 factory。

### 2. `gold` 只有存档字段，没有运行时真相

当前项目已经有：

- `SaveData::player.gold`
- `serialize()/deserialize()` 对 `gold` 的读写

但当前运行时没有真正持有金币真相的位置：

- `SaveService::capture()` / `apply()` 还没有把 `gold` 与玩家实体打通
- `InventoryMenuScene::syncCharacterPanel()` 仍然写死 `Gold: --`
- 目前也没有 ECS component 挂在 player entity 上表示“当前金币”

这意味着：

- 存档里虽然有 `gold` 字段，但它还只是 schema 占位
- 后续商店系统即使开始实现，也没有稳定的运行时余额来源

同时，当前玩家实体已经天然拥有一组“探索态真相组件”：

- `PlayerTag`
- `InventoryComponent`
- `HotbarComponent`
- `AppearanceComponent`

并且它们都挂在 player entity 上，由 `EntityFactory::createActor("player")` 初始化。因此，金币真相也应该落在同一个实体上，而不是另起全局单例或散落在 Scene 私有字段中。

因此，Stage 1 的目标不是实现 AI 或奖励本身，而是补齐这两条基础链路：

- `BattleUnit` 来源信息
- 玩家金币运行时真相

## 范围

### 本阶段包含

- 为 `BattleUnit` 定义最小来源字段
- 在 `buildBattleUnitsFromCatalog()` 中写入来源字段
- 锁定预构建 `BattleUnit` 的“无来源”语义
- 新增玩家金币运行时组件，明确组件命名与所有权
- `SaveService::capture()/apply()` 接入金币组件
- `InventoryMenuScene` 用真实金币替换占位文本
- 对应的测试补强方案

### 本阶段不包含

- 敌方 AI planner
- 战斗奖励汇总器
- `BattleEndedEvent` 奖励扩展
- 金币增减领域服务
- 商店规则
- 经验值成长或升级系统

## 实现思路

### 1. `BattleUnit` 来源字段使用 `std::optional<std::string>`

推荐在 `src/game/battle/battle_types.h` 的 `BattleUnit` 末尾新增：

- `std::optional<std::string> source_actor_id{}`
- `std::optional<std::string> source_enemy_id{}`

字段位置建议：

- 放在 `skill_ids` 之后
- 不调整现有字段顺序

原因：

- 当前测试和领域代码大量使用 C++20 designated initializer，新增字段追加在末尾，对现有初始化点的破坏最小
- `std::optional` 能明确表达“这个单位没有来源信息”，比空字符串 sentinel 更清晰
- `std::string` 与 `RpgCatalog::findActor(std::string_view)` / `findEnemy(std::string_view)` 直接兼容，不需要额外 hash 往返
- 比 `entt::id_type` 更可读，调试日志和测试断言也更友好

本阶段不建议新增：

- `enum class BattleUnitSourceKind`
- `std::variant<ActorSource, EnemySource>`
- 额外的来源查询 helper 类

原因：

- 当前只需要最小来源桥接，不需要一开始把来源系统抽象得很重
- 玩家和敌方的来源字段已经足够支撑 Milestone B 后续 AI 与奖励使用

### 2. 来源字段的语义约束要先锁定

建议把以下约束写死到实现与测试预期中：

- `side == BattleSide::Player` 的单位，推荐只写 `source_actor_id`
- `side == BattleSide::Enemy` 的单位，推荐只写 `source_enemy_id`
- 允许两个字段都为空，这表示“无目录来源”
- 不把显示名、索引位置、battle unit id 当成来源推断依据

这里要特别照顾预构建单位路径：

- `EnterBattleCommand.player_units / enemy_units` 直接传入的单位，不强制要求调用者立刻补来源字段
- 若调用者未填写来源字段，Stage 1 只保留空 `optional`
- 后续 Stage 2 / Stage 3 的 AI 与奖励逻辑必须显式处理“无来源”分支，而不是隐式猜测

推荐策略：

- Stage 1 不在 `GameScene` 中尝试对预构建单位做显示名反查 enrichment
- 保持调用者传什么，战斗层就接什么

原因：

- 通过显示名或 troop 站位反推 `enemy_id` 非常脆弱
- 预构建单位未来可能来自脚本、测试、特殊事件战斗，不一定对应标准 catalog 条目

### 3. `buildBattleUnitsFromCatalog()` 负责写入来源字段

在 `src/game/battle/battle_unit_factory.cpp` 中建议明确：

- 玩家单位写 `.source_actor_id = actor->id_`
- 敌方单位写 `.source_enemy_id = enemy->id_`

同时保持：

- 玩家单位的 `skill_ids` 继续来自 `actor->skill_ids_`
- 敌方单位的 `skill_ids` 继续来自当前的 `collectEnemySkillIds(*enemy)`

Stage 1 不修改的点：

- 不改变敌方 `skill_ids` 去重策略
- 不引入 `EnemyActionData::rating_` 到 `BattleUnit`
- 不让 `GameScene` 保存额外的 troop member -> enemy source side table

原因：

- Stage 1 的职责是让来源“能被找回”，不是一次性把 AI 所需全部数据都搬进 `BattleUnit`
- 敌方 AI 仍可在 Stage 2 通过 `source_enemy_id -> EnemyData` 访问原始 `actions_`

### 4. 金币真相采用独立 `PlayerWalletComponent`

推荐新增文件：

- `src/game/component/player_wallet_component.h`

推荐结构保持最小：

```cpp
namespace game::component {

struct PlayerWalletComponent {
    int gold_{0};
};

} // namespace game::component
```

设计约束：

- 只承担金币真相，不扩成“大而全玩家进度容器”
- 不把技能、任务、经验等未来字段一起塞进来
- 不把金币硬塞进 `ActorComponent`

原因：

- `ActorComponent` 当前只承担动作/持物语义，掺入经济字段会让职责发散
- `PlayerWalletComponent` 命名准确，后续若确实需要更大范围的进度组件，再另行抽象
- 金币和背包一样，都是探索态玩家实体持有的真相，放在 ECS 上最自然

### 5. 组件创建责任放在玩家实体初始化路径

推荐在 `EntityFactory::createActor()` 的 `actor_name_id == "player"_hs` 分支中新增：

- `registry_.emplace<game::component::PlayerWalletComponent>(entity);`

即让 player entity 在创建时天然具备：

- `PlayerTag`
- `InventoryComponent`
- `HotbarComponent`
- `PlayerWalletComponent`

这样可以保证：

- 正常新开局时金币有稳定默认值
- 后续 `InventoryMenuScene`、`SaveService` 都能把钱包组件当成核心玩家状态读取

说明：

- Stage 1 不需要新增专门的 wallet 初始化服务
- 默认值为 `0` 足够，真正的存档恢复由 `SaveService::apply()` 覆盖

### 6. `SaveService` 负责在存档模型和运行时钱包之间桥接

当前 `SaveData::player.gold` 已经存在，因此 Stage 1 不需要修改：

- `save_data.h`
- `save_data.cpp`
- schema version
- migrator

真正需要修改的是 `SaveService::capture()` 和 `SaveService::apply()`。

#### `capture()` 推荐方案

在捕获 player entity 的核心状态时：

- 读取 `PlayerWalletComponent`
- 写入 `out.player.gold`

推荐把钱包组件视为“强烈建议存在的运行时真相”，但在 `capture()` 中采用降级策略：

- 若 player 存在 `PlayerWalletComponent`，正常写入 `out.player.gold`
- 若 player 缺少 `PlayerWalletComponent`，记录 `warn`，并将 `out.player.gold` 保持为默认值 `0`
- 不因为缺少 wallet 中断整个 `capture()` 流程

原因：

- 当前 `InventoryComponent` / `HotbarComponent` 已被视为硬依赖
- 但 wallet 缺失的后果远轻于 inventory/hotbar 缺失，最多是本次存档中的金币回落到 `0`
- 当前已有测试 fixture 手工构造 player entity，只挂 `InventoryComponent / HotbarComponent`；若把 wallet 缺失升级为硬失败，会破坏现有测试与非 factory 初始化路径
- 这种降级策略能兼顾 Stage 1 的真相链目标和当前代码库的兼容过渡成本

#### `apply()` 推荐方案

在 `loadMap()` 后、定位到 player entity 后：

- `emplace_or_replace<game::component::PlayerWalletComponent>(player, game::component::PlayerWalletComponent{data.player.gold});`

推荐使用 `emplace_or_replace` 而不是只 `get()`：

- 即使某些测试或旧路径没有提前创建组件，也能在读档时补齐
- 这比假设所有运行时路径都先经过 `EntityFactory::createActor("player")` 更稳

设置时机建议：

- 放在 inventory/hotbar 恢复之后、appearance 恢复之前
- 在任何 UI 或 dispatcher sync 之前完成

推荐插入位置可直接对齐当前 `save_service.cpp` 结构：

- 先 `try_get` / 恢复 `InventoryComponent`
- 再 `try_get` / 恢复 `HotbarComponent`
- 然后 `emplace_or_replace<PlayerWalletComponent>()`
- 再处理 `AppearanceComponent`

这样 `InventoryMenuScene`、后续商店、奖励写回都能在玩家实体上看到一致金币值。

### 7. `InventoryMenuScene` 改为读取钱包组件

当前 `InventoryMenuScene::syncCharacterPanel()` 写死：

- `gold_label_ = "Gold: --";`

Stage 1 推荐改为：

- 优先读取 `game_registry_.try_get<game::component::PlayerWalletComponent>(player_)`
- 成功时使用 `fmt::format("Gold: {}", wallet->gold_)`
- 若缺失组件，显示 `Gold: 0` 并记录 warn，而不是继续显示 `"--"`

原因：

- Stage 1 的目标之一就是去掉金币占位文本
- 对 UI 而言，用 `0` 兜底比继续保留 `"--"` 更接近真实系统
- 即使个别测试手工造 player entity 忘了加 wallet，菜单也不至于退回占位态
- 项目已通过 `spdlog/fmt` 使用 `fmt::format`，这里沿用同一风格更自然

本阶段不强制新增：

- 钱包变更事件
- 菜单打开期间的实时金币热更新

原因：

- `InventoryMenuScene` 当前本来就在 `init()` 时同步角色面板
- Milestone B Stage 1 只需要先打通真相链，不需要做实时订阅系统

### 8. Stage 1 不需要修改 `BattleEndedEvent`

虽然本阶段和 Milestone B 有关，但 Stage 1 只处理基础桥接，不应提前改奖励事件结构。

因此本阶段不修改：

- `BattleEndedEvent`
- `GameScene::onBattleEnded()`
- `BattleRewardSummary`

原因：

- 奖励汇总和事件负载扩展属于 Stage 3/4 的职责
- 现在先把来源字段和金币真相准备好，后续事件扩展才能有稳定落点

## 测试策略

Stage 1 建议至少补强以下测试：

### 1. `tests/game/battle/battle_unit_factory_test.cpp`

新增断言：

- 玩家单位 `source_actor_id.has_value()`
- 敌方单位 `source_enemy_id.has_value()`
- 玩家单位不应错误写入 `source_enemy_id`
- 敌方单位不应错误写入 `source_actor_id`

这是 Stage 1 最关键的回归测试。

### 2. `SaveService` 行为测试

推荐在现有 save 测试上补两类断言：

- `capture()` 会把 `PlayerWalletComponent.gold_` 写入 `SaveData::player.gold`
- `apply()` 会把 `SaveData::player.gold` 恢复到 `PlayerWalletComponent.gold_`

优先方案：

- 直接扩展 `tests/game/save_service_async_test.cpp`

原因：

- 这个 fixture 已经有完整的 registry / map / player / `SaveService` 装配流程
- 当前 fixture 还是手工创建 player entity，这也正好能覆盖“wallet 组件不是所有路径都天然存在”的现实约束
- 在现有 save service 测试体系里补 wallet 断言，比新增专项文件更经济

推荐最小补强方式：

- 在 fixture 的 player 上补 `PlayerWalletComponent`
- 在现有写文件 case 中断言保存出来的 JSON `player.gold` 来自 wallet
- 若要补 `apply()` 恢复断言，优先继续放在现有 save service 测试体系里，而不是新增独立 wallet 专项文件

### 3. `InventoryMenuScene` 金币显示测试

推荐验证：

- player entity 挂 `PlayerWalletComponent{345}` 时，`syncCharacterPanel()` 不再写 `"Gold: --"`
- 而是生成 `"Gold: 345"`

若当前不想为 RmlUi 场景加完整行为测试，至少可以补一个轻量 source smoke / UI integration 测试，确认：

- `InventoryMenuScene` 读取 `PlayerWalletComponent`
- `gold_label_` 以真实值构造

### 4. 存档 roundtrip 测试保持不变但继续作为护栏

`tests/game/save_data_v3_roundtrip_test.cpp` 已经覆盖：

- `SaveData::player.gold` 的 JSON roundtrip

Stage 1 不需要改 schema 测试预期，但这组测试继续提供“序列化层未被破坏”的护栏。

## 需要新增或修改的文件

### 计划文档

- `plans/jrpg-milestone-b-stage1-battle-provenance.md`

### 预计修改的代码文件

- `src/game/battle/battle_types.h`
- `src/game/battle/battle_unit_factory.cpp`
- `src/game/component/player_wallet_component.h`
- `src/game/factory/entity_factory.cpp`
- `src/game/save/save_service.cpp`
- `src/game/scene/inventory_menu_scene.cpp`

### 实现时需要注意的头文件依赖变化

- `src/game/save/save_service.cpp` 需要新增 `player_wallet_component.h`
- `src/game/scene/inventory_menu_scene.cpp` 需要新增 `player_wallet_component.h`
- 若库存菜单用 `fmt::format` 生成金币文本，`src/game/scene/inventory_menu_scene.cpp` 还需要新增 `#include <spdlog/fmt/fmt.h>`

### 预计补强的测试文件

- `tests/game/battle/battle_unit_factory_test.cpp`
- `tests/game/save_service_async_test.cpp` 或新增 wallet 专项测试
- `tests/game/inventory_menu_scene_*` 相关测试或新增轻量 smoke

## 实现步骤

### Step 1: 为 `BattleUnit` 增加来源字段

在 `battle_types.h` 中新增：

- `std::optional<std::string> source_actor_id{}`
- `std::optional<std::string> source_enemy_id{}`

并保持字段追加，不重排已有成员。

### Step 2: 在 battle unit factory 中写入来源

在 `buildBattleUnitsFromCatalog()` 中：

- 玩家单位写 `actor->id_`
- 敌方单位写 `enemy->id_`

同时补充 `BattleUnitFactoryTest` 断言。

### Step 3: 新增 `PlayerWalletComponent`

新增组件头文件，并在 `EntityFactory::createActor("player")` 中初始化默认钱包。

### Step 4: 打通 `SaveService` 的金币桥接

在 `capture()` 读取 wallet；
在 `apply()` 恢复 wallet。

本步骤不修改 `SaveData` schema。

### Step 5: 替换库存菜单中的金币占位文本

让 `InventoryMenuScene::syncCharacterPanel()` 从钱包组件读取真实金币，移除 `"Gold: --"` 占位路径。

### Step 6: 补强测试并锁定“无来源”语义

补充：

- provenance 断言
- wallet capture/apply 断言
- inventory menu gold label 断言

同时明确：

- 预构建单位来源为空是合法输入
- Stage 2 / 3 必须显式处理这一分支

## 完成标准

- [ ] `BattleUnit` 具备显式来源字段，且 factory 路径会正确写入
- [ ] 预构建 `BattleUnit` 可以合法保留空来源
- [ ] player entity 持有独立 `PlayerWalletComponent`
- [ ] `SaveService::capture()` 会写出真实金币
- [ ] `SaveService::apply()` 会恢复真实金币
- [ ] `InventoryMenuScene` 不再显示 `Gold: --`
- [ ] 关键测试覆盖 provenance 与 wallet 真相链

## 备注

当前额外设计结论：

- 来源字段使用 `std::optional<std::string>`，不使用空字符串 sentinel
- 金币真相放在 player entity 上，而不是 Scene 私有字段或全局单例
- 钱包组件保持最小，不提前演化成综合 progress 容器
- Stage 1 只准备基础桥接，不提前混入 AI planner、奖励事件或经验值系统

这样可以保证 Milestone B 后续的 AI 与奖励实现建立在稳定、可复用的运行时真相之上。
