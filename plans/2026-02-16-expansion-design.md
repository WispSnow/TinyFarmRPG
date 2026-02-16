# TinyFarm RPG 扩展设计文档

## 概述

基于现有 TinyFarm 农场模拟 Demo，进行四大方向的扩展：

1. **Lua 脚本支持**（Lua + Sol2）
2. **RPG 玩法**（商店、任务线、回合制战斗、技能系统）
3. **角色外观分层**（body + clothes + hair + eyes + weapon，全场景）
4. **粒子特效**（Effekseer 集成）

## 需求决策

| 维度 | 决定 |
|------|------|
| Lua 范围 | 轻量胶水层 — 任务/对话/物品效果脚本，核心系统仍 C++ |
| 战斗系统 | 中等策略式 — 多角色队伍、属性克制、Buff/Debuff、多目标技能、状态异常 |
| 角色分层 | 5层：body + clothes + hair + eyes + weapon，全场景（地图+战斗） |
| 粒子系统 | Effekseer 集成 |
| 优先级 | Lua 脚本先行，作为其余系统的地基 |

## 重构策略：方案 A（渐进式重构）

保持现有架构基本不动，逐层叠加新能力。每一步都保证游戏可运行。

---

## Phase 0 — 数据层去硬编码

### 问题

当前项目中多处 C++ enum 硬编码，添加新内容必须改 C++ 代码并重新编译：

- `crop_defs.h`: `enum class CropType { Strawberry, Potato, Unknown }`
- `constants.h`: `enum class Tool { None, Hoe, WateringCan, Pickaxe, Axe, Sickle }`
- `constants.h`: `enum class Action { Idle, Walk, Hoe, Watering, ... }`
- `item_catalog.cpp`: `toolFromString()` / `cropTypeFromString()` 手动映射

### 方案

#### 1. 统一类型 ID 系统

用 `entt::hashed_string`（项目已大量使用）替代 enum 作为类型标识符：

```cpp
// 新增: src/game/defs/type_id.h
namespace game::defs {
    using TypeId = entt::id_type;

    namespace tool_id {
        inline constexpr TypeId Hoe = "hoe"_hs;
        inline constexpr TypeId WateringCan = "watering_can"_hs;
        // 不再是 enum，可以无限扩展
    }

    inline TypeId typeIdFromString(std::string_view s) {
        return entt::hashed_string{s.data()}.value();
    }
}
```

影响范围：
- `CropComponent`: `CropType` → `TypeId crop_type_id_`
- `ItemDef`: `Tool tool_` → `TypeId tool_id_`
- `StateComponent`: `Action action_` → `TypeId action_id_`
- `SaveData`: 序列化时存字符串，加载时 hash

#### 2. ItemCatalog 简化

```cpp
struct ItemDef {
    TypeId id;
    TypeId category_id;   // "tool", "seed", "crop", "material"
    TypeId tool_id;       // 仅工具类物品有效
    TypeId crop_type_id;  // 仅种子/作物有效
    std::string display_name;
    int stack_limit;
    // JSON 直接映射，不需要 enum 转换
};
```

#### 3. Blueprint 系统泛化

```cpp
class BlueprintManager {
    std::unordered_map<TypeId, std::unordered_map<TypeId, nlohmann::json>> blueprints_;
    using Loader = std::function<void(const nlohmann::json&, TypeId id)>;
    std::unordered_map<TypeId, Loader> loaders_;

public:
    void registerLoader(TypeId category, Loader loader);
    void loadBlueprints(TypeId category, const std::string& path);
    const nlohmann::json& get(TypeId category, TypeId id) const;
};
```

#### 4. 存档兼容

- `SaveData` schema 升级至 v3
- TypeId → 字符串序列化，字符串 → typeIdFromString() 反序列化
- 提供 v2→v3 迁移逻辑

#### 保留不变

- `Direction` enum（上下左右只有4个值，语义固定）
- `GameState` enum（Title/Playing/Paused，引擎级状态）
- `GrowthStage` enum（Seed/Sprout/Growing/Mature，阶段结构固定）

#### 验收标准

