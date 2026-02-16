# Phase 0: 数据层去硬编码 Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 将 CropType、Tool、Action、ItemCategory 等 C++ enum 替换为 `entt::hashed_string` 驱动的 TypeId，使新增游戏内容不再需要修改 C++ 代码。

**Architecture:** 引入统一的 `TypeId`（即 `entt::id_type`）作为类型标识符。提供编译期常量供 C++ 热路径使用，运行时通过字符串 hash 让 JSON/Lua 动态注册新类型。保留 Direction 和 GrowthStage enum 不变（语义固定）。

**Tech Stack:** C++20, EnTT (hashed_string), nlohmann-json, Google Test, CMake

**Design doc:** `plans/2026-02-16-expansion-design.md` (Phase 0 section)

---

## 预备知识

### 关键发现

1. **存档系统已使用字符串**：`save_data.h` 中 action/direction/crop_type/growth_stage 均以 `std::string` 存储，降低了序列化改造难度。
2. **Blueprint 查找键已使用 hashed_string**：`blueprint_manager.cpp:330` 已经用 `entt::hashed_string(type_str.c_str())` 作为作物蓝图的 key。
3. **工具初始化已使用 hashed_string**：`entity_factory.cpp:127-133` 中 `"tool_hoe"_hs` 等。

### 改动范围摘要

| 旧 enum | 替换为 | 影响文件数 |
|---------|--------|-----------|
| `CropType` | `TypeId` | ~6 文件 |
| `Tool` | `TypeId` | ~4 文件 |
| `Action` | `TypeId` | ~5 文件 |
| `ItemCategory` | `TypeId` | ~2 文件 |
| `BlueprintManager` 泛化 | 注册式加载 | 2 文件 |

### 构建与测试命令

```bash
# 构建（从项目根目录）
cmake --build build --parallel

# 运行全部测试
ctest --test-dir build --output-on-failure

# 运行单个测试
ctest --test-dir build -R <test_name> --output-on-failure
```

---

## Task 1: 创建 TypeId 基础设施

**Files:**
- Create: `src/game/defs/type_id.h`
- Test: `tests/game/type_id_test.cpp`
- Modify: `CMakeLists.txt` (添加测试)

### Step 1: 编写测试

```cpp
// tests/game/type_id_test.cpp
#include <gtest/gtest.h>
#include "game/defs/type_id.h"

using namespace game::defs;

TEST(TypeId, CompileTimeConstantsAreStable) {
    // 编译期常量在不同编译单元中必须一致
    EXPECT_EQ(tool_id::Hoe, typeIdFromString("hoe"));
    EXPECT_EQ(tool_id::WateringCan, typeIdFromString("watering_can"));
    EXPECT_EQ(tool_id::Pickaxe, typeIdFromString("pickaxe"));
    EXPECT_EQ(tool_id::Axe, typeIdFromString("axe"));
    EXPECT_EQ(tool_id::Sickle, typeIdFromString("sickle"));
}

TEST(TypeId, CropTypeConstants) {
    EXPECT_EQ(crop_id::Strawberry, typeIdFromString("strawberry"));
    EXPECT_EQ(crop_id::Potato, typeIdFromString("potato"));
}

TEST(TypeId, ActionConstants) {
    EXPECT_EQ(action_id::Idle, typeIdFromString("idle"));
    EXPECT_EQ(action_id::Walk, typeIdFromString("walk"));
    EXPECT_EQ(action_id::Hoe, typeIdFromString("hoe"));
}

TEST(TypeId, CategoryConstants) {
    EXPECT_EQ(category_id::Tool, typeIdFromString("tool"));
    EXPECT_EQ(category_id::Crop, typeIdFromString("crop"));
    EXPECT_EQ(category_id::Seed, typeIdFromString("seed"));
    EXPECT_EQ(category_id::Material, typeIdFromString("material"));
    EXPECT_EQ(category_id::Consumable, typeIdFromString("consumable"));
}

TEST(TypeId, NullIdForEmptyString) {
    // 空字符串应返回一个确定值（不是 entt::null）
    auto empty_id = typeIdFromString("");
    // 只需保证不崩溃，且结果确定
    EXPECT_EQ(empty_id, typeIdFromString(""));
}

TEST(TypeId, DifferentStringsProduceDifferentIds) {
    EXPECT_NE(typeIdFromString("hoe"), typeIdFromString("axe"));
    EXPECT_NE(typeIdFromString("strawberry"), typeIdFromString("potato"));
}
```

