# FND-005 计划 Review

> 审核人：Claude Opus 4.6
> 审核日期：2026-02-18
> 计划文档：`plans/foundation/FND-005.md`
> 结论：**有条件通过（Conditional Pass）**——总体方案正确，有 3 个需要修正的问题和若干建议。

---

## 1. 基线验证

| 计划中的断言 | 实际代码 | 结果 |
|---|---|---|
| `save_data.h:13` — `SAVE_SCHEMA_VERSION = 2` | `constexpr std::uint32_t SAVE_SCHEMA_VERSION = 2;`（第 13 行） | **正确** |
| `save_data.cpp:185` — `deserialize` 仅做版本上限校验 | `deserialize` 从第 185 行起，版本校验在 192-200 行，无迁移入口 | **正确** |
| `save_service.cpp:236` — `loadFromFile` 流程为"读 JSON → deserialize → apply" | 第 236-261 行，流程完全匹配 | **正确** |
| `save_slot_summary_test.cpp:22` 仅覆盖 slot 摘要 | 2 个测试，仅 `ReadsDayAndTimestampFromFile` 和 `FailsWhenGameTimeMissing` | **正确** |

**基线信息准确，可信。**

---

## 2. 需要修正的问题

### 问题 A：v1 存档不存在——迁移范围需缩窄

**严重度：中**

计划多处提到 `v1/v2 → v3` 迁移（T3、T6、测试计划），但根据 git 历史，save 模块在初始 commit (`dac4589 init`) 时就是 `schema_version = 2`，**项目从未产生过 v1 格式的存档**。代码中也没有任何 v1 格式的定义或测试夹具。

**建议：**
- 迁移器只需实现 `v2 → v3`，不需要处理 v1。
- `V1ToV3FillsDefaultStates` 测试用例改为对"缺少 schema_version"或"schema_version = 0"的错误拒绝测试，而非迁移测试。
- 若未来确实需要 v1 支持，应在另一个任务中定义 v1 的 JSON 规范后再实现。编造一个不存在的 v1 格式反而会引入虚假的兼容性承诺。

### 问题 B：四个新增字段的默认值语义未定义

**严重度：中**

计划提到新增 `quest_state`、`skill_state`、`appearance_state`、`combat_state` 并使用"稳定对象 + 默认值"策略，但**没有定义这四个字段的具体结构和默认值**。

当前 `SaveData` 中其他字段都有明确的 struct 定义（如 `GameTimeSaveData`、`PlayerSaveData`），新字段如果只是一个空的 `nlohmann::json` 对象，会与现有的强类型设计风格不一致。

**建议：**
- 在实现步骤 T2 之前，先确定每个字段的最小结构。最简方案：每个字段对应一个空 struct（如 `QuestStateSaveData {}`），JSON 写入 `{}`，读取时缺失就用默认构造。
- 在计划中补充一行明确说明："v3 占位字段在 JSON 中固定为 `{}`，对应 C++ 空 struct，后续任务扩展字段时只需修改 struct 而不改迁移器。"

### 问题 C：`save_slot_summary.cpp` 的版本校验未纳入改动范围

**严重度：低**

`save_slot_summary.cpp:36` 有独立的 `schema_version > SAVE_SCHEMA_VERSION` 校验。当 `SAVE_SCHEMA_VERSION` 从 2 升到 3 时，这个校验会**自动生效**（因为它引用同一常量），所以不需要改代码逻辑。但计划"预计改动文件"中将其标为"如需统一版本校验/错误文案"，暗示可能要改。

**建议：**
- 明确：`save_slot_summary.cpp` **无需改动**，从预计改动列表中移除或标注为"不需修改，已确认"。减少实现时不必要的关注。

---

## 3. 改进建议（非阻塞）

### 建议 1：迁移器 API 设计——操作 JSON 还是 SaveData？

计划明确选择了"迁移前置"方案：在 `deserialize` 之前对原始 JSON 做规范化。这是正确的方向。建议迁移器的函数签名为：

```cpp
// save_migrator.h
namespace game::save {

/// 将原始 JSON 就地迁移到最新 schema 版本。
/// 返回 true 表示成功（包括已是最新版本无需迁移）。
/// 返回 false 时 out_error 包含诊断信息。
[[nodiscard]] bool migrateToLatest(nlohmann::json& json, std::string& out_error);

} // namespace game::save
```

理由：
- 单一入口，`loadFromFile` 只需在 `deserialize` 前加一行调用。
- 就地修改避免拷贝。
- 后续 v3 → v4 只需在内部追加一个 step，对外接口不变。

### 建议 2：测试夹具建议使用内联 JSON 字符串

现有 `save_slot_summary_test.cpp` 使用临时文件写入再读取。对于 migrator 测试，建议直接用内联 JSON 字符串 + `nlohmann::json::parse()`，避免文件 IO 带来的测试不稳定性：