- 所有现有功能正常（种地、收获、工具使用、存档读档）
- 可以仅通过修改 JSON 添加新的作物/工具类型
- 现有单元测试全部通过

---

## Phase 1 — Lua + Sol2 集成

### 架构定位

```
┌─────────────────────────────────────────────┐
│  Lua 脚本层 (assets/scripts/)               │
│  对话分支、任务触发/完成条件、物品使用效果     │
│  战斗技能效果计算（Phase 3 用）、事件响应钩子  │
├─────────────────────────────────────────────┤
│  ScriptManager (C++ 桥接层)                  │
│  管理 sol::state、注册 C++ API 绑定           │
│  脚本热重载（debug）、沙盒化                   │
├─────────────────────────────────────────────┤
│  现有 C++ 引擎 + 游戏系统（不变）             │
└─────────────────────────────────────────────┘
```

### 依赖

```cmake
FetchContent_Declare(lua URL https://github.com/lua/lua/archive/v5.4.7.tar.gz)
FetchContent_Declare(sol2 URL https://github.com/ThePhD/sol2/archive/v3.3.1.tar.gz)
```

Sol2 header-only，Lua 编译为静态库。只链接到 game library。

### ScriptManager

```cpp
// 新增: src/game/script/script_manager.h
class ScriptManager {
    sol::state lua_;
    entt::dispatcher& dispatcher_;
    entt::registry& registry_;

public:
    ScriptManager(entt::registry& reg, entt::dispatcher& disp,
                  const game::data::ItemCatalog& catalog);
    void loadScripts(const std::string& directory);
    sol::protected_function_result call(const std::string& func_name,
                                         sol::variadic_args args);
    void reloadAll();  // debug 热重载

    // 协程管理
    void startCoroutine(const std::string& script,
                        const std::string& func, sol::table ctx);
    bool resumeCoroutine(const std::string& id, sol::object result = sol::nil);
    bool isCoroutineActive(const std::string& id) const;

private:
    void registerCoreAPI();
    void registerEventAPI();
    void registerEntityAPI();
    void registerItemAPI();
    void setupSandbox();

    struct ActiveCoroutine {
        sol::thread thread;
        sol::coroutine co;
        std::string id;
    };
    std::unordered_map<std::string, ActiveCoroutine> active_coroutines_;
};
```

### Lua API

```lua
-- 物品操作
item.give(player, "strawberry", 5)
item.remove(player, "potato_seed", 1)
item.has(player, "axe")

-- 事件触发
event.emit("dialogue_show", { npc = "friend", dialogue_id = "quest_01_start" })

-- 实体查询（只读）
local pos = entity.get_position(player)
local hp = entity.get_hp(player)

-- 游戏时间
local day = game_time.get_day()
local hour = game_time.get_hour()

-- 全局标记（任务系统用）
flags.set("quest_01_talked", true)
flags.get("quest_01_talked")
```

### 对话系统改造

```jsonc
// dialogue_script.json 支持两种模式
{
  "friend_greeting": {
    "type": "script",
    "script": "dialogues/friend"   // Lua 脚本驱动
  },
  "old_man_talk": {
    "type": "simple",              // 向后兼容
    "lines": ["你好啊。", "天气真好。"]
  }
}
```

```lua
-- assets/scripts/dialogues/friend.lua
function on_talk(ctx)
    if not flags.get("quest_01_started") then
        dialogue.say("你好！最近农场怎么样？")
        local choice = dialogue.choose("你愿意帮忙吗？", { "当然！", "我再想想" })
        if choice == 1 then
            flags.set("quest_01_started", true)
            quest.start("harvest_strawberries")
            dialogue.say("太好了！请帮我收获5个草莓。")
        else
            dialogue.say("好吧，需要的时候再来找我。")
        end
    end
end
```

### 执行模型

对话脚本使用 Lua 协程，不阻塞主循环：

```
DialogueSystem 收到 InteractRequest
→ 查找 dialogue_script.json，发现 type="script"
→ ScriptManager::startCoroutine()
→ Lua 执行到 dialogue.say() 时 yield
→ C++ 显示对话气泡，等待玩家确认
→ 玩家按键 → ScriptManager::resumeCoroutine()
→ Lua 继续执行
```