### Step 2: 运行测试，确认失败

```bash
cmake --build build --parallel && ctest --test-dir build -R type_id_test --output-on-failure
```

Expected: 编译失败，找不到 `game/defs/type_id.h`

### Step 3: 实现 type_id.h

```cpp
// src/game/defs/type_id.h
#pragma once
#include <string_view>
#include <entt/core/hashed_string.hpp>

namespace game::defs {

/// 统一类型标识符，替代各种游戏 enum
using TypeId = entt::id_type;

/// 运行时从字符串获取 TypeId（用于 JSON/Lua 通道）
[[nodiscard]] inline TypeId typeIdFromString(std::string_view s) {
    return entt::hashed_string{s.data()}.value();
}

/// 编译期工具 ID 常量（C++ 热路径使用）
namespace tool_id {
    using namespace entt::literals;
    inline constexpr TypeId Hoe         = "hoe"_hs;
    inline constexpr TypeId WateringCan = "watering_can"_hs;
    inline constexpr TypeId Pickaxe     = "pickaxe"_hs;
    inline constexpr TypeId Axe         = "axe"_hs;
    inline constexpr TypeId Sickle      = "sickle"_hs;
    inline constexpr TypeId None        = 0;
}

/// 编译期作物 ID 常量
namespace crop_id {
    using namespace entt::literals;
    inline constexpr TypeId Strawberry = "strawberry"_hs;
    inline constexpr TypeId Potato     = "potato"_hs;
    inline constexpr TypeId Unknown    = 0;
}

/// 编译期动作 ID 常量
namespace action_id {
    using namespace entt::literals;
    inline constexpr TypeId Idle     = "idle"_hs;
    inline constexpr TypeId Walk     = "walk"_hs;
    inline constexpr TypeId Hoe      = "hoe"_hs;
    inline constexpr TypeId Watering = "watering"_hs;
    inline constexpr TypeId Planting = "planting"_hs;
    inline constexpr TypeId Sickle   = "sickle"_hs;
    inline constexpr TypeId Pickaxe  = "pickaxe"_hs;
    inline constexpr TypeId Axe      = "axe"_hs;
    inline constexpr TypeId Sleep    = "sleep"_hs;
    inline constexpr TypeId Eat      = "eat"_hs;
}

/// 编译期物品类别 ID 常量
namespace category_id {
    using namespace entt::literals;
    inline constexpr TypeId Tool       = "tool"_hs;
    inline constexpr TypeId Crop       = "crop"_hs;
    inline constexpr TypeId Seed       = "seed"_hs;
    inline constexpr TypeId Material   = "material"_hs;
    inline constexpr TypeId Consumable = "consumable"_hs;
    inline constexpr TypeId Unknown    = 0;
}

} // namespace game::defs
```

### Step 4: 添加测试到构建系统，运行测试确认通过

修改 `CMakeLists.txt`（或对应的测试 CMake 文件）添加 `type_id_test.cpp`。

```bash
cmake --build build --parallel && ctest --test-dir build -R type_id_test --output-on-failure
```

Expected: 全部 PASS

### Step 5: Commit

```bash
git add src/game/defs/type_id.h tests/game/type_id_test.cpp CMakeLists.txt
git commit -m "feat: add TypeId infrastructure for data-driven type system"
```

---

## Task 2: 替换 CropType enum → TypeId

**Files:**
- Modify: `src/game/defs/crop_defs.h` (保留 GrowthStage，移除 CropType enum，保留转换函数作为兼容层)
- Modify: `src/game/component/crop_component.h:10` (`CropType` → `TypeId`)
- Modify: `src/game/factory/blueprint.h:81` (`CropBlueprint::type_` → `TypeId`)
- Modify: `src/game/factory/blueprint_manager.cpp:309` (移除 `cropTypeFromString` 调用)
- Modify: `src/game/factory/entity_factory.cpp` (crop 创建逻辑)
- Modify: `src/game/system/farm_system.h:64,71` (`CropType` 参数 → `TypeId`)
- Modify: `src/game/system/crop_system.h` (CropType 引用)
- Modify: `src/game/data/item_catalog.h:51` (`crop_type_` → `TypeId`)
- Modify: `src/game/data/item_catalog.cpp:136-142` (移除 `cropTypeFromString` 调用)
- Modify: `src/game/save/save_data.cpp` (crop 反序列化改为 `typeIdFromString`)

