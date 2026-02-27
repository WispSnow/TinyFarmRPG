# RPG 数据格式设计与实现计划

## Context

项目正在从农场经营向日式RPG扩展。现有战斗系统极简（`BattleUnit` 仅有 hp/max_hp/attack/speed 4个属性），需要建立完整的RPG数据格式支持职业、技能、装备、敌人、状态效果等系统。参考 RPGMaker 的成熟数据架构，但采用项目自身风格（字符串ID、语义化JSON、Lua公式求值）。

**本次只做数据格式层（JSON schema + C++ 数据结构 + Catalog 加载）。不涉及战斗逻辑改动。**

---

## 文件概览

### 新建文件

| 文件 | 作用 |
|------|------|
| `src/game/data/rpg_types.h` | 共享枚举与类型别名（ParamIndex, Element, Scope, DamageType 等） |
| `src/game/data/rpg_types.cpp` | 枚举 string 转换实现 |
| `src/game/data/rpg_data.h` | 所有数据结构（ClassData, SkillData, WeaponData, ArmorData, EnemyData, EncounterData, StateData, TraitData, EffectData） |
| `src/game/data/rpg_catalog.h` | RpgCatalog 类声明 |
| `src/game/data/rpg_catalog.cpp` | RpgCatalog 加载实现 |
| `src/game/data/rpg_formula.h` | Lua 公式求值接口 |
| `src/game/data/rpg_formula.cpp` | Lua 公式求值实现 |
| `assets/data/class_config.json` | 职业定义 |
| `assets/data/skill_config.json` | 技能定义 |
| `assets/data/equipment_config.json` | 武器 + 防具定义 |
| `assets/data/enemy_config.json` | 敌人定义 |
| `assets/data/encounter_config.json` | 遭遇编组 |
| `assets/data/state_config.json` | 状态效果定义 |
| `tests/game/rpg_catalog_test.cpp` | Catalog 加载单元测试 |
| `tests/game/rpg_formula_test.cpp` | 公式求值单元测试 |

### 修改文件

| 文件 | 改动 |
|------|------|
| `src/game/data/item_catalog.h` | ItemCategory 增加 Equipment/BattleConsumable/KeyItem；ItemUseEffect 扩展战斗效果字段 |
| `src/game/data/item_catalog.cpp` | 解析新 category 和扩展 effect |
| `src/game/battle/battle_types.h` | BattleUnit 扩展为 8 属性体系；BattleActionType 增加 Skill/Item/Guard/Escape |
| `src/game/runtime/system_bundle.h` | GameRuntimeServices 增加 `shared_ptr<RpgCatalog>` |
| `src/game/runtime/game_runtime_assembler.cpp` | 增加 `ensureRpgCatalog()` |
| `CMakeLists.txt`（相关） | 新增 .cpp 源文件 |

---

## Step 1: rpg_types.h / .cpp — 共享枚举与类型

**文件**: `src/game/data/rpg_types.h`, `src/game/data/rpg_types.cpp`

定义所有 RPG 系统共用的枚举和类型别名：

```cpp
// 8 属性体系
enum class ParamIndex : int { Mhp, Mmp, Atk, Def, Mat, Mdf, Agi, Luk, Count };
constexpr int kParamCount = 8;
using ParamArray = std::array<int, kParamCount>;

// 扩展属性 (x-param): 命中/闪避/暴击 等
enum class XParam : int { Hit, Eva, Cri, Cev, Mev, Mrf, Cnt, Hrg, Mrg, Trg, Count };
constexpr int kXParamCount = 10;
using XParamArray = std::array<float, kXParamCount>;

// 元素
enum class Element : int { Normal, Fire, Ice, Thunder, Water, Earth, Wind, Light, Dark, Count };

// 技能/物品作用范围
enum class Scope : uint8_t { None, OneEnemy, AllEnemies, RandomEnemy, OneAlly, AllAllies, OneDeadAlly, AllDeadAllies, User };

// 伤害类型
enum class DamageType : uint8_t { None, HpDamage, MpDamage, HpRecover, MpRecover, HpDrain, MpDrain };

// 命中类型
enum class HitType : uint8_t { Certain, Physical, Magical };

// 状态行动限制
enum class Restriction : uint8_t { None, AttackEnemy, AttackAnyone, AttackAlly, NoneAction };

// 状态自动解除时机
enum class RemovalTiming : uint8_t { None, ActionEnd, TurnEnd };

// 使用场合
enum class Occasion : uint8_t { Always, BattleOnly, MenuOnly, Never };

// 装备槽位
enum class EquipSlot : uint8_t { Weapon, Shield, Head, Body, Accessory, Count };
```