### 全局标记系统（GameFlags）

```cpp
class GameFlags {
    std::unordered_map<std::string, sol::object> flags_;
public:
    void set(const std::string& key, sol::object value);
    sol::object get(const std::string& key) const;
    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);
};
```

纳入 SaveData 的 `game_flags` 字段。

### 文件结构

```
assets/scripts/
├── dialogues/          # 对话脚本
├── items/              # 物品效果脚本
├── quests/             # 任务定义
└── lib/                # 公共 Lua 工具库
```

### 验收标准

- sol::state 正常初始化，脚本可加载执行
- Lua 编写对话分支，包含条件判断和选项
- Lua 定义物品使用效果
- 协程正确 yield/resume，不阻塞主循环
- GameFlags 可序列化到存档
- Debug 模式支持脚本热重载

---

## Phase 2 — 多层精灵渲染与角色外观系统

### 组件设计

```cpp
// 引擎层: src/engine/component/composite_sprite_component.h
struct SpriteLayer {
    entt::id_type texture_id{entt::null};
    engine::utils::Rect src_rect{};
    glm::vec2 dst_size{0.0f};
    glm::vec2 pivot{0.0f};
    glm::vec2 offset{0.0f};
    bool flip_horizontal{false};
    bool visible{true};
};

struct CompositeSpriteComponent {
    // [0]=body, [1]=clothes, [2]=hair, [3]=eyes, [4]=weapon
    std::vector<SpriteLayer> layers;
};

// 引擎层: src/engine/component/composite_animation_component.h
struct LayerAnimationData {
    entt::id_type texture_id{entt::null};
    glm::vec2 frame_src_position{0.0f};
    glm::vec2 frame_src_size{0.0f};
    std::vector<int> frames;
    bool flip_horizontal{false};
};

struct CompositeAnimationComponent {
    entt::id_type current_anim_id{entt::null};
    std::vector<LayerAnimationData> layer_anims;
    float ms_per_frame{0.0f};
    float elapsed_ms{0.0f};
    int current_frame_index{0};
    bool loop{true};
    std::unordered_map<int, entt::id_type> events;
};

// 游戏层: src/game/component/equipment_visual_component.h
struct EquipmentVisualComponent {
    std::unordered_map<TypeId, TypeId> slots;
    // 例: {"clothes"_hs: "blue_shirt"_hs, "hair"_hs: "red_long"_hs}
};
```

### 外观配置

```jsonc
// assets/data/appearance_config.json
{
  "layer_order": ["body", "clothes", "hair", "eyes", "weapon"],
  "slots": {
    "body": {
      "default": "skin_light",
      "options": {
        "skin_light": { "texture": "textures/characters/body_light.png", "animations": "appearance/body_light" },
        "skin_dark":  { "texture": "textures/characters/body_dark.png",  "animations": "appearance/body_dark" }
      }
    },
    "clothes": { /* 同结构 */ },
    "hair":    { /* 同结构 */ },
    "eyes":    { /* 同结构 */ },
    "weapon":  { "default": null, "options": { "hoe": {...}, "sword_iron": {...} } }
  }
}
```

### 系统改造

- **RenderSystem**: 新增对 `CompositeSpriteComponent` 的处理，逐层调用 `drawSprite()`
- **AnimationSystem**: 新增对 `CompositeAnimationComponent` 的处理，一个时间轴驱动多层
- **AppearanceSystem**: 监听装备变化事件，更新 `CompositeSpriteComponent` 的对应层

同一动画所有层必须有相同帧数和帧时长（素材规格保证），加载时校验。

### 外观切换流程

```
装备变化 → EquipmentVisualComponent.slots 更新
→ AppearanceSystem 监听事件
→ 查找 appearance_config.json 获取新纹理/动画数据
→ 更新 CompositeSpriteComponent 对应层
→ 下一帧自动以新外观渲染
```

### 战斗场景