### Step 1: 修改 CropComponent

```cpp
// src/game/component/crop_component.h
// 将 Line 10:
//   game::defs::CropType type_;
// 改为:
//   game::defs::TypeId crop_type_id_;
```

同时在文件顶部添加 `#include "game/defs/type_id.h"`，移除 `#include "game/defs/crop_defs.h"`（如果仅因 CropType 引入的话；GrowthStage 仍需保留）。

### Step 2: 修改 CropBlueprint

```cpp
// src/game/factory/blueprint.h
// 将 Line 81:
//   game::defs::CropType type_{game::defs::CropType::Unknown};
// 改为:
//   game::defs::TypeId crop_type_id_{0};
```

### Step 3: 修改 ItemData

```cpp
// src/game/data/item_catalog.h
// 将 Line 51:
//   game::defs::CropType crop_type_{game::defs::CropType::Unknown};
// 改为:
//   game::defs::TypeId crop_type_id_{0};
```

### Step 4: 修改 item_catalog.cpp 加载逻辑

```cpp
// src/game/data/item_catalog.cpp
// 将 Lines 139-141 (crop_type 加载):
//   data.crop_type_ = game::defs::cropTypeFromString(item_obj.value("crop_type", ""));
// 改为:
//   std::string crop_str = item_obj.value("crop_type", "");
//   data.crop_type_id_ = crop_str.empty() ? 0 : game::defs::typeIdFromString(crop_str);
```

### Step 5: 修改 blueprint_manager.cpp

```cpp
// src/game/factory/blueprint_manager.cpp
// Line 309: 将 cropTypeFromString 替换:
//   auto crop_type = game::defs::cropTypeFromString(type_str);
// 改为:
//   auto crop_type_id = game::defs::typeIdFromString(type_str);
// 并将后续对 crop_type 的使用改为 crop_type_id
```

### Step 6: 修改 farm_system.h

```cpp
// src/game/system/farm_system.h
// 将 plantSeed() 参数中的 CropType 改为 TypeId:
//   void plantSeed(..., game::defs::CropType seed_type);
// 改为:
//   void plantSeed(..., game::defs::TypeId crop_type_id);
```

同步修改 farm_system.cpp 中的实现。

### Step 7: 修改 crop_system（根据具体使用方式）

检查 crop_system.cpp 中所有对 `CropType` 的 switch/比较，改为 TypeId 比较。

### Step 8: 修改 entity_factory.cpp

crop 创建逻辑中将 `CropType` 参数改为 `TypeId`。

### Step 9: 修改 save_data.cpp 反序列化

```cpp
// crop 反序列化处 (约 Lines 304-318):
// 将 crop_type string → CropType enum 的转换
// 改为 crop_type string → typeIdFromString()
```

### Step 10: 更新 crop_defs.h

保留 `GrowthStage` enum 和其转换函数不变。移除 `CropType` enum、`cropTypeToString()`、`cropTypeFromString()`、`cropTypeToId()`。如果有其他文件仍引用这些函数，提供内联兼容包装（deprecated 标记）。

### Step 11: 编译并运行全部测试

```bash
cmake --build build --parallel && ctest --test-dir build --output-on-failure
```

Expected: 全部 PASS（尤其关注 `blueprint_manager_smoke_test`、`farm_system_*` 测试）

### Step 12: Commit

```bash
git add -u && git add src/game/defs/type_id.h
git commit -m "refactor: replace CropType enum with TypeId for data-driven crop types"
```

---

## Task 3: 替换 Tool enum → TypeId

**Files:**
- Modify: `src/game/defs/constants.h:19-26` (移除 Tool enum)
- Modify: `src/game/data/item_catalog.h:50` (`tool_type_` → `TypeId tool_id_`)
- Modify: `src/game/data/item_catalog.cpp:25-32` (移除 `toolFromString()`，改为 `typeIdFromString`)
- Modify: `src/game/system/farm_system.h/cpp` (Tool 参数 → TypeId)
- Modify: `src/game/system/player_control_system.h/cpp` (Tool 引用)
- Modify: `src/game/system/item_use_system.h/cpp` (Tool 引用)
- Modify: `src/game/defs/events.h` (UseToolEvent 中的 Tool 字段)