每个枚举提供 `xxxFromString(string_view)` 和 `xxxToString()` 转换函数。

---

## Step 2: rpg_data.h — 所有数据结构

**文件**: `src/game/data/rpg_data.h`

将 Trait、Effect 和所有数据 struct 定义在同一个头文件中（结构不复杂，无需拆分多个文件）：

### Trait（被动修饰，附着在职业/装备/状态上）

```cpp
enum class TraitType : uint8_t {
    ElementRate,      // 元素抗性倍率
    ParamRate,        // 属性倍率 (用于 xparam: hit/eva/cri 等)
    AttackElement,    // 攻击附带元素
    AttackState,      // 攻击附带状态
    EquipWeaponType,  // 可装备武器类型
    EquipArmorType,   // 可装备防具类型
    StateRate,        // 状态抗性倍率
    StateImmunity,    // 状态免疫
    SpecialFlag,      // 特殊标记 (guard, auto_battle 等)
};

struct TraitData {
    TraitType type{};
    std::string key{};        // 元素名/属性名/武器类型/状态名 等
    entt::id_type target_id{};
    float value{0.0f};
};
```

### Effect（主动效果，技能/物品使用时触发）

```cpp
enum class EffectType : uint8_t {
    RecoverHp, RecoverMp, GainTp,
    AddState, RemoveState,
    AddBuff, AddDebuff, RemoveBuff, RemoveDebuff,
    GrowParam,        // 永久提升属性
    LearnSkill,
    Escape,
    AddItem,          // 兼容现有农场系统
};

struct EffectData {
    EffectType type{};
    entt::id_type target_id{};  // state_id / skill_id / item_id
    std::string param_key{};
    float value1{};             // rate / chance / turns
    float value2{};             // flat value
    int count{};                // AddItem 数量
};
```

### 技能

```cpp
struct DamageFormulaData {
    DamageType type{DamageType::None};
    Element element{Element::Normal};
    std::string formula{};     // Lua 表达式: "a.atk * 4 - b.def * 2"
    int variance{20};
    bool critical{false};
};

struct SkillData {
    entt::id_type id_{};
    std::string display_name_{};
    std::string description_{};
    std::string skill_type_{};   // "physical" / "magic" / "special"
    entt::id_type icon_id_{};
    int mp_cost_{}, tp_cost_{}, tp_gain_{};
    Scope scope_{Scope::None};
    HitType hit_type_{HitType::Certain};
    int success_rate_{100};
    int repeats_{1};
    int speed_bonus_{};
    Occasion occasion_{Occasion::Always};
    DamageFormulaData damage_{};
    std::vector<EffectData> effects_{};
    entt::id_type animation_id_{};
};
```

### 职业

```cpp
struct ClassLearning { int level; entt::id_type skill_id; };

struct ParamCurveData {
    ParamArray level_1{};
    ParamArray level_99{};
    std::string growth{"linear"};   // "linear" / "early" / "late"
};

struct ExpCurveData { int base{30}, extra{20}, acc_a{30}, acc_b{30}; };

struct ClassData {
    entt::id_type id_{};
    std::string display_name_{}, description_{};
    ExpCurveData exp_curve_{};
    ParamCurveData param_curves_{};
    std::vector<ClassLearning> learnings_{};
    std::vector<TraitData> traits_{};
};
```

### 装备

```cpp
struct WeaponData {
    entt::id_type id_{};
    std::string display_name_{}, description_{}, weapon_type_{};
    entt::id_type icon_id_{};
    int price_{};
    ParamArray params_{};
    std::vector<TraitData> traits_{};
    entt::id_type animation_id_{};
};

struct ArmorData {
    entt::id_type id_{};
    std::string display_name_{}, description_{}, armor_type_{};
    EquipSlot equip_slot_{EquipSlot::Body};
    entt::id_type icon_id_{};
    int price_{};
    ParamArray params_{};
    std::vector<TraitData> traits_{};
};
```

### 敌人

```cpp
struct EnemyActionCondition {
    std::string type{"always"};  // "always" / "hp_below" / "turn"
    float param_a{}, param_b{};
};

struct EnemyAction {
    entt::id_type skill_id{};
    int rating{5};
    EnemyActionCondition condition{};
};

struct EnemyDropItem {
    entt::id_type item_id{};
    float chance{};   // 0.0 ~ 1.0
};

struct EnemyData {
    entt::id_type id_{};
    std::string display_name_{}, battler_image_{};
    ParamArray params_{};
    int exp_reward_{}, gold_reward_{};
    std::vector<EnemyDropItem> drop_items_{};
    std::vector<EnemyAction> action_patterns_{};
    std::vector<TraitData> traits_{};
};
```