```cpp
TEST(SaveMigratorTest, V2ToV3FillsNewStateFields) {
    auto json = nlohmann::json::parse(R"({
        "schema_version": 2,
        "game_time": {"day": 5, "hour": 10.0, "minute": 30.0, "time_scale": 1.0, "paused": false},
        "player": { ... },
        "maps": []
    })");
    std::string error;
    ASSERT_TRUE(migrateToLatest(json, error)) << error;
    EXPECT_EQ(json["schema_version"], 3);
    EXPECT_TRUE(json.contains("quest_state"));
    // ...
}
```

### 建议 3：round-trip 测试应覆盖核心数据完整性

`save_data_v3_roundtrip_test.cpp` 不应只检查"能 serialize 再 deserialize 不报错"，还应验证关键字段值保持一致。建议至少覆盖：
- `game_time.day/hour` 值一致
- `player.position.x/y` 值一致
- `player.inventory.slots` 数量和内容一致
- `maps` 中 `tilled_tiles` 和 `crops` 数量一致
- 四个新增字段序列化后存在且为默认值

### 建议 4：`loadFromFile` 接入点建议

```cpp
// save_service.cpp::loadFromFile 改造
nlohmann::json json;
try { in >> json; } catch (...) { ... }

// ---- 新增：迁移前置 ----
std::string migrate_error;
if (!migrateToLatest(json, migrate_error)) {
    out_error = "存档迁移失败: " + migrate_error;
    return false;
}
// ---- 迁移结束 ----

SaveData data{};
std::string parse_error;
if (!deserialize(json, data, parse_error)) { ... }
```

简洁且不改变现有错误处理结构。

### 建议 5：考虑在 `deserialize` 中移除旧版本容错

当前 `deserialize` 对缺失字段使用 `json.value<T>(key, default)` 做兜底。引入迁移前置后，`deserialize` 只会看到规范化后的 v3 JSON。可以考虑：
- 短期（本任务）：保持现有兜底逻辑不变，降低改动风险。
- 中期：后续任务逐步将 `deserialize` 改为严格模式，缺失必要字段直接报错。

本任务建议保持兜底逻辑不变，只做加法。

---

## 4. 步骤排序评审

| 步骤 | 描述 | 评价 |
|---|---|---|
| T1 | 升级 `SAVE_SCHEMA_VERSION` 到 3 | 应放在最后或与 T5 合并。过早改常量会导致现有测试中 `schema_version:2` 的 JSON 被拒绝，打断开发节奏。 |
| T2 | 在 `SaveData` 增加四个新状态字段 | OK，但需先明确 struct 定义 |
| T3 | 新增 `save_migrator.h/.cpp` | OK |
| T4 | `loadFromFile` 接入迁移器 | OK，依赖 T3 |
| T5 | `serialize/deserialize` 完成 v3 字段读写 | OK，依赖 T2 |
| T6 | 新增迁移测试 | 建议和 T3 同步（TDD） |
| T7 | 新增 round-trip 测试 | 建议和 T5 同步（TDD） |
| T8 | 更新 CMakeLists 并跑测试 | OK |
| T9 | 更新文档 | OK |

**建议调整执行顺序为：**
1. T2（定义新 struct）→ T5（serialize/deserialize v3 字段）→ T7（round-trip 测试先行）
2. T3（migrator 实现）→ T6（migrator 测试先行）→ T4（接入 loadFromFile）
3. T1（升级常量，放在所有功能就绪后）
4. T8 → T9

---

## 5. 风险补充

计划中列出的风险基本合理，补充一条：

- **测试构建依赖风险**：新测试文件 `save_migrator_test.cpp` 和 `save_data_v3_roundtrip_test.cpp` 需要链接 `nlohmann_json`。当前 `game_tests` 通过 `game` 库间接链接了它（`save_data.cpp` 中 `#include <nlohmann/json.hpp>`）。只要测试文件 `#include "game/save/save_data.h"` 后再 `#include <nlohmann/json.hpp>`，应该没问题。但需确认 `game_tests` 的 include path 包含了 nlohmann 头文件路径。

---

## 6. 总结

| 维度 | 评分 | 说明 |
|---|---|---|
| 基线准确性 | **A** | 所有行号和代码引用经核实准确 |
| 方案合理性 | **A** | "迁移前置 + v3 规范化"是正确的架构选择 |
| 范围界定 | **B** | v1 迁移属于虚设需求，应移除 |
| 字段定义完整性 | **B-** | 四个新字段缺少具体 struct 定义 |
| 步骤可执行性 | **B+** | T1 时机需调整，测试应与实现同步 |
| 风险评估 | **A-** | 基本完善，缺少构建依赖风险 |

**下一步：** 修正问题 A/B/C 后即可开始实现。建议从 T2（定义 struct）+ T3（migrator）开始，配合 TDD 同步写测试。