同样使用 CompositeSpriteComponent，但引用战斗立绘素材（侧视/正视静态分层），动画更简单（idle、攻击、受伤、胜利）。

### 验收标准

- 玩家角色以 5 层叠加渲染，4 方向行走动画各层同步
- 可通过修改 EquipmentVisualComponent 实时切换外观
- 原有单层实体不受影响
- 5 层叠加无明显帧率下降

---

## Phase 3 — RPG 战斗与游戏系统

### 3A. 战斗系统核心

#### 场景隔离

```
SceneManager 场景栈:
  [0] GameScene (暂停)
  [1] BattleScene (栈顶)

触发: 地图遭遇/脚本触发 → push BattleScene
结束: 战斗结算 → pop → GameScene resume
```

BattleScene 拥有独立的 `entt::registry`，共享 Context 中的引擎服务。

#### 战斗状态机

```
BattleStart → TurnBegin → CommandSelect → ActionExec → TurnEnd
                 ↑                                        │
                 └────────────────────────────────────────┘
                                    │
                             Victory / Defeat / Escape
                                    ↓
                               BattleEnd
```

#### 战斗组件

```cpp
struct BattlerStatsComponent {
    int hp, max_hp, mp, max_mp;
    int atk, def, mat, mdf, agi, luk;
};

struct ElementComponent { TypeId element_id; };

struct BuffListComponent {
    struct BuffInstance { TypeId buff_id; int remaining_turns; int stack_count; };
    std::vector<BuffInstance> buffs;
};

struct StatusEffectComponent {
    std::unordered_map<TypeId, int> effects;  // id → 剩余回合
};

struct BattleCommandComponent {
    enum class Type { Attack, Defend, Skill, Item, Escape };
    Type type; TypeId skill_id; TypeId item_id;
    entt::entity target; std::vector<entt::entity> multi_targets;
};

struct PartyMemberTag {};
struct EnemyTag {};
```

#### BattleScene 结构

```cpp
class BattleScene : public Scene {
    BattlePhase phase_;
    std::unique_ptr<TurnOrderSystem> turn_order_system_;
    std::unique_ptr<CommandSelectSystem> command_select_system_;
    std::unique_ptr<ActionExecuteSystem> action_execute_system_;
    std::unique_ptr<BuffSystem> buff_system_;
    std::unique_ptr<DamageCalculator> damage_calculator_;
    std::unique_ptr<BattleAISystem> ai_system_;
    std::unique_ptr<BattleUI> battle_ui_;
    BattleConfig config_;
};
```

#### 伤害计算

- 基础物理: `atk * 2 - def + rand`
- 基础魔法: `mat * 2 - mdf + rand`
- 属性克制: 从 `element_chart.json` 查表 (0.5x / 1.0x / 2.0x)

### 3B. 技能与 Buff 系统

#### 技能定义（JSON + Lua）

```jsonc
// assets/data/skill_config.json
{
  "fireball": {
    "display_name": "火球术", "mp_cost": 8, "element": "fire",
    "target": "single_enemy", "damage_type": "magical",
    "power": 120, "accuracy": 95,
    "effects": [], "animation": "fx_fireball"
  },
  "special_combo": {
    "display_name": "特殊连击", "mp_cost": 20,
    "script": "skills/special_combo"  // 复杂技能走 Lua
  }
}
```

#### Buff 定义

```jsonc
// assets/data/buff_config.json
{
  "atk_up": {
    "display_name": "攻击力提升", "duration": 3, "max_stack": 3,
    "stat_modifiers": { "atk": { "percent": 25 } }
  },
  "regen": {
    "display_name": "再生", "duration": 5,
    "on_turn_start": { "heal_percent": 5 }
  }
}
```

### 3C. 商店系统

```jsonc
// assets/data/shop_config.json
{
  "general_store": {
    "display_name": "杂货店",
    "items": [
      { "item_id": "health_potion", "price": 50 },
      { "item_id": "strawberry_seed", "price": 20 }
    ]
  },
  "blacksmith": {
    "display_name": "铁匠铺",
    "items": [ { "item_id": "sword_iron", "price": 200 } ],
    "unlock_condition": "quest_01_complete"
  }
}
```

