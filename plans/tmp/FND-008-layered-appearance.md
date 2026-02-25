# FND-008 角色分层外观模型

## 目标

从单 Sprite 模型过渡到可组合层模型，使同一角色可由多层纹理叠加渲染（身体/眼睛/头发/衣服/配饰/武器），各层共享同一动画骨架帧号，运行时可独立切换任意层的素材。

## 思路

### 核心设计

当前系统中，一个角色 = 1 个 `SpriteComponent` + 1 个 `AnimationComponent`，动画帧直接写入 `SpriteComponent.sprite_.src_rect_`。分层外观的核心变化是：

**一个可分层角色 = N 个子层实体（每层有独立的 Sprite/Texture），由一个主实体上的 `AppearanceComponent` 统一管理。**

具体做法：
1. **主实体**保留 `AnimationComponent`（驱动帧号）、`TransformComponent`、`RenderComponent`（Y-sort 参照）、碰撞体等，但 **移除 `SpriteComponent`**。主实体不再直接渲染，仅作为逻辑锚点。
2. 新增 **`AppearanceComponent`**（挂在主实体），记录各 slot 配置（slot 类型 → asset id → texture path），以及子层实体列表。
3. 为每个活跃 slot 创建一个 **子层实体**，只包含 `SpriteComponent` + `RenderComponent`（layer/depth 与主实体相同，按 slot 顺序微调 depth 偏移实现叠加排序）。子层实体无 `TransformComponent`，需要的位置信息从主实体读取。
4. **`LayeredAnimationSystem`**（引擎层）每帧读取主实体的 `AnimationComponent` 帧号，同步写入所有子层实体的 `SpriteComponent.sprite_.src_rect_` / `pivot_` / `size_` / `is_flipped_`。同时同步子层的 `RenderComponent.depth_` = 主实体 depth + slot 微偏移。
5. 渲染侧 **无需修改** —— `RenderSystem` 已按 `RenderComponent` 排序遍历所有有 `SpriteComponent` 的实体，子层实体自然被包含在内。

### 为何使用子实体而非多 Sprite 数组

- 不侵入现有 `RenderSystem`/`AnimationSystem` 的视图查询逻辑
- 每个子层仍是一个标准 ECS 实体，可用现有 debug 面板检视
- 可自然支持不同层不同 tint/特效（未来扩展）

### 素材映射

分层 PNG 文件夹结构：
```
PNG/<动作名>/Skins/1.png      → slot: skin
PNG/<动作名>/Eyes/Male/1.png   → slot: eyes（子目录由 gender 决定）
PNG/<动作名>/Hair's/Standard/Black.png → slot: hair
PNG/<动作名>/Clothers/Farm/Red.png     → slot: clothes
PNG/<动作名>/Acc/Beret.png    → slot: accessory
PNG/<动作名>/Weapons/Hoe/1.png → slot: weapon (仅工具动作有，其余动作自动隐藏)
```

每个分层 PNG 与 Pre-made 的完整角色 PNG 共享 **相同的帧尺寸(32x32)、行排列(方向)和帧序列**，因此 `AnimationBlueprint` 中的 `frames`/`direction`/`src_size`/`position` 完全复用，只需替换 `texture_path` 即可。

### 数据配置格式

在 `actor_blueprint.json` 中，为需要分层的角色新增 `appearance` 字段：

```json
{
  "player": {
    "appearance": {
      "gender": "male",
      "slots": ["skin", "eyes", "hair", "clothes", "accessory", "weapon"],
      "defaults": {
        "skin": "1",
        "eyes": "1",
        "hair": "Standard/Black",
        "clothes": "Farm/Red"
      },
      "base_path": "assets/farm-rpg/Character and Portrait/Character/PNG"
    },
    "animations": { ... }
  }
}
```

`gender` 字段用于眼睛素材的子目录选择（`Eyes/Male/` 或 `Eyes/Female/`），eyes slot 的 asset id 只需写颜色编号（如 `"1"`），加载时自动拼接性别子目录。

`AppearanceComponent` 运行时持有各 slot 的当前 asset id 和性别信息。蓝图加载时，根据 `base_path` + 动作文件夹映射名 + slot 目录（eyes 额外加性别子目录）+ asset id 构造纹理路径。

### 动作文件夹映射

需要一个映射表把 animation name（如 `idle`, `walk`, `hoe`）对应到素材文件夹名（如 `1. Idle`, `2. Walk`, `4. Pickaxe, Hoe and Catching insects`）。此映射在配置文件 `appearance_mapping.json` 中定义。

### 向后兼容（NPC 保持单层）