### Step 1: 修改 ItemData

```cpp
// src/game/data/item_catalog.h
// 将 Line 50:
//   game::defs::Tool tool_type_{game::defs::Tool::None};
// 改为:
//   game::defs::TypeId tool_id_{game::defs::tool_id::None};
```

### Step 2: 修改 item_catalog.cpp

```cpp
// 移除 toolFromString() (Lines 25-32)
// 将 Line 136-138 的工具加载:
//   data.tool_type_ = toolFromString(item_obj.value("tool", ""));
// 改为:
//   std::string tool_str = item_obj.value("tool", "");
//   data.tool_id_ = tool_str.empty() ? game::defs::tool_id::None
//                                    : game::defs::typeIdFromString(tool_str);
```

### Step 3: 修改 events.h

```cpp
// src/game/defs/events.h
// UseToolEvent 中:
//   Tool tool_{Tool::None};
// 改为:
//   TypeId tool_id_{tool_id::None};
```

### Step 4: 逐一修改引用 Tool enum 的系统文件

对 farm_system、player_control_system、item_use_system 中的所有 `Tool::Hoe` 改为 `tool_id::Hoe`，`Tool::WateringCan` 改为 `tool_id::WateringCan`，以此类推。

### Step 5: 移除 constants.h 中的 Tool enum

保留其他常量（`TOOL_TARGET_TILE_RANGE`、`TOOL_COOLDOWN`）不变。

### Step 6: 编译并运行全部测试

```bash
cmake --build build --parallel && ctest --test-dir build --output-on-failure
```

Expected: 全部 PASS（尤其 `farm_system_hoe_blocking_test`、`item_use_system_test`）

### Step 7: Commit

```bash
git add -u
git commit -m "refactor: replace Tool enum with TypeId for data-driven tool types"
```

---

## Task 4: 替换 Action enum → TypeId

**Files:**
- Modify: `src/game/component/state_component.h:12-24` (Action enum → TypeId)
- Modify: `src/game/component/state_component.h:39-50` (移除 ACTION_NAMES)
- Modify: `src/game/component/state_component.h:64-68` (actionToString → 查表或保留兼容)
- Modify: `src/game/component/state_component.h:79-82` (StateComponent 字段)
- Modify: `src/game/system/state_system.h/cpp`
- Modify: `src/game/system/player_control_system.h/cpp`
- Modify: `src/game/save/save_data.cpp:106-118` (actionFromString → typeIdFromString)
- Modify: `src/game/system/action_sound_system.h/cpp`
- Modify: `src/game/system/animation_event_system.h/cpp`

**注意**：Direction enum 保留不变。

### Step 1: 修改 StateComponent

```cpp
// src/game/component/state_component.h
struct StateComponent {
    game::defs::TypeId action_id_{game::defs::action_id::Idle};
    Direction direction_{Direction::Down};  // Direction 保留 enum
};
```

### Step 2: 提供 action_id → string 反查（用于调试/日志）

```cpp
// src/game/defs/type_id.h 中新增:
namespace action_id {
    // ... 现有常量 ...

    /// 调试用：TypeId → 可读字符串（仅用于日志，不影响游戏逻辑）
    [[nodiscard]] inline const char* toDebugString(TypeId id) {
        if (id == Idle)     return "idle";
        if (id == Walk)     return "walk";
        if (id == Hoe)      return "hoe";
        if (id == Watering) return "watering";
        if (id == Planting) return "planting";
        if (id == Sickle)   return "sickle";
        if (id == Pickaxe)  return "pickaxe";
        if (id == Axe)      return "axe";
        if (id == Sleep)    return "sleep";
        if (id == Eat)      return "eat";
        return "unknown";
    }
}
```

### Step 3: 逐一修改所有 `Action::Xxx` 引用

全局搜索 `Action::` 并替换为 `action_id::`（排除 `Direction::` 和非 Action 的用法）。

### Step 4: 修改 save_data.cpp

```cpp
// Lines 106-118: actionFromString() 改为直接使用 typeIdFromString
// 加载时:
//   out.player.state.action_id = game::defs::typeIdFromString(action_str);
// 保存时:
//   state[KEY_ACTION] = game::defs::action_id::toDebugString(player.state.action_id);
```