通过 NPC 对话 Lua 脚本触发 `shop.open("general_store", "buy")`。

### 3D. 任务系统

```jsonc
// assets/data/quest_config.json
{
  "harvest_strawberries": {
    "display_name": "草莓丰收",
    "objectives": [{ "type": "collect_item", "item_id": "strawberry", "count": 5 }],
    "rewards": { "gold": 100, "items": [{"item_id": "health_potion", "count": 3}], "exp": 50 }
  },
  "defeat_slimes": {
    "display_name": "消灭史莱姆",
    "prerequisites": ["harvest_strawberries"],
    "objectives": [{ "type": "defeat_enemy", "enemy_id": "slime", "count": 3 }],
    "rewards": { "gold": 200, "exp": 100 }
  }
}
```

QuestManager 管理任务生命周期；QuestTrackingSystem 通过事件监听自动更新进度。

### 存档扩展

SaveData 新增 `game_flags`、`quest_progress`、`party_data` 字段。

### 验收标准

- 可从地图触发战斗，选择指令，看到伤害，战斗结束返回地图
- JSON 定义技能含属性克制和 Buff，复杂技能可走 Lua
- NPC 对话触发商店，可买卖物品
- 可接取/追踪/完成任务，进度自动更新，可序列化存档

---

## Phase 4 — Effekseer 粒子系统集成

### 依赖

```cmake
FetchContent_Declare(effekseer
    GIT_REPOSITORY https://github.com/effekseer/Effekseer.git
    GIT_TAG <stable_tag>)
# 需要: Effekseer (核心) + EffekseerRendererGL (OpenGL 后端)
```

链接到 engine library（粒子是引擎级能力）。

### ParticleManager

```cpp
// 新增: src/engine/particle/particle_manager.h
class ParticleManager {
    Effekseer::ManagerRef efk_manager_;
    EffekseerRendererGL::RendererRef efk_renderer_;
    std::unordered_map<entt::id_type, Effekseer::EffectRef> effect_cache_;

public:
    bool init(int max_sprites = 8000);
    void loadEffect(entt::id_type id, const std::string& path);
    Effekseer::Handle play(entt::id_type effect_id, const glm::vec3& position, float scale = 1.0f);
    void stop(Effekseer::Handle handle);
    void setPosition(Effekseer::Handle handle, const glm::vec3& pos);
    void update(float delta_time);
    void render(const render::Camera& camera);
    void shutdown();
};
```

### ECS 集成

```cpp
struct ParticleComponent {
    entt::id_type effect_id{entt::null};
    Effekseer::Handle handle{-1};
    glm::vec2 offset{0.0f};
    float scale{1.0f};
    bool loop{false};
    bool auto_destroy{false};
};
```

ParticleSystem 负责启动/跟随/停止粒子实例。

### 渲染管线

```
ScenePass → LightingPass → EmissivePass → BloomPass
→ ParticlePass (新增) → CompositePass → UIPass
```

ParticlePass 调用前后保存/恢复 GL 状态，防止 Effekseer 破坏项目渲染状态。

### 2D 适配

- 正交投影，Z 轴固定
- 特效制作限制粒子运动在 XY 平面
- 游戏像素坐标 → Effekseer 世界坐标统一转换

### 资源结构

```
assets/effects/
├── battle/     # 技能特效
├── map/        # 环境特效
└── ui/         # UI 特效
```

### 验收标准

- Effekseer runtime 正常初始化，.efk 文件可加载
- 地图场景播放循环粒子（篝火），跟随实体
- 战斗场景技能释放播放一次性特效
- 粒子层级正确（场景之上、UI 之下）
- GL 状态不被破坏
- Lua 可通过 API 触发特效

---

## 实施顺序总览

```
Phase 0: 数据层去硬编码
    → Phase 1: Lua + Sol2 集成
        → Phase 2: 多层精灵渲染
            → Phase 3: RPG 战斗与游戏系统
                → Phase 4: Effekseer 粒子
```

每个 Phase 结束时游戏可正常运行并展示新能力。