不含 `appearance` 字段的角色（如 NPC "friend"）走旧路径：`AnimationSystem` + `SpriteComponent` 不变。`LayeredAnimationSystem` 只处理有 `AppearanceComponent` 的实体。

---

## 需要新增的文件

| 文件路径 | 说明 |
|---|---|
| `src/engine/component/appearance_component.h` | `AppearanceComponent` 定义（slot 列表、子层实体列表、asset 配置） |
| `src/engine/system/layered_animation_system.h/cpp` | 分层动画同步系统 |
| `src/game/factory/appearance_builder.h/cpp` | 从蓝图构建 `AppearanceComponent` 并创建子层实体的工厂逻辑 |
| `assets/data/appearance_mapping.json` | animation name → 素材文件夹名的映射配置 |

---

## 实现步骤

### 步骤 1：定义 AppearanceComponent

在 `engine/component/` 新建 `appearance_component.h`：

```cpp
// 性别枚举（用于 Eyes 等有性别分类的素材）
enum class Gender : uint8_t { Male, Female };

// 槽位类型枚举
enum class AppearanceSlot : uint8_t {
    Skin, Eyes, Hair, Clothes, Accessory, Weapon, COUNT
};

// 单个层的运行时数据
struct AppearanceLayer {
    AppearanceSlot slot;
    entt::entity layer_entity{entt::null};   // 子层实体
    entt::id_type texture_id{entt::null};     // 当前纹理ID
};

struct AppearanceComponent {
    std::vector<AppearanceLayer> layers;      // 按渲染顺序排列
    std::string base_path;                     // 素材根目录
    Gender gender{Gender::Male};               // 性别（影响 Eyes 子目录）
    // slot → 当前 asset id（如 "Standard/Black"）
    std::unordered_map<AppearanceSlot, std::string> slot_assets;
};

// 蓝图加载阶段使用的数据
struct AppearanceBlueprintData {
    std::string base_path;
    Gender gender{Gender::Male};
    std::vector<AppearanceSlot> slots;
    std::unordered_map<AppearanceSlot, std::string> defaults;  // slot → default asset id
};
```

### 步骤 2：创建 appearance_mapping.json

配置动画名到素材文件夹的映射，以及各 slot 对应的子目录名：

```json
{
  "slot_dirs": {
    "skin": "Skins",
    "eyes": "Eyes",
    "hair": "Hair's",
    "clothes": "Clothers",
    "accessory": "Acc",
    "weapon": "Weapons"
  },
  "action_dirs": {
    "idle": "1. Idle",
    "walk": "2. Walk",
    "run": "3. Run",
    "hoe": "4. Pickaxe, Hoe and Catching insects",
    "pickaxe": "4. Pickaxe, Hoe and Catching insects",
    "axe": "5. Axe and Sickle",
    "sickle": "5. Axe and Sickle",
    "watering": "7. Watering",
    "planting": "13.4 Carrying - Throwing items"
  }
}
```

### 步骤 3：实现 AppearanceBuilder

在 `game/factory/` 新增 `appearance_builder.h/cpp`：

- 解析 `actor_blueprint.json` 中的 `appearance` 字段
- 根据 `defaults` 和 `slots` 构建 `AppearanceComponent`
- 为每个活跃 slot 创建子层实体（`SpriteComponent` + `RenderComponent`），depth 按 slot 顺序偏移（如 +0.001 * slot_index）
- 预加载所有动画状态下各 slot 的纹理（复用现有动画的 `frames`/`src_size` 等参数，仅替换 texture path）
- 在子层实体上也挂 `AnimationComponent` 的引用标记（或直接用一个 `LayerOwnerTag{owner_entity}` 指回主实体）

关键：子层实体需要位置信息才能渲染。方案是在子层实体上也挂一个 `TransformComponent`，由 `LayeredAnimationSystem` 每帧从主实体同步。这样 `RenderSystem` 无需任何改动。

### 步骤 4：实现 LayeredAnimationSystem

在 `engine/system/` 新增 `layered_animation_system.h/cpp`：

每帧对所有含 `AppearanceComponent` + `AnimationComponent` 的主实体：
1. 读取当前 `animation_id` 和 `current_frame_index`
2. 根据 animation name 查 `appearance_mapping` 得到素材文件夹
3. 遍历 `AppearanceComponent.layers`，对每个子层实体：
   - 根据 slot + asset id + 动画文件夹 计算纹理 path → texture_id
     - eyes slot 额外插入性别子目录（`Eyes/Male/` 或 `Eyes/Female/`，由 `gender` 决定）
   - 设置子层 `SpriteComponent.sprite_.texture_id_` = 该纹理
   - 从主实体 `AnimationComponent` 取当前帧的 `src_rect_`，写入子层 `SpriteComponent.sprite_.src_rect_`
   - 同步 `pivot_`、`size_`、`is_flipped_`
   - 同步 `TransformComponent.position_` / `previous_position_` 从主实体
   - 同步 `RenderComponent.depth_` = 主实体 depth + slot 微偏移