### 遭遇编组与状态

```cpp
struct EncounterMember { entt::id_type enemy_id{}; float x{}, y{}; };
struct EncounterData {
    entt::id_type id_{};
    std::string display_name_{};
    std::vector<EncounterMember> members_{};
};

struct StateRemovalData { RemovalTiming timing{RemovalTiming::None}; int min_turns{1}, max_turns{1}; };
struct StateMessages { std::string inflict{}, persist{}, remove{}; };

struct StateData {
    entt::id_type id_{};
    std::string display_name_{};
    entt::id_type icon_id_{};
    int priority_{50};
    Restriction restriction_{Restriction::None};
    StateRemovalData removal_{};
    bool remove_at_battle_end_{};
    bool remove_by_damage_{};
    StateMessages messages_{};
    std::vector<TraitData> traits_{};
};
```

---

## Step 3: rpg_catalog.h / .cpp — 统一数据目录

**文件**: `src/game/data/rpg_catalog.h`, `src/game/data/rpg_catalog.cpp`

遵循 `ItemCatalog` 模式（`loadJsonObjectFile` → 遍历数组 → `insert_or_assign`）：

```cpp
class RpgCatalog final {
public:
    [[nodiscard]] bool loadClassConfig(std::string_view path);
    [[nodiscard]] bool loadSkillConfig(std::string_view path);
    [[nodiscard]] bool loadEquipmentConfig(std::string_view path);  // weapons + armors
    [[nodiscard]] bool loadEnemyConfig(std::string_view path);
    [[nodiscard]] bool loadEncounterConfig(std::string_view path);
    [[nodiscard]] bool loadStateConfig(std::string_view path);

    [[nodiscard]] const ClassData*     findClass(entt::id_type id) const;
    [[nodiscard]] const SkillData*     findSkill(entt::id_type id) const;
    [[nodiscard]] const WeaponData*    findWeapon(entt::id_type id) const;
    [[nodiscard]] const ArmorData*     findArmor(entt::id_type id) const;
    [[nodiscard]] const EnemyData*     findEnemy(entt::id_type id) const;
    [[nodiscard]] const EncounterData* findEncounter(entt::id_type id) const;
    [[nodiscard]] const StateData*     findState(entt::id_type id) const;

private:
    std::unordered_map<entt::id_type, ClassData>     classes_{};
    std::unordered_map<entt::id_type, SkillData>     skills_{};
    std::unordered_map<entt::id_type, WeaponData>    weapons_{};
    std::unordered_map<entt::id_type, ArmorData>     armors_{};
    std::unordered_map<entt::id_type, EnemyData>     enemies_{};
    std::unordered_map<entt::id_type, EncounterData> encounters_{};
    std::unordered_map<entt::id_type, StateData>     states_{};
};
```

cpp 实现中用匿名 namespace 的辅助函数：`parseTraitList()`, `parseEffectList()`, `parseParamArray()` 等。所有 `loadXxx` 方法使用 `engine::utils::loadJsonObjectFile()` 加载。

---

## Step 4: rpg_formula.h / .cpp — Lua 伤害公式

**文件**: `src/game/data/rpg_formula.h`, `src/game/data/rpg_formula.cpp`

```cpp
struct FormulaContext {
    ParamArray params{};
    int level{1};
};

/// 用 Lua 求值伤害公式字符串，如 "a.atk * 4 - b.def * 2"
/// 返回 nullopt 表示公式执行出错
[[nodiscard]] std::optional<int> evaluateFormula(
    sol::state& lua,
    std::string_view formula,
    const FormulaContext& attacker,
    const FormulaContext& defender);
```

实现：构造 `a`/`b` 两个 Lua table（填充 mhp/mmp/atk/def/mat/mdf/agi/luk/level），包装公式为 `return function(a,b) return <formula> end`，调用并取返回值。

---

## Step 5: 6 个 JSON 数据文件

在 `assets/data/` 下创建，每个包含 2~3 条示例数据（翻译自 RPGMaker 参考数据），供测试和演示使用。JSON schema 见 Step 2 的结构对应。

示例（skill_config.json 节选）：
```json
{
  "skills": [
    {
      "id": "attack",
      "display_name": "攻击",
      "skill_type": "physical",
      "mp_cost": 0, "tp_gain": 5,
      "scope": "one_enemy",
      "hit_type": "physical",
      "damage": {
        "type": "hp_damage",
        "element": "normal",
        "formula": "a.atk * 4 - b.def * 2",
        "variance": 20,
        "critical": true
      },
      "effects": []
    },
    {
      "id": "heal_i",
      "display_name": "治疗I",
      "skill_type": "magic",
      "mp_cost": 12, "tp_gain": 5,
      "scope": "one_ally",
      "hit_type": "certain",
      "damage": {
        "type": "hp_recover",
        "formula": "500 + a.mat",
        "variance": 20
      },
      "effects": []
    }
  ]
}
```