### Step 5: 修改动画相关系统

`action_sound_system` 和 `animation_event_system` 中的 Action 匹配改为 TypeId 比较。

### Step 6: 移除旧 Action enum 和 ACTION_NAMES

从 `state_component.h` 中移除。

### Step 7: 编译并运行全部测试

```bash
cmake --build build --parallel && ctest --test-dir build --output-on-failure
```

Expected: 全部 PASS（尤其 `action_sound_system_test`）

### Step 8: Commit

```bash
git add -u
git commit -m "refactor: replace Action enum with TypeId for data-driven action types"
```

---

## Task 5: 替换 ItemCategory enum → TypeId

**Files:**
- Modify: `src/game/data/item_catalog.h:17-24` (ItemCategory enum → TypeId)
- Modify: `src/game/data/item_catalog.h:42-53` (ItemData struct)
- Modify: `src/game/data/item_catalog.cpp:16-23` (categoryFromString → typeIdFromString)

### Step 1: 修改 ItemData

```cpp
// src/game/data/item_catalog.h
struct ItemData {
    entt::id_type id_{entt::null};
    game::defs::TypeId category_id_{game::defs::category_id::Unknown};
    game::defs::TypeId tool_id_{game::defs::tool_id::None};      // Task 3 已改
    game::defs::TypeId crop_type_id_{0};                           // Task 2 已改
    // ... 其余字段不变
};
```

### Step 2: 修改 item_catalog.cpp 加载逻辑

```cpp
// 移除 categoryFromString()，改为:
data.category_id_ = game::defs::typeIdFromString(item_obj.value("category", ""));
```

### Step 3: 修改所有 `ItemCategory::Tool` 等引用

全局搜索 `ItemCategory::` 并替换为 `category_id::`。

### Step 4: 移除 ItemCategory enum

### Step 5: 编译并运行全部测试

```bash
cmake --build build --parallel && ctest --test-dir build --output-on-failure
```

### Step 6: Commit

```bash
git add -u
git commit -m "refactor: replace ItemCategory enum with TypeId"
```

---

## Task 6: 泛化 BlueprintManager

**Files:**
- Modify: `src/game/factory/blueprint_manager.h`
- Modify: `src/game/factory/blueprint_manager.cpp`
- Test: 确保 `blueprint_manager_smoke_test` 通过

### Step 1: 添加通用注册机制

在现有 BlueprintManager 中新增：

```cpp
// src/game/factory/blueprint_manager.h
class BlueprintManager {
    // 新增：通用蓝图存储
    using BlueprintStore = std::unordered_map<game::defs::TypeId, nlohmann::json>;
    std::unordered_map<game::defs::TypeId, BlueprintStore> generic_blueprints_;

public:
    // 新增：通用蓝图注册和查询
    void loadGenericBlueprints(game::defs::TypeId category, const std::string& path,
                                const std::string& id_field = "id");
    const nlohmann::json* findGenericBlueprint(game::defs::TypeId category,
                                                game::defs::TypeId id) const;

    // 现有方法保留（ActorBlueprint/AnimalBlueprint/CropBlueprint 的类型化接口保留）
    // ...
};
```

### Step 2: 实现通用加载

```cpp
// src/game/factory/blueprint_manager.cpp
void BlueprintManager::loadGenericBlueprints(TypeId category, const std::string& path,
                                              const std::string& id_field) {
    auto json = loadJsonFile(path);
    auto& store = generic_blueprints_[category];
    for (auto& [key, value] : json.items()) {
        auto id = game::defs::typeIdFromString(key);
        store[id] = value;
    }
}

const nlohmann::json* BlueprintManager::findGenericBlueprint(TypeId category, TypeId id) const {
    auto cat_it = generic_blueprints_.find(category);
    if (cat_it == generic_blueprints_.end()) return nullptr;
    auto bp_it = cat_it->second.find(id);
    if (bp_it == cat_it->second.end()) return nullptr;
    return &bp_it->second;
}
```

### Step 3: 编译并运行测试

```bash
cmake --build build --parallel && ctest --test-dir build -R blueprint --output-on-failure
```

### Step 4: Commit