4. **weapon slot 可见性控制**：若当前动画对应的素材文件夹下无 `Weapons/` 目录，则给 weapon 子层实体加 `InvisibleTag`；有武器素材时移除 `InvisibleTag`

此系统需在 `AnimationSystem` 之后、`RenderSystem` 之前执行。

### 步骤 5：修改 BlueprintManager 解析 appearance 字段

在 `blueprint_manager.cpp` 的 `parseMobBlueprintCommon` 中：
- 如果 JSON 包含 `appearance` 字段，解析为 `AppearanceBlueprintData`（base_path, gender, slots, defaults）
- `gender` 字段解析 `"male"` / `"female"` 字符串 → `Gender` 枚举
- 存入 `ActorBlueprint` 新增的 `std::optional<AppearanceBlueprintData> appearance_` 字段

### 步骤 6：修改 EntityFactory 接入分层构建

在 `entity_factory.cpp` 的 `createMobBase` 中：
- 如果蓝图包含 `appearance` 数据，调用 `AppearanceBuilder` 构建分层
- 主实体 **不挂 `SpriteComponent`**（子层实体负责渲染）
- 保留主实体的 `AnimationComponent`（驱动帧号）和 `RenderComponent`（Y-sort 基准），但给主实体加 `InvisibleTag` 使其本身不被 `RenderSystem` 绘制

### 步骤 7：注册系统到 SystemScheduler

在 `GameRuntimeAssembler` 或 `SystemScheduler` 中注册 `LayeredAnimationSystem`，排在 `AnimationSystem` 之后。

### 步骤 8：运行时换装接口

提供一个 `changeAppearance(registry, entity, slot, new_asset_id)` 函数：
- 更新 `AppearanceComponent.slot_assets[slot]`
- 对应子层实体的纹理在下一帧由 `LayeredAnimationSystem` 自动刷新（因为每帧都会重新计算 texture path）

### 步骤 9：更新 actor_blueprint.json

为 player 角色添加 `appearance` 配置，保留 `animations` 不变（帧数据复用）。

### 步骤 10：验证与清理

- 确认玩家角色分层渲染正确，所有动画状态各层同步
- 确认运行时切换头发/衣服等生效
- 确认 NPC（无 appearance 字段）行为不变
- 确认碰撞、Y-sort、动画事件等不受影响
- 子层实体在角色销毁时一同清理

---

## 待办清单

- [ ] 1. 新建 `appearance_component.h`，定义 `Gender`、`AppearanceSlot`、`AppearanceLayer`、`AppearanceComponent` 及 `AppearanceBlueprintData`
- [ ] 2. 新建 `appearance_mapping.json`，配置动画名→文件夹、slot→子目录的映射
- [ ] 3. 在 `ActorBlueprint` 中新增 `std::optional<AppearanceBlueprintData> appearance_` 字段
- [ ] 4. 修改 `BlueprintManager` 解析 `appearance` JSON 字段
- [ ] 5. 新建 `appearance_builder.h/cpp`，实现子层实体创建与纹理预加载
- [ ] 6. 新建 `layered_animation_system.h/cpp`，实现每帧同步帧数据和位置到子层实体
- [ ] 7. 修改 `EntityFactory::createMobBase`，接入分层构建逻辑
- [ ] 8. 注册 `LayeredAnimationSystem` 到 `SystemScheduler`
- [ ] 9. 更新 `actor_blueprint.json`，为 player 添加 `appearance` 配置
- [ ] 10. 实现 `changeAppearance()` 换装接口
- [ ] 11. 端到端验证：分层渲染、动画同步、运行时换装、NPC 不受影响
- [ ] 12. 清理：角色销毁时级联删除子层实体

---

## 已确认的设计决策

1. **武器层仅在工具动作时显示。** idle/walk 等无武器素材的动画状态下，weapon slot 子层实体加 `InvisibleTag` 隐藏。
2. **性别区分。** `appearance` 配置中新增 `gender` 字段（`"male"` / `"female"`），eyes slot 加载时根据性别选择 `Eyes/Male/` 或 `Eyes/Female/` 子目录，asset id 只需写颜色编号。
3. **本次迭代仅做 C++ 侧。** Lua 脚本换装绑定留到后续迭代。