---

## Step 6: 修改现有类型

### item_catalog.h — 扩展 ItemCategory 和 ItemUseEffect

```cpp
enum class ItemCategory {
    Tool, Crop, Seed, Material, Consumable,
    Equipment,          // 新增：装备类物品
    BattleConsumable,   // 新增：战斗消耗品（药水等）
    KeyItem,            // 新增：关键道具
    Unknown
};
```

`ItemData` 增加:
- `int price_{}`
- `std::optional<entt::id_type> equipment_id_{}` （Equipment 类关联 RpgCatalog 中的武器/防具 ID）

`ItemUseEffectType` 增加: `RecoverHp, RecoverMp, AddState, RemoveState, GrowParam`

`ItemUseEffect` 增加: `entt::id_type state_id{}; std::string param_key{}; float value1{}, value2{};`

`ItemUseConfig` 增加: `Scope scope{Scope::None};`

### battle_types.h — 扩展 BattleUnit

```cpp
struct BattleUnit {
    BattleUnitId id{};
    std::string name{};
    BattleSide side{BattleSide::Player};

    // 8 属性体系 (替换原有 hp/max_hp/attack/speed)
    game::data::ParamArray base_params{};
    game::data::ParamArray equip_bonus{};
    game::data::ParamArray buff_bonus{};
    int hp{}, mp{}, tp{};
    int level{1};

    // 引用
    entt::id_type class_id{};
    entt::id_type weapon_id{};
    std::array<entt::id_type, 5> armor_ids{};
    std::vector<entt::id_type> state_ids{};

    [[nodiscard]] int param(game::data::ParamIndex i) const;
    [[nodiscard]] int maxHp() const;
    [[nodiscard]] int maxMp() const;
    [[nodiscard]] bool isAlive() const { return hp > 0; }
};
```

`BattleActionType` 增加: `Skill, Item, Guard, Escape`

`BattleAction` 增加: `std::optional<entt::id_type> skill_id{}; std::optional<entt::id_type> item_id{};`

### system_bundle.h — 注册 RpgCatalog

`GameRuntimeServices` 增加: `std::shared_ptr<game::data::RpgCatalog> rpg_catalog;`

### game_runtime_assembler.cpp — 加载入口

增加 `ensureRpgCatalog()` 函数，在 `assembleServices()` 中调用（位于 `ensureVfxCatalog()` 之后）。6 个 JSON 分步加载，任何一个失败则 warn 但不阻塞（RPG 数据在非战斗场景非必须）。

---

## Step 7: 单元测试

### tests/game/rpg_catalog_test.cpp

- 为每种数据类型编写加载测试（创建临时 JSON → 加载 → 验证 find 返回正确数据）
- 测试 trait/effect 解析
- 测试 ID 不存在时 find 返回 nullptr

### tests/game/rpg_formula_test.cpp

- 基本公式: `"a.atk * 4 - b.def * 2"` → 验证数值
- 空公式: `""` → 返回 0
- 错误公式: `"invalid!!!"` → 返回 nullopt
- 复杂公式: `"math.max(a.mat * 3 - b.mdf, 0)"` → 验证 Lua math 库可用

---

## 实施顺序

1. `rpg_types.h` + `rpg_types.cpp` — 枚举基础
2. `rpg_data.h` — 数据结构定义（含 Trait/Effect）
3. `rpg_catalog.h` + `rpg_catalog.cpp` — Catalog 加载
4. 6 个 JSON 数据文件
5. `rpg_catalog_test.cpp` — 验证加载正确
6. `rpg_formula.h` + `rpg_formula.cpp` — 公式求值
7. `rpg_formula_test.cpp` — 验证公式
8. 修改 `item_catalog.h/cpp` — 扩展物品类型
9. 修改 `battle_types.h` — 扩展 BattleUnit
10. 修改 `system_bundle.h` + `game_runtime_assembler.cpp` — 集成
11. 更新 CMakeLists.txt
12. 编译验证 + 运行全量测试

## 验证

- `cmake --build build` 编译通过
- `ctest --test-dir build` 全量测试通过（含新增的 rpg_catalog_test 和 rpg_formula_test）
- 现有 battle_session_test 需同步更新 BattleUnit 构造