```bash
git add -u
git commit -m "feat: add generic blueprint registration to BlueprintManager"
```

---

## Task 7: 存档 Schema v3 升级

**Files:**
- Modify: `src/game/save/save_data.h` (schema version, 新增 game_flags 字段预留)
- Modify: `src/game/save/save_data.cpp` (v2→v3 迁移逻辑)

### Step 1: 升级 schema version

```cpp
// src/game/save/save_data.h
// 将 schema_version 常量从 2 改为 3
constexpr int CURRENT_SCHEMA_VERSION = 3;
```

### Step 2: 添加 v2→v3 迁移

```cpp
// src/game/save/save_data.cpp
// 在反序列化函数中:
if (schema_version == 2) {
    // v2→v3: 旧格式的 enum 字符串名与新格式完全相同
    // （"strawberry" 仍是 "strawberry"，"idle" 仍是 "idle"）
    // 只需更新 schema_version 标记
    // 新增: 补充 game_flags 空对象（如果不存在）
    if (!json.contains("game_flags")) {
        json["game_flags"] = nlohmann::json::object();
    }
    schema_version = 3;
}
```

### Step 3: 添加 game_flags 字段到 SaveData

```cpp
// src/game/save/save_data.h
struct SaveData {
    // ... 现有字段 ...
    nlohmann::json game_flags{};  // Phase 1 用，此处仅预留
};
```

### Step 4: 序列化/反序列化 game_flags

```cpp
// save_data.cpp 序列化:
json["game_flags"] = data.game_flags;

// 反序列化:
if (json.contains("game_flags")) {
    out.game_flags = json["game_flags"];
}
```

### Step 5: 编译并运行测试

```bash
cmake --build build --parallel && ctest --test-dir build -R save --output-on-failure
```

### Step 6: Commit

```bash
git add -u
git commit -m "feat: upgrade save schema to v3, add game_flags field"
```

---

## Task 8: 全量回归测试与清理

**Files:**
- Modify: 移除不再需要的旧 enum 转换函数残留
- Verify: 所有测试通过

### Step 1: 全局搜索旧 enum 残留

```bash
# 搜索是否还有直接引用旧 enum 的代码
grep -rn "CropType::" src/game/ --include="*.h" --include="*.cpp"
grep -rn "Tool::" src/game/ --include="*.h" --include="*.cpp"
grep -rn "Action::" src/game/ --include="*.h" --include="*.cpp"  # 排除 BattleCommand::Type
grep -rn "ItemCategory::" src/game/ --include="*.h" --include="*.cpp"
```

Expected: 无结果（或仅有注释中的引用）

### Step 2: 清理 crop_defs.h

确认只保留 `GrowthStage` enum 和其转换函数，`CropType` 相关内容已完全移除。

### Step 3: 清理 constants.h

确认 `Tool` enum 已移除，保留 `TOOL_TARGET_TILE_RANGE`、`TOOL_COOLDOWN` 等常量。

### Step 4: 清理 state_component.h

确认 `Action` enum、`ACTION_NAMES` 数组、`actionToString()` 已移除。`Direction` enum 和 `directionToString()` 保留。

### Step 5: 运行完整测试套件

```bash
cmake --build build --parallel && ctest --test-dir build --output-on-failure
```

Expected: 全部 PASS

### Step 6: 手动冒烟测试

启动游戏，验证：
- 玩家可以正常移动（4方向）
- 可以使用工具（锄头、水壶等）
- 可以种植和收获作物
- 可以存档和读档
- 存档文件格式正确（schema_version = 3）

### Step 7: Commit

```bash
git add -u
git commit -m "chore: clean up removed enum references, Phase 0 complete"
```

---

## 完成后验证清单

- [ ] `type_id_test` 全部通过
- [ ] `blueprint_manager_smoke_test` 通过
- [ ] `farm_system_*` 测试通过
- [ ] `item_use_system_test` 通过
- [ ] `action_sound_system_test` 通过
- [ ] `save_slot_summary_test` 通过
- [ ] 手动冒烟：移动、工具、种植、收获、存档读档
- [ ] 无旧 enum 残留（grep 确认）
- [ ] 新增 `type_id.h` 提供编译期常量
- [ ] BlueprintManager 支持通用蓝图注册
- [ ] SaveData schema v3，含 game_flags 预留字段
